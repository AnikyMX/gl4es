#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
#include <iostream>

// Include header dari Library Eksternal
#include "glslang/Public/ShaderLang.h"
#include "SPIRV/GlslangToSpv.h"
#include "spirv_cross/spirv_glsl.hpp"

// Include header lokal
#include "spirv_wrapper.h"
#include "../gl/debug.h"
#include "../gl/init.h" // Untuk akses globals4es

// --- BAGIAN 1: KONFIGURASI RESOURCE (WAJIB UNTUK GLSLANG) ---
// Ini mendefinisikan batas kemampuan GPU standar agar parser tidak bingung
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
    /* .MaxVertexUniformVectors = */ 128,
    /* .MaxVaryingVectors = */ 8,
    /* .MaxFragmentUniformVectors = */ 16,
    /* .MaxVertexOutputVectors = */ 16,
    /* .MaxFragmentInputVectors = */ 15,
    /* .MinProgramTexelOffset = */ -8,
    /* .MaxProgramTexelOffset = */ 7,
    /* .MaxClipDistances = */ 8,
    /* .MaxComputeWorkGroupCountX = */ 65535,
    /* .MaxComputeWorkGroupCountY = */ 65535,
    /* .MaxComputeWorkGroupCountZ = */ 65535,
    /* .MaxComputeWorkGroupSizeX = */ 1024,
    /* .MaxComputeWorkGroupSizeY = */ 1024,
    /* .MaxComputeWorkGroupSizeZ = */ 64,
    /* .MaxComputeUniformComponents = */ 1024,
    /* .MaxComputeTextureImageUnits = */ 16,
    /* .MaxComputeImageUniforms = */ 8,
    /* .MaxComputeAtomicCounters = */ 8,
    /* .MaxComputeAtomicCounterBuffers = */ 1,
    /* .MaxVaryingComponents = */ 60,
    /* .MaxVertexOutputComponents = */ 64,
    /* .MaxGeometryInputComponents = */ 64,
    /* .MaxGeometryOutputComponents = */ 128,
    /* .MaxFragmentInputComponents = */ 128,
    /* .MaxImageUnits = */ 8,
    /* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
    /* .MaxCombinedShaderOutputResources = */ 8,
    /* .MaxImageSamples = */ 0,
    /* .MaxVertexImageUniforms = */ 0,
    /* .MaxTessControlImageUniforms = */ 0,
    /* .MaxTessEvaluationImageUniforms = */ 0,
    /* .MaxGeometryImageUniforms = */ 0,
    /* .MaxFragmentImageUniforms = */ 8,
    /* .MaxCombinedImageUniforms = */ 8,
    /* .MaxGeometryTextureImageUnits = */ 16,
    /* .MaxGeometryOutputVertices = */ 256,
    /* .MaxGeometryTotalOutputComponents = */ 1024,
    /* .MaxGeometryUniformComponents = */ 1024,
    /* .MaxTessControlTextureImageUnits = */ 16,
    /* .MaxTessEvaluationTextureImageUnits = */ 16,
    /* .MaxTessControlUniformComponents = */ 1024,
    /* .MaxTessEvaluationUniformComponents = */ 1024,
    /* .MaxTessControlTotalOutputComponents = */ 4096,
    /* .MaxTessEvaluationOutputComponents = */ 128,
    /* .MaxTessGenLevel = */ 64,
    /* .MaxViewports = */ 16,
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

// Global flag untuk init sekali saja
static bool glslangInitialized = false;

