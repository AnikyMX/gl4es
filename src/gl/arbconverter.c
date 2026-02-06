#include "arbconverter.h"

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "arbgenerator.h"
#include "arbhelper.h"
#include "arbparser.h"
#include "khash.h"

// Optimization macros for Branch Prediction (Helio P35 benefit)
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

#define FAIL(str) \
    curStatus.status = ST_ERROR; \
    if (*error_msg) free(*error_msg); \
    *error_msg = strdup(str); \
    continue

char* gl4es_convertARB(const char* restrict code, int vertex, char ** restrict error_msg, int * restrict error_ptr) {
    *error_ptr = -1; // Reinit error pointer
    
    struct sSpecialCases specialCases = {0, 0};
    const char *codeStart = code;

    // --- OPTIMIZATION 1: Fast Header Detection ---
    // Instead of manual byte looping, use strstr for SIMD-optimized search
    const char *headerTarget = (vertex) ? "!!ARBvp1.0" : "!!ARBfp1.0";
    const char *foundHeader = strstr(code, headerTarget);

    if (UNLIKELY(!foundHeader)) {
        // Fallback: Check if it's a valid ARB start but wrong type (for error reporting)
        if (strstr(code, "!!ARB")) {
             // It's an ARB shader, just not the one we asked for (mismatch)
            if (*error_msg) free(*error_msg);
            *error_msg = strdup("Invalid program start (Type Mismatch)");
            *error_ptr = 0;
            return NULL;
        }
        // Completely invalid
        if (*error_msg) free(*error_msg);
        *error_msg = strdup("Invalid program start (No ARB Header)");
        *error_ptr = 0;
        return NULL;
    }

    codeStart = foundHeader + 10; // Skip "!!ARBxp1.0"

    // Initialize Status
    sCurStatus curStatus = {0};
    initStatus(&curStatus, codeStart);
    
    // Prime the parser
    readNextToken(&curStatus);
    if (UNLIKELY((curStatus.curToken != TOK_NEWLINE) && (curStatus.curToken != TOK_WHITESPACE))) {
        curStatus.status = ST_ERROR;
    } else {
        readNextToken(&curStatus);
    }
    
    // --- MAIN PARSING LOOP ---
    while ((curStatus.status != ST_ERROR) && (curStatus.status != ST_DONE)) {
        // Note: massive debug printf blocks removed for performance/cache locality
        parseToken(&curStatus, vertex, error_msg, &specialCases);
        readNextToken(&curStatus);
    }
    
    // Check for parsing errors
    if (UNLIKELY(curStatus.status == ST_ERROR)) {
        *error_ptr = curStatus.codePtr - code;
        freeStatus(&curStatus);
        if (curStatus.outputString) free(curStatus.outputString);
        return NULL;
    }
    
    // --- GENERATION PHASE ---
    // Variables are automatically created, only need to write main()
    size_t varIdx = 0;
    sVariable *varPtr;
    size_t instIdx = 0;
    sInstruction *instPtr;
    
    // Wrapped in do-while(0) for centralized error breaking, standard C pattern
    do {
        // 1. Header Injection
        if (vertex) {
            // Vertex Shader: Add structure for address reg
            APPEND_OUTPUT("#version 120\n\nstruct _structOnlyX { int x; };\n\nvoid main() {\n", 61)
            if (specialCases.hasFogFragCoord) {
                APPEND_OUTPUT("\tvec4 gl4es_FogFragCoordTemp = vec4(gl_FogFragCoord);\n", 54)
            }
        } else {
            // Fragment Shader
            APPEND_OUTPUT("#version 120\n\nvoid main() {\n", 28)
            if (specialCases.isDepthReplacing) {
                APPEND_OUTPUT("\tvec4 gl4es_FragDepthTemp = vec4(gl_FragDepth);\n", 48)
            }
        }
        
        // 2. Variable Declarations (Pre-Main Logic)
        for (; (varIdx < curStatus.variables.size) && (curStatus.status != ST_ERROR); ++varIdx) {
            varPtr = curStatus.variables.vars[varIdx];
            generateVariablePre(&curStatus, vertex, error_msg, varPtr);
        }
        if (UNLIKELY(curStatus.status == ST_ERROR)) {
            --varIdx;
            *error_ptr = 1; // Phase 1 Error
            break;
        }
        
        APPEND_OUTPUT("\t\n", 2)

        // 3. Instruction Generation (The Heavy Lifting)
        for (; (instIdx < curStatus.instructions.size) && (curStatus.status != ST_ERROR); ++instIdx) {
            instPtr = curStatus.instructions.insts[instIdx];
            generateInstruction(&curStatus, vertex, error_msg, instPtr);
        }
        if (UNLIKELY(curStatus.status == ST_ERROR)) {
            // Pointer arithmetic to find offset of failed instruction
            if(instIdx < curStatus.instructions.size)
                 *error_ptr = curStatus.instructions.insts[instIdx]->codeLocation - code;
            else 
                 *error_ptr = 0;
            break;
        }
        
        APPEND_OUTPUT("\t\n", 2)

        // 4. Variable Finalization (Post-Processing)
        for (varIdx = 0; (varIdx < curStatus.variables.size) && (curStatus.status != ST_ERROR); ++varIdx) {
            varPtr = curStatus.variables.vars[varIdx];
            generateVariablePst(&curStatus, vertex, error_msg, varPtr);
        }
        if (UNLIKELY(curStatus.status == ST_ERROR)) {
            --varIdx;
            *error_ptr = 2; // Phase 2 Error
            break;
        }
        
        // 5. Special Features Injection (Fog & Depth)
        if (specialCases.hasFogFragCoord) {
            APPEND_OUTPUT("\tgl_FogFragCoord = gl4es_FogFragCoordTemp.x;\n", 45)
        }
        if (specialCases.isDepthReplacing) {
            APPEND_OUTPUT("\tgl_FragDepth = gl4es_FragDepthTemp.z;\n", 39)
        }

        // Optimized Fog Logic Switch
        switch (curStatus.fogType) {
            case FOG_NONE:
                break; // Do nothing, fastest path
            case FOG_EXP:
                APPEND_OUTPUT(
                    "\tgl_FragColor.rgb = mix(gl_Fog.color.rgb, gl_FragColor.rgb, "
                    "clamp(exp(-gl_Fog.density * gl_FogFragCoord), 0.0, 1.0));\n",
                    118 // Length adjusted for 0.0/1.0 precision
                )
                break;
            case FOG_EXP2:
                APPEND_OUTPUT(
                    "\tgl_FragColor.rgb = mix(gl_Fog.color.rgb, gl_FragColor.rgb, "
                    "clamp(exp(-(gl_Fog.density * gl_FogFragCoord)*(gl_Fog.density * gl_FogFragCoord)), 0.0, 1.0));\n",
                    157
                )
                break;
            case FOG_LINEAR:
                APPEND_OUTPUT(
                    "\tgl_FragColor.rgb = mix(gl_Fog.color.rgb, gl_FragColor.rgb, "
                    "clamp((gl_Fog.end - gl_FogFragCoord) * gl_Fog.scale, 0.0, 1.0));\n",
                    127
                )
                break;
        }

        if(curStatus.position_invariant) {
            APPEND_OUTPUT("\tgl_Position = ftransform();\n", 29)
        }
        
        APPEND_OUTPUT("}\n", 2)

    } while (0);
    
    // --- FINAL CLEANUP ---
    
    if (UNLIKELY(curStatus.status == ST_ERROR)) {
        if (*error_ptr == -1) {
            if (*error_msg) free(*error_msg);
            *error_msg = strdup("Generic Conversion Error (OOM?)");
            *error_ptr = 0;
        }
        
        freeStatus(&curStatus);
        if (curStatus.outputString) free(curStatus.outputString);
        return NULL;
    }
    
    freeStatus(&curStatus);
    // Return the successfully generated GLSL string
    return curStatus.outputString;
}