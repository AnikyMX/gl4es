#ifndef _GL4ES_SPIRV_WRAPPER_H_
#define _GL4ES_SPIRV_WRAPPER_H_

#include "../gl/shaderconv.h"

#ifdef __cplusplus
extern "C" {
#endif

// Fungsi utama yang akan dipanggil oleh shaderconv.c
// Mengembalikan NULL jika konversi gagal (agar fallback ke metode lama)
char* ConvertShaderSPIRV(const char* pEntry, int isVertex, shaderconv_need_t *need);

#ifdef __cplusplus
}
#endif

#endif // _GL4ES_SPIRV_WRAPPER_H_