extern "C" {

char* ConvertShaderSPIRV(const char* pEntry, int isVertex, shaderconv_need_t *need) {
    // 1. Inisialisasi glslang (Hanya sekali seumur hidup aplikasi)
    if (!glslangInitialized) {
        glslang::InitializeProcess();
        glslangInitialized = true;
        LOGD("[SPIR-V] Glslang Initialized.\n");
    }

    // 2. Tentukan Stage (Vertex atau Fragment)
    EShLanguage stage = isVertex ? EShLangVertex : EShLangFragment;

    // 3. Setup Shader Source
    glslang::TShader shader(stage);
    const char* shaderStrings[1];
    shaderStrings[0] = pEntry;
    shader.setStrings(shaderStrings, 1);

    // 4. Konfigurasi Environment (Pura-pura jadi Desktop OpenGL 4.5 biar fitur lengkap)
    // Minecraft pakai GLSL 1.20, glslang otomatis akan mendeteksi via #version 120
    int ClientInputSemanticsVersion = 100; // Default mapping
    glslang::EShTargetClientVersion ClientVersion = glslang::EShTargetOpenGL_450;
    glslang::EShTargetLanguageVersion TargetVersion = glslang::EShTargetSpv_1_0;

    shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientOpenGL, ClientInputSemanticsVersion);
    shader.setEnvClient(glslang::EShClientOpenGL, ClientVersion);
    shader.setEnvTarget(glslang::EShTargetSpv, TargetVersion);

    // 5. Parsing GLSL ke Intermediate AST
    EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules); // Mode Vulkan/SPIRV rules
    
    // String preprocessor (Macro definition)
    std::string preamble = "#define GL4ES 1\n"; 
    shader.setPreamble(preamble.c_str());

    if (!shader.parse(&DefaultTBuiltInResource, 100, false, messages)) {
        LOGD("[SPIR-V] Parsing Failed for %s shader!\n", isVertex ? "Vertex" : "Fragment");
        LOGD("[SPIR-V] InfoLog: %s\n", shader.getInfoLog());
        LOGD("[SPIR-V] DebugLog: %s\n", shader.getInfoDebugLog());
        return NULL; // Gagal parse, fallback ke gl4es shaderconv
    }

    // 6. Linking (Wajib untuk generate SPIR-V)
    glslang::TProgram program;
    program.addShader(&shader);

    if (!program.link(messages)) {
        LOGD("[SPIR-V] Linking Failed!\n");
        LOGD("[SPIR-V] InfoLog: %s\n", program.getInfoLog());
        return NULL;
    }

    // 7. Konversi ke SPIR-V Binary
    std::vector<unsigned int> spirv;
    glslang::GlslangToSpirv(*program.getIntermediate(stage), spirv);

    if (spirv.empty()) {
        LOGD("[SPIR-V] Generated SPIR-V is empty!\n");
        return NULL;
    }

    // 8. Cross-Compile: SPIR-V -> GLES 3.0 Source Code
    try {
        spirv_cross::CompilerGLSL glsl(spirv);

        // Opsi Kompilasi agar sesuai dengan PowerVR/Android
        spirv_cross::CompilerGLSL::Options options;
        options.version = 300; // GLES 3.0 (Sesuai dengan log kamu: Using GLES 3.0 backend)
        options.es = true;     // Mode ES (Embedded Systems)
        options.vulkan_semantics = false; 
        options.emit_uniform_buffer_as_plain_uniforms = true; // Minecraft lama gak pake UBO
        options.emit_push_constant_as_uniform_buffer = false; 
        
        // Atur presisi default ke highp agar aman
        options.fragment.default_float_precision = spirv_cross::CompilerGLSL::Options::Precision::Highp;
        options.fragment.default_int_precision = spirv_cross::CompilerGLSL::Options::Precision::Highp;

        glsl.set_common_options(options);

        // Generate Source Code String
        std::string source = glsl.compile();

        // 9. Kembalikan string ke C (gl4es)
        // Kita pakai strdup karena C perlu pointer yang bisa di-free()
        // LOGD("[SPIR-V] Success converting %s shader! (Size: %d)\n", isVertex ? "Vertex" : "Fragment", source.length());
        return strdup(source.c_str());

    } catch (const std::exception& e) {
        LOGD("[SPIR-V] SPIRV-Cross Exception: %s\n", e.what());
        return NULL;
    }
}

}