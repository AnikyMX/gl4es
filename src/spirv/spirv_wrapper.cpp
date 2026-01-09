#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
#include <iostream>

// Include Android Log untuk debug yang pasti jalan
#include <android/log.h>
#define LOG_TAG "GL4ES_SPIRV"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

// Include header dari Library Eksternal
#include "glslang/Public/ShaderLang.h"
#include "SPIRV/GlslangToSpv.h"
#include "spirv_glsl.hpp"

// Include header lokal
#include "spirv_wrapper.h"

// Global Resource struct
TBuiltInResource DefaultTBuiltInResource;

// Fungsi untuk mengisi resource limit secara manual (Anti-Error Initializer)
void InitDefaultResources() {
    DefaultTBuiltInResource.maxLights = 32;
    DefaultTBuiltInResource.maxClipPlanes = 6;
    DefaultTBuiltInResource.maxTextureUnits = 32;
    DefaultTBuiltInResource.maxTextureCoords = 32;
    DefaultTBuiltInResource.maxVertexAttribs = 64;
    DefaultTBuiltInResource.maxVertexUniformComponents = 4096;
    DefaultTBuiltInResource.maxVaryingFloats = 64;
    DefaultTBuiltInResource.maxVertexTextureImageUnits = 32;
    DefaultTBuiltInResource.maxCombinedTextureImageUnits = 80;
    DefaultTBuiltInResource.maxTextureImageUnits = 32;
    DefaultTBuiltInResource.maxFragmentUniformComponents = 4096;
    DefaultTBuiltInResource.maxDrawBuffers = 32;
    DefaultTBuiltInResource.maxVertexUniformVectors = 128;
    DefaultTBuiltInResource.maxVaryingVectors = 8;
    DefaultTBuiltInResource.maxFragmentUniformVectors = 16;
    DefaultTBuiltInResource.maxVertexOutputVectors = 16;
    DefaultTBuiltInResource.maxFragmentInputVectors = 15;
    DefaultTBuiltInResource.minProgramTexelOffset = -8;
    DefaultTBuiltInResource.maxProgramTexelOffset = 7;
    DefaultTBuiltInResource.maxClipDistances = 8;
    DefaultTBuiltInResource.maxComputeWorkGroupCountX = 65535;
    DefaultTBuiltInResource.maxComputeWorkGroupCountY = 65535;
    DefaultTBuiltInResource.maxComputeWorkGroupCountZ = 65535;
    DefaultTBuiltInResource.maxComputeWorkGroupSizeX = 1024;
    DefaultTBuiltInResource.maxComputeWorkGroupSizeY = 1024;
    DefaultTBuiltInResource.maxComputeWorkGroupSizeZ = 64;
    DefaultTBuiltInResource.maxComputeUniformComponents = 1024;
    DefaultTBuiltInResource.maxComputeTextureImageUnits = 16;
    DefaultTBuiltInResource.maxComputeImageUniforms = 8;
    DefaultTBuiltInResource.maxComputeAtomicCounters = 8;
    DefaultTBuiltInResource.maxComputeAtomicCounterBuffers = 1;
    DefaultTBuiltInResource.maxVaryingComponents = 60;
    DefaultTBuiltInResource.maxVertexOutputComponents = 64;
    DefaultTBuiltInResource.maxGeometryInputComponents = 64;
    DefaultTBuiltInResource.maxGeometryOutputComponents = 128;
    DefaultTBuiltInResource.maxFragmentInputComponents = 128;
    DefaultTBuiltInResource.maxImageUnits = 8;
    DefaultTBuiltInResource.maxCombinedImageUnitsAndFragmentOutputs = 8;
    DefaultTBuiltInResource.maxCombinedShaderOutputResources = 8;
    DefaultTBuiltInResource.maxImageSamples = 0;
    DefaultTBuiltInResource.maxVertexImageUniforms = 0;
    DefaultTBuiltInResource.maxTessControlImageUniforms = 0;
    DefaultTBuiltInResource.maxTessEvaluationImageUniforms = 0;
    DefaultTBuiltInResource.maxGeometryImageUniforms = 0;
    DefaultTBuiltInResource.maxFragmentImageUniforms = 8;
    DefaultTBuiltInResource.maxCombinedImageUniforms = 8;
    DefaultTBuiltInResource.maxGeometryTextureImageUnits = 16;
    DefaultTBuiltInResource.maxGeometryOutputVertices = 256;
    DefaultTBuiltInResource.maxGeometryTotalOutputComponents = 1024;
    DefaultTBuiltInResource.maxGeometryUniformComponents = 1024;
    DefaultTBuiltInResource.maxTessControlTextureImageUnits = 16;
    DefaultTBuiltInResource.maxTessEvaluationTextureImageUnits = 16;
    DefaultTBuiltInResource.maxTessControlUniformComponents = 1024;
    DefaultTBuiltInResource.maxTessEvaluationUniformComponents = 1024;
    DefaultTBuiltInResource.maxTessControlTotalOutputComponents = 4096;
    DefaultTBuiltInResource.maxTessEvaluationOutputComponents = 128;
    DefaultTBuiltInResource.maxTessGenLevel = 64;
    DefaultTBuiltInResource.maxViewports = 16;
    DefaultTBuiltInResource.maxVertexAtomicCounters = 0;
    DefaultTBuiltInResource.maxTessControlAtomicCounters = 0;
    DefaultTBuiltInResource.maxTessEvaluationAtomicCounters = 0;
    DefaultTBuiltInResource.maxGeometryAtomicCounters = 0;
    DefaultTBuiltInResource.maxFragmentAtomicCounters = 8;
    DefaultTBuiltInResource.maxCombinedAtomicCounters = 8;
    DefaultTBuiltInResource.maxAtomicCounterBindings = 1;
    DefaultTBuiltInResource.maxVertexAtomicCounterBuffers = 0;
    DefaultTBuiltInResource.maxTessControlAtomicCounterBuffers = 0;
    DefaultTBuiltInResource.maxTessEvaluationAtomicCounterBuffers = 0;
    DefaultTBuiltInResource.maxGeometryAtomicCounterBuffers = 0;
    DefaultTBuiltInResource.maxFragmentAtomicCounterBuffers = 1;
    DefaultTBuiltInResource.maxCombinedAtomicCounterBuffers = 1;
    DefaultTBuiltInResource.maxAtomicCounterBufferSize = 16384;
    DefaultTBuiltInResource.maxTransformFeedbackBuffers = 4;
    DefaultTBuiltInResource.maxTransformFeedbackInterleavedComponents = 64;
    DefaultTBuiltInResource.maxCullDistances = 8;
    DefaultTBuiltInResource.maxCombinedClipAndCullDistances = 8;
    DefaultTBuiltInResource.maxSamples = 4;
    DefaultTBuiltInResource.maxMeshOutputVerticesNV = 256;
    DefaultTBuiltInResource.maxMeshOutputPrimitivesNV = 512;
    DefaultTBuiltInResource.maxMeshWorkGroupSizeX_NV = 32;
    DefaultTBuiltInResource.maxMeshWorkGroupSizeY_NV = 1;
    DefaultTBuiltInResource.maxMeshWorkGroupSizeZ_NV = 1;
    DefaultTBuiltInResource.maxTaskWorkGroupSizeX_NV = 32;
    DefaultTBuiltInResource.maxTaskWorkGroupSizeY_NV = 1;
    DefaultTBuiltInResource.maxTaskWorkGroupSizeZ_NV = 1;
    DefaultTBuiltInResource.maxMeshViewCountNV = 4;
    
    // Limits struct assignment
    DefaultTBuiltInResource.limits.nonInductiveForLoops = 1;
    DefaultTBuiltInResource.limits.whileLoops = 1;
    DefaultTBuiltInResource.limits.doWhileLoops = 1;
    DefaultTBuiltInResource.limits.generalUniformIndexing = 1;
    DefaultTBuiltInResource.limits.generalAttributeMatrixVectorIndexing = 1;
    DefaultTBuiltInResource.limits.generalVaryingIndexing = 1;
    DefaultTBuiltInResource.limits.generalSamplerIndexing = 1;
    DefaultTBuiltInResource.limits.generalVariableIndexing = 1;
    DefaultTBuiltInResource.limits.generalConstantMatrixVectorIndexing = 1;
}

