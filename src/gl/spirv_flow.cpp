#include "spirv_flow.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <iostream>

// --- PERBAIKAN HEADER ADA DI SINI ---
#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>  // <--- Hapus "glslang/" agar path benar
// ------------------------------------

// Include library SPIRV-Cross (Backend)
#include <spirv_cross.hpp>
#include <spirv_glsl.hpp>

// Setup resource limits (Standar OpenGL)
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

    glslang::TShader shader(stage);
    const char* shaderStrings[1];
    shaderStrings[0] = source;
    shader.setStrings(shaderStrings, 1);

    shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientOpenGL, 100);
    shader.setEnvClient(glslang::EShClientOpenGL, glslang::EShTargetOpenGL_450);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);

    if (!shader.parse(&DefaultTBuiltInResource, 100, false, EShMsgDefault)) {
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