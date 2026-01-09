#include "spirv_flow.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <iostream>

// Include Header
#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>

// Include Backend
#include <spirv_cross.hpp>
#include <spirv_glsl.hpp>

extern "C" {

// Helper: Setup Resource Limits secara manual (Anti-Error Version)
void SetupDefaultResources(TBuiltInResource* res) {
    // 1. Bersihkan memory (Zero out)
    memset(res, 0, sizeof(TBuiltInResource));

    // 2. Isi nilai standar OpenGL ES 3.0 / OpenGL 4.5
    res->maxLights = 32;
    res->maxClipPlanes = 6;
    res->maxTextureUnits = 32;
    res->maxTextureCoords = 32;
    res->maxVertexAttribs = 64;
    res->maxVertexUniformComponents = 4096;
    res->maxVaryingFloats = 64;
    res->maxVertexTextureImageUnits = 32;
    res->maxCombinedTextureImageUnits = 80;
    res->maxTextureImageUnits = 32;
    res->maxFragmentUniformComponents = 4096;
    res->maxDrawBuffers = 32;
    res->maxVertexAtomicCounters = 8;
    res->maxTessControlAtomicCounters = 8;
    res->maxTessEvaluationAtomicCounters = 8;
    res->maxGeometryAtomicCounters = 8;
    res->maxFragmentAtomicCounters = 8;
    res->maxCombinedAtomicCounters = 8;
    res->maxAtomicCounterBindings = 1;
    res->maxVertexAtomicCounterBuffers = 1;
    res->maxTessControlAtomicCounterBuffers = 1;
    res->maxTessEvaluationAtomicCounterBuffers = 1;
    res->maxGeometryAtomicCounterBuffers = 1;
    res->maxFragmentAtomicCounterBuffers = 1;
    res->maxCombinedAtomicCounterBuffers = 1;
    res->maxAtomicCounterBufferSize = 16384;
    res->maxTransformFeedbackBuffers = 4;
    res->maxTransformFeedbackInterleavedComponents = 64;
    res->maxCullDistances = 8;
    res->maxCombinedClipAndCullDistances = 8;
    res->maxSamples = 4;
    
    // Limits (Bagian yang tadi error, sekarang aman)
    res->limits.nonInductiveForLoops = 1;
    res->limits.whileLoops = 1;
    res->limits.doWhileLoops = 1;
    res->limits.generalUniformIndexing = 1;
    res->limits.generalAttributeMatrixVectorIndexing = 1;
    res->limits.generalVaryingIndexing = 1;
    res->limits.generalSamplerIndexing = 1;
    res->limits.generalVariableIndexing = 1;
    res->limits.generalConstantMatrixVectorIndexing = 1;
}

static int is_initialized = 0;
void ensure_init() {
    if (!is_initialized) {
        glslang::InitializeProcess();
        is_initialized = 1;
    }
}

char* spirv_try_convert(const char* source, GLenum shaderType) {
    ensure_init();

    EShLanguage stage;
    if (shaderType == GL_VERTEX_SHADER) stage = EShLangVertex;
    else if (shaderType == GL_FRAGMENT_SHADER) stage = EShLangFragment;
    else return NULL;

    // --- SETUP RESOURCES ---
    TBuiltInResource Resources;
    SetupDefaultResources(&Resources);
    // -----------------------

    glslang::TShader shader(stage);
    const char* shaderStrings[1];
    shaderStrings[0] = source;
    shader.setStrings(shaderStrings, 1);

    shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientOpenGL, 100);
    shader.setEnvClient(glslang::EShClientOpenGL, glslang::EShTargetOpenGL_450);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);

    // Pass variable 'Resources' bukan constant
    if (!shader.parse(&Resources, 100, false, EShMsgDefault)) {
        printf("LIBGL: [SPIRV-Flow] Parse Failed:\n%s\n%s\n", shader.getInfoLog(), shader.getInfoDebugLog());
        return NULL;
    }

    glslang::TProgram program;
    program.addShader(&shader);
    if (!program.link(EShMsgDefault)) {
        printf("LIBGL: [SPIRV-Flow] Link Failed:\n%s\n", program.getInfoLog());
        return NULL;
    }

    std::vector<unsigned int> spirv_binary;
    glslang::GlslangToSpv(*program.getIntermediate(stage), spirv_binary);

    if (spirv_binary.empty()) return NULL;

    try {
        spirv_cross::CompilerGLSL compiler(spirv_binary);

        spirv_cross::CompilerGLSL::Options options;
        options.version = 300;
        options.es = true;
        options.enable_420pack_extension = false; 
        options.vertex.fixup_clipspace = true;
        
        compiler.set_common_options(options);

        std::string clean_source = compiler.compile();
        return strdup(clean_source.c_str());

    } catch (const std::exception& e) {
        printf("LIBGL: [SPIRV-Cross] Exception: %s\n", e.what());
        return NULL;
    }
}

}