static bool glslangInitialized = false;

extern "C" {

__attribute__((visibility("default"))) 
char* ConvertShaderSPIRV(const char* pEntry, int isVertex, shaderconv_need_t *need) {
    if (!glslangInitialized) {
        glslang::InitializeProcess();
        InitDefaultResources(); // Panggil fungsi inisialisasi manual
        glslangInitialized = true;
        LOGD("Glslang Initialized.");
    }

    EShLanguage stage = isVertex ? EShLangVertex : EShLangFragment;

    glslang::TShader shader(stage);
    const char* shaderStrings[1];
    shaderStrings[0] = pEntry;
    shader.setStrings(shaderStrings, 1);

    int ClientInputSemanticsVersion = 120; 
    glslang::EShTargetClientVersion ClientVersion = glslang::EShTargetOpenGL_450;
    glslang::EShTargetLanguageVersion TargetVersion = glslang::EShTargetSpv_1_0;

    shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientOpenGL, ClientInputSemanticsVersion);
    shader.setEnvClient(glslang::EShClientOpenGL, ClientVersion);
    shader.setEnvTarget(glslang::EShTargetSpv, TargetVersion);

    EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules); 
    
    std::string preamble = "#define GL4ES 1\n"; 
    shader.setPreamble(preamble.c_str());

    // Fix: Menggunakan InitDefaultResources yang sudah kita buat
    if (!shader.parse(&DefaultTBuiltInResource, 100, false, messages)) {
        LOGD("Parsing Failed for %s shader!", isVertex ? "Vertex" : "Fragment");
        LOGD("InfoLog: %s", shader.getInfoLog());
        LOGD("DebugLog: %s", shader.getInfoDebugLog());
        return NULL; 
    }

    glslang::TProgram program;
    program.addShader(&shader);

    if (!program.link(messages)) {
        LOGD("Linking Failed!");
        LOGD("InfoLog: %s", program.getInfoLog());
        return NULL;
    }

    std::vector<unsigned int> spirv;
    // Fix: Menggunakan GlslangToSpv (bukan GlslangToSpirv)
    glslang::GlslangToSpv(*program.getIntermediate(stage), spirv);

    if (spirv.empty()) {
        LOGD("Generated SPIR-V is empty!");
        return NULL;
    }

    try {
        spirv_cross::CompilerGLSL glsl(spirv);

        spirv_cross::CompilerGLSL::Options options;
        options.version = 300; 
        options.es = true;     
        options.vulkan_semantics = false; 
        options.emit_uniform_buffer_as_plain_uniforms = true; 
        options.emit_push_constant_as_uniform_buffer = false; 
        
        options.fragment.default_float_precision = spirv_cross::CompilerGLSL::Options::Precision::Highp;
        options.fragment.default_int_precision = spirv_cross::CompilerGLSL::Options::Precision::Highp;

        glsl.set_common_options(options);

        std::string source = glsl.compile();
        return strdup(source.c_str());

    } catch (const std::exception& e) {
        LOGD("SPIRV-Cross Exception: %s", e.what());
        return NULL;
    }
}

}