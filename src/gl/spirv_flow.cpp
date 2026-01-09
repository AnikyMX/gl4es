#include "spirv_flow.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <iostream>

// Include library glslang (Frontend)
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>

// Include library SPIRV-Cross (Backend)
#include <spirv_cross.hpp>
#include <spirv_glsl.hpp>

// Setup resource limits (Standar OpenGL)
// Ini diperlukan glslang agar tahu batasan parsing
const TBuiltInResource DefaultTBuiltInResource = {
    /* .MaxLights = */ 32,
    /* .MaxClipPlanes = */ 6,
    /* .MaxTextureUnits = */ 32,
    /* .MaxTextureCoords = */ 32,
    /* .MaxVertexAttribs = */ 64,
    /* .MaxVertexUniformComponents = */ 4096,
    /* .MaxVaryingFloats = */ 64,
    /* .MaxVertexTextureImageUnits = */ 32,
    /* .MaxCombinedTextureImageUnits = */ 80,
    /* .MaxTextureImageUnits = */ 32,
    /* .MaxFragmentUniformComponents = */ 4096,
    /* .MaxDrawBuffers = */ 32,
    /* .MaxVertexAtomicCounters = */ 0,
    /* .MaxTessControlAtomicCounters = */ 0,
    /* .MaxTessEvaluationAtomicCounters = */ 0,
    /* .MaxGeometryAtomicCounters = */ 0,
    /* .MaxFragmentAtomicCounters = */ 8,
    /* .MaxCombinedAtomicCounters = */ 8,
    /* .MaxAtomicCounterBindings = */ 1,
    /* .MaxVertexAtomicCounterBuffers = */ 0,
    /* .MaxTessControlAtomicCounterBuffers = */ 0,
    /* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
    /* .MaxGeometryAtomicCounterBuffers = */ 0,
    /* .MaxFragmentAtomicCounterBuffers = */ 1,
    /* .MaxCombinedAtomicCounterBuffers = */ 1,
    /* .MaxAtomicCounterBufferSize = */ 16384,
    /* .MaxTransformFeedbackBuffers = */ 4,
    /* .MaxTransformFeedbackInterleavedComponents = */ 64,
    /* .MaxCullDistances = */ 8,
    /* .MaxCombinedClipAndCullDistances = */ 8,
    /* .MaxSamples = */ 4,
    /* .maxMeshOutputVerticesNV = */ 256,
    /* .maxMeshOutputPrimitivesNV = */ 512,
    /* .maxMeshWorkGroupSizeX_NV = */ 32,
    /* .maxMeshWorkGroupSizeY_NV = */ 1,
    /* .maxMeshWorkGroupSizeZ_NV = */ 1,
    /* .maxTaskWorkGroupSizeX_NV = */ 32,
    /* .maxTaskWorkGroupSizeY_NV = */ 1,
    /* .maxTaskWorkGroupSizeZ_NV = */ 1,
    /* .maxMeshViewCountNV = */ 4,
    /* .limits = */ {
        /* .nonInductiveForLoops = */ 1,
        /* .whileLoops = */ 1,
        /* .doWhileLoops = */ 1,
        /* .generalUniformIndexing = */ 1,
        /* .generalAttributeMatrixVectorIndexing = */ 1,
        /* .generalVaryingIndexing = */ 1,
        /* .generalSamplerIndexing = */ 1,
        /* .generalVariableIndexing = */ 1,
        /* .generalConstantMatrixVectorIndexing = */ 1,
    }
};

extern "C" {

// Helper: Inisialisasi proses glslang (hanya sekali)
static int is_initialized = 0;
void ensure_init() {
    if (!is_initialized) {
        glslang::InitializeProcess();
        is_initialized = 1;
    }
}

// FUNGSI UTAMA KITA
char* spirv_try_convert(const char* source, GLenum shaderType) {
    ensure_init();

    // 1. Tentukan tipe shader (Vertex atau Fragment)
    EShLanguage stage;
    if (shaderType == GL_VERTEX_SHADER) stage = EShLangVertex;
    else if (shaderType == GL_FRAGMENT_SHADER) stage = EShLangFragment;
    else return NULL; // Tipe tidak dikenal, fallback ke legacy

    // 2. Setup Shader Parser
    glslang::TShader shader(stage);
    const char* shaderStrings[1];
    shaderStrings[0] = source;
    shader.setStrings(shaderStrings, 1);

    // Set Environment Target ke OpenGL Client
    // Kita targetkan Vulkan SPIR-V 1.0 (Standar paling kompatibel)
    shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientOpenGL, 100);
    shader.setEnvClient(glslang::EShClientOpenGL, glslang::EShTargetOpenGL_450);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);

    // 3. Parse (Compile ke AST)
    // EShMsgDefault berarti kita ingin pesan error standar jika gagal
    if (!shader.parse(&DefaultTBuiltInResource, 100, false, EShMsgDefault)) {
        // Jika parsing gagal, kita print errornya ke Logcat tapi JANGAN CRASH.
        // Return NULL agar gl4es pakai cara lama.
        printf("LIBGL: [SPIRV-Flow] Parse Failed:\n%s\n%s\n", shader.getInfoLog(), shader.getInfoDebugLog());
        return NULL;
    }

    // 4. Link ke Program (Diperlukan untuk generate SPIR-V yang valid)
    glslang::TProgram program;
    program.addShader(&shader);
    if (!program.link(EShMsgDefault)) {
        printf("LIBGL: [SPIRV-Flow] Link Failed:\n%s\n", program.getInfoLog());
        return NULL;
    }

    // 5. Generate Binary SPIR-V
    std::vector<unsigned int> spirv_binary;
    glslang::GlslangToSpv(*program.getIntermediate(stage), spirv_binary);

    if (spirv_binary.empty()) return NULL;

    // --- BATAS SUCI: Masuk ke wilayah SPIRV-Cross ---

    try {
        // 6. Baca SPIR-V
        spirv_cross::CompilerGLSL compiler(spirv_binary);

        // 7. Konfigurasi Output untuk PowerVR (GLES 3.0)
        spirv_cross::CompilerGLSL::Options options;
        options.version = 300; // Target GLES 3.0
        options.es = true;     // Mode Embedded Systems
        
        // Tweaks khusus PowerVR agar tidak ngelag
        options.enable_420pack_extension = false; 
        options.vertex.fixup_clipspace = true; // Fix koordinat z
        
        compiler.set_common_options(options);

        // 8. Compile balik ke String GLSL
        std::string clean_source = compiler.compile();

        // 9. Kirim hasilnya ke C (gl4es)
        // Kita harus pakai strdup (malloc) karena gl4es akan mem-free-nya nanti
        return strdup(clean_source.c_str());

    } catch (const std::exception& e) {
        printf("LIBGL: [SPIRV-Cross] Exception: %s\n", e.what());
        return NULL; // Fallback jika SPIRV-Cross gagal
    }
}

}