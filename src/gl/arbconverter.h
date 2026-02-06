#ifndef _GL4ES_ARBCONVERTER_H_
#define _GL4ES_ARBCONVERTER_H_

#include <stdint.h>

// Hint to compiler: pointer 'code', 'error_msg', and 'error_ptr' do not overlap.
// This allows aggressive optimization on ARM64.
char* gl4es_convertARB(const char* restrict code, int vertex, char ** restrict error_msg, int * restrict error_ptr);

#endif // _GL4ES_ARBCONVERTER_H_