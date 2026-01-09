#include "spirv_flow.h"
#include <stdio.h>

extern "C" {

// Saat ini kita return 0 dulu (Artinya: Fitur SPIR-V belum aktif)
// Ini pengaman agar tidak crash saat pertama kali build.
int is_spirv_enabled() {
    return 0; 
}

}

