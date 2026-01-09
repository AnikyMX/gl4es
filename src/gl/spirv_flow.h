#ifndef _GL4ES_SPIRV_FLOW_H_
#define _GL4ES_SPIRV_FLOW_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <GLES2/gl2.h>

char* spirv_try_convert(const char* source, GLenum shaderType);

#ifdef __cplusplus
}
#endif

#endif // _GL4ES_SPIRV_FLOW_H_