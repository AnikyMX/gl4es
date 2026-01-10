#ifndef GLSL_OPTIMIZER_H
#define GLSL_OPTIMIZER_H

/*
  [INJEKSI] C COMPATIBILITY BLOCK
  Bagian ini ditambahkan agar header ini bisa dibaca oleh shaderconv.c (Bahasa C)
  tanpa menyebabkan error "must use struct/enum tag" atau "unknown type bool".
*/
#ifndef __cplusplus
    #include <stdbool.h>
    /* Definisi Type agar C bisa memanggil tanpa keyword struct/enum */
    typedef struct glslopt_shader glslopt_shader;
    typedef struct glslopt_ctx glslopt_ctx;
    typedef enum glslopt_shader_type glslopt_shader_type;
    typedef enum glslopt_options glslopt_options;
    typedef enum glslopt_target glslopt_target;
    typedef enum glslopt_basic_type glslopt_basic_type;
    typedef enum glslopt_precision glslopt_precision;
#endif

/* [INJEKSI] EXTERN C
   Wajib ada agar Compiler C++ tidak mengacak nama fungsi (Name Mangling),
   sehingga gl4es bisa memanggil fungsinya.
*/
#ifdef __cplusplus
extern "C" {
#endif

/*
 Main GLSL optimizer interface.
 See ../../README.md for more instructions.
*/

struct glslopt_shader;
struct glslopt_ctx;

enum glslopt_shader_type {
	kGlslOptShaderVertex = 0,
	kGlslOptShaderFragment,
};

// Options flags for glsl_optimize
enum glslopt_options {
	kGlslOptionSkipPreprocessor = (1<<0), // Skip preprocessing shader source. Saves some time if you know you don't need it.
	kGlslOptionNotFullShader = (1<<1), // Passed shader is not the full shader source. This makes some optimizations weaker.
};

// Optimizer target language
enum glslopt_target {
	kGlslTargetOpenGL = 0,
	kGlslTargetOpenGLES20 = 1,
	kGlslTargetOpenGLES30 = 2,
	kGlslTargetMetal = 3,
};

// Type info
enum glslopt_basic_type {
	kGlslTypeFloat = 0,
	kGlslTypeInt,
	kGlslTypeBool,
	kGlslTypeTex2D,
	kGlslTypeTex3D,
	kGlslTypeTexCube,
	kGlslTypeTex2DShadow,
	kGlslTypeTex2DArray,
	kGlslTypeOther,
	kGlslTypeCount
};

enum glslopt_precision {
	kGlslPrecHigh = 0,
	kGlslPrecMedium,
	kGlslPrecLow,
	kGlslPrecCount
};

glslopt_ctx* glslopt_initialize (glslopt_target target);
void glslopt_cleanup (glslopt_ctx* ctx);

void glslopt_set_max_unroll_iterations (glslopt_ctx* ctx, unsigned iterations);

glslopt_shader* glslopt_optimize (glslopt_ctx* ctx, glslopt_shader_type type, const char* shaderSource, unsigned options);
bool glslopt_get_status (glslopt_shader* shader);
const char* glslopt_get_output (glslopt_shader* shader);
const char* glslopt_get_raw_output (glslopt_shader* shader);
const char* glslopt_get_log (glslopt_shader* shader);
void glslopt_shader_delete (glslopt_shader* shader);

int glslopt_shader_get_input_count (glslopt_shader* shader);
void glslopt_shader_get_input_desc (glslopt_shader* shader, int index, const char** outName, glslopt_basic_type* outType, glslopt_precision* outPrec, int* outVecSize, int* outMatSize, int* outArraySize, int* outLocation);
int glslopt_shader_get_uniform_count (glslopt_shader* shader);
int glslopt_shader_get_uniform_total_size (glslopt_shader* shader);
void glslopt_shader_get_uniform_desc (glslopt_shader* shader, int index, const char** outName, glslopt_basic_type* outType, glslopt_precision* outPrec, int* outVecSize, int* outMatSize, int* outArraySize, int* outLocation);
int glslopt_shader_get_texture_count (glslopt_shader* shader);
void glslopt_shader_get_texture_desc (glslopt_shader* shader, int index, const char** outName, glslopt_basic_type* outType, glslopt_precision* outPrec, int* outVecSize, int* outMatSize, int* outArraySize, int* outLocation);

// Get *very* approximate shader stats:
// Number of math, texture and flow control instructions.
void glslopt_shader_get_stats (glslopt_shader* shader, int* approxMath, int* approxTex, int* approxFlow);

#ifdef __cplusplus
}
#endif

#endif /* GLSL_OPTIMIZER_H */