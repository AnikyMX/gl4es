#include "matvec.h"
#include <math.h>
#include <string.h>

// Helper to make code cleaner
#ifndef FASTMATH
#define FASTMATH
#endif

float FASTMATH dot(const float *a, const float *b) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

float FASTMATH dot4(const float *a, const float *b) {
#if defined(__ARM_NEON__) && !defined(__APPLE__)
    register float ret;
    asm volatile (
    "vld1.f32 {d0-d1}, [%1]        \n" // q0 = a(0..3)
    "vld1.f32 {d2-d3}, [%2]        \n" // q1 = b(0..3)
    "vmul.f32 q0, q0, q1           \n" // q0 = a*b per component
    "vpadd.f32 d0, d0, d1          \n" // pair add part 1
    "vpadd.f32 d0, d0, d0          \n" // pair add part 2 to get scalar
    "vmov.f32 %0, s0               \n"
    :"=w"(ret): "r"(a), "r"(b)
    : "q0", "q1", "memory"
        );
    return ret;
#else
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3];
#endif
}

void cross3(const float *a, const float *b, float* c) {
    // FIX: Original code used a[3] (W component) for Y calculation which is mathematically wrong for 3D Cross Product.
    // Correct formula:
    // x = y1*z2 - z1*y2  (1,2)
    // y = z1*x2 - x1*z2  (2,0)
    // z = x1*y2 - y1*x2  (0,1)
    
    // Using local vars to help compiler optimization
    float a0=a[0], a1=a[1], a2=a[2];
    float b0=b[0], b1=b[1], b2=b[2];
    
    c[0] = a1*b2 - a2*b1;
    c[1] = a2*b0 - a0*b2; // FIXED (was a[3]*b[0] - a[0]*b[3])
    c[2] = a0*b1 - a1*b0;
}

void matrix_vector(const float *a, const float *b, float *c) {
#if defined(__ARM_NEON__) && !defined(__APPLE__)
    const float* a1 = a+8;
    asm volatile (
    "vld4.f32 {d0,d2,d4,d6}, [%1]        \n" 
    "vld4.f32 {d1,d3,d5,d7}, [%2]        \n" // q0-q3 = columns
    "vld1.f32 {q4}, [%3]       \n" // q4 = vector b
    "vmul.f32 q0, q0, d8[0]    \n" // q0 = col0 * b[0]
    "vmla.f32 q0, q1, d8[1]    \n" // q0 += col1 * b[1]
    "vmla.f32 q0, q2, d9[0]    \n" // q0 += col2 * b[2]
    "vmla.f32 q0, q3, d9[1]    \n" // q0 += col3 * b[3]
    "vst1.f32 {q0}, [%0]       \n"
    ::"r"(c), "r"(a), "r"(a1), "r"(b)
    : "q0", "q1", "q2", "q3", "q4", "memory"
        );
#else
    c[0] = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    c[1] = a[4] * b[0] + a[5] * b[1] + a[6] * b[2] + a[7] * b[3];
    c[2] = a[8] * b[0] + a[9] * b[1] + a[10] * b[2] + a[11] * b[3];
    c[3] = a[12] * b[0] + a[13] * b[1] + a[14] * b[2] + a[15] * b[3];
#endif
}

void vector_matrix(const float *a, const float *b, float *c) {
#if defined(__ARM_NEON__) && !defined(__APPLE__)
    const float* b2=b+4;
    const float* b3=b+8;
    const float* b4=b+12;
    asm volatile (
    "vld1.f32 {q0}, [%1]        \n" // %q0 = vector a
    "vld1.f32 {q1}, [%2]        \n" // %q1 = b row 0
    "vmul.f32 q1, q1, d0[0]     \n" // %q1 = row0 * a[0]
    "vld1.f32 {q2}, [%3]        \n" // %q2 = b row 1
    "vmla.f32 q1, q2, d0[1]     \n" // %q1 += row1 * a[1]
    "vld1.f32 {q2}, [%4]        \n" // %q2 = b row 2
    "vmla.f32 q1, q2, d1[0]     \n" // %q1 += row2 * a[2]
    "vld1.f32 {q2}, [%5]        \n" // %q2 = b row 3
    "vmla.f32 q1, q2, d1[1]     \n" // %q1 += row3 * a[3]
    "vst1.f32 {q1}, [%0]        \n"
    ::"r"(c), "r"(a), "r"(b), "r"(b2), "r"(b3), "r"(b4)
    : "q0", "q1", "q2", "memory"
        );
#else
    const float a0=a[0], a1=a[1], a2=a[2], a3=a[3];
    c[0] = a0 * b[0] + a1 * b[4] + a2 * b[8] + a3 * b[12];
    c[1] = a0 * b[1] + a1 * b[5] + a2 * b[9] + a3 * b[13];
    c[2] = a0 * b[2] + a1 * b[6] + a2 * b[10] + a3 * b[14];
    c[3] = a0 * b[3] + a1 * b[7] + a2 * b[11] + a3 * b[15];
#endif
}

void vector3_matrix(const float *a, const float *b, float *c) {
#if defined(__ARM_NEON__) && !defined(__APPLE__)
    const float* b2=b+4;
    const float* b3=b+8;
    const float* b4=b+12;
    asm volatile (
    "vld1.32  {d0}, [%1]        \n" // load a[0], a[1]
    "flds     s2, [%1, #8]      \n" // load a[2]
    //"vsub.f32 s3, s3, s3        \n" // clear a[3] if needed, but we don't multiply it
    
    "vld1.f32 {q1}, [%2]        \n" // row 0
    "vmul.f32 q1, q1, d0[0]     \n" // acc = row0 * a[0]
    
    "vld1.f32 {q2}, [%3]        \n" // row 1
    "vmla.f32 q1, q2, d0[1]     \n" // acc += row1 * a[1]
    
    "vld1.f32 {q2}, [%4]        \n" // row 2
    "vmla.f32 q1, q2, d1[0]     \n" // acc += row2 * a[2]
    
    "vld1.f32 {q2}, [%5]        \n" // row 3 (translation part)
    "vadd.f32 q1, q1, q2        \n" // acc += row3 (implicitly * 1.0)
    
    "vst1.f32 {q1}, [%0]        \n"
    ::"r"(c), "r"(a), "r"(b), "r"(b2), "r"(b3), "r"(b4)
    : "q0", "q1", "q2", "memory"
        );
#else
    c[0] = a[0] * b[0] + a[1] * b[4] + a[2] * b[8] + b[12];
    c[1] = a[0] * b[1] + a[1] * b[5] + a[2] * b[9] + b[13];
    c[2] = a[0] * b[2] + a[1] * b[6] + a[2] * b[10] + b[14];
    c[3] = a[0] * b[3] + a[1] * b[7] + a[2] * b[11] + b[15];
#endif
}

void vector3_matrix4(const float *a, const float *b, float *c) {
    c[0] = a[0] * b[0] + a[1] * b[4] + a[2] * b[8];
    c[1] = a[0] * b[1] + a[1] * b[5] + a[2] * b[9];
    c[2] = a[0] * b[2] + a[1] * b[6] + a[2] * b[10];
}

void vector3_matrix3(const float *a, const float *b, float *c) {
    c[0] = a[0] * b[0] + a[1] * b[3] + a[2] * b[6];
    c[1] = a[0] * b[1] + a[1] * b[4] + a[2] * b[7];
    c[2] = a[0] * b[2] + a[1] * b[5] + a[2] * b[8];
}

void vector_normalize(float *a) {
#if defined(__ARM_NEON__) && !defined(__APPLE__)
        asm volatile (
        "vld1.32                {d4}, [%0]                      \n\t"   // d4 = {x, y}
        "flds                   s10, [%0, #8]                   \n\t"   // d5[0] = z
        
        "vmul.f32               d0, d4, d4                      \n\t"   // d0 = x*x, y*y
        "vpadd.f32              d0, d0                          \n\t"   // d0 = x*x + y*y
        "vmla.f32               d0, d5, d5                      \n\t"   // d0 += z*z (Length Sq)
        
        "vmov.f32               d1, d0                          \n\t"   // d1 = Length Sq
        "vrsqrte.f32    		d0, d0                          \n\t"   // d0 = approx 1/sqrt(LenSq)
        
        // Newton-Raphson Step 1 (Refine accuracy)
        "vmul.f32               d2, d0, d1                      \n\t"   
        "vrsqrts.f32    		d3, d2, d0                      \n\t"         
        "vmul.f32               d0, d0, d3                      \n\t"   
        
        // Multiply original vector by 1/Length
        "vmul.f32               q2, q2, d0[0]                   \n\t"   
        
        "vst1.32                {d4}, [%0]                     	\n\t"   
        "fsts                   s10, [%0, #8]                   \n\t"   
        
        :"+&r"(a): 
    : "d0", "d1", "d2", "d3", "d4", "d5", "q2", "memory"
        );
#else
    float det=1.0f/sqrtf(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]);
    a[0]*=det;
    a[1]*=det;
    a[2]*=det;
#endif
}

void vector4_normalize(float *a) {
#if defined(__ARM_NEON__) && !defined(__APPLE__)
        asm volatile (
        "vld1.32                {q2}, [%0]                      \n\t"   // q2 = {x, y, z, w}

        "vmul.f32               q0, q2, q2                      \n\t"   // square components
        "vpadd.f32              d0, d0, d1                      \n\t"   // x+y, z+w
        "vpadd.f32              d0, d0, d0                      \n\t"   // x+y+z+w (Length Sq) - Note: assumes W is 0 for normal vector or we want W normalization too
        
        "vmov.f32               d1, d0                          \n\t"
        "vrsqrte.f32    		d0, d0                          \n\t"
        
        // Newton-Raphson Step 1
        "vmul.f32               d2, d0, d1                      \n\t"
        "vrsqrts.f32    		d3, d2, d0                      \n\t"
        "vmul.f32               d0, d0, d3                      \n\t"

        "vmul.f32               q2, q2, d0[0]                   \n\t"
        "vst1.32                {q2}, [%0]                    	\n\t"
        
        :"+&r"(a): 
    : "d0", "d1", "d2", "d3", "q0", "q1", "q2", "memory"
        );
#else
    float det=1.0f/sqrtf(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]);
    a[0]*=det;
    a[1]*=det;
    a[2]*=det;
#endif
}

void FASTMATH matrix_transpose(const float *a, float *b) {
#if defined(__ARM_NEON__) && !defined(__APPLE__)
   const float* a1 = a+8;
	float* b1=b+8;
    asm volatile (
    "vld4.f32 {d0,d2,d4,d6}, [%1]        \n" 
    "vld4.f32 {d1,d3,d5,d7}, [%2]        \n" 
    "vst1.f32 {d0-d3}, [%0]        \n"
    "vst1.f32 {d4-d7}, [%3]        \n"
    ::"r"(b), "r"(a), "r"(a1), "r"(b1)
    : "q0", "q1", "q2", "q3", "memory"
        );
#else
    for (int i=0; i<4; i++)
        for (int j=0; j<4; j++)
            b[i*4+j]=a[i+j*4];
#endif
}

void matrix_inverse(const float *m, float *r) {
    // Unrolled scalar implementation is often faster than unoptimized NEON for 4x4 inverse due to complexity
    // But we use local variables to ensure registers are used efficiently.
    float m0=m[0], m1=m[1], m2=m[2], m3=m[3];
    float m4=m[4], m5=m[5], m6=m[6], m7=m[7];
    float m8=m[8], m9=m[9], m10=m[10], m11=m[11];
    float m12=m[12], m13=m[13], m14=m[14], m15=m[15];

    r[0] = m5*m10*m15 - m5*m14*m11 - m6*m9*m15 + m6*m13*m11 + m7*m9*m14 - m7*m13*m10;
    r[1] = -m1*m10*m15 + m1*m14*m11 + m2*m9*m15 - m2*m13*m11 - m3*m9*m14 + m3*m13*m10;
    r[2] = m1*m6*m15 - m1*m14*m7 - m2*m5*m15 + m2*m13*m7 + m3*m5*m14 - m3*m13*m6;
    r[3] = -m1*m6*m11 + m1*m10*m7 + m2*m5*m11 - m2*m9*m7 - m3*m5*m10 + m3*m9*m6;

    r[4] = -m4*m10*m15 + m4*m14*m11 + m6*m8*m15 - m6*m12*m11 - m7*m8*m14 + m7*m12*m10;
    r[5] = m0*m10*m15 - m0*m14*m11 - m2*m8*m15 + m2*m12*m11 + m3*m8*m14 - m3*m12*m10;
    r[6] = -m0*m6*m15 + m0*m14*m7 + m2*m4*m15 - m2*m12*m7 - m3*m4*m14 + m3*m12*m6;
    r[7] = m0*m6*m11 - m0*m10*m7 - m2*m4*m11 + m2*m8*m7 + m3*m4*m10 - m3*m8*m6;

    r[8] = m4*m9*m15 - m4*m13*m11 - m5*m8*m15 + m5*m12*m11 + m7*m8*m13 - m7*m12*m9;
    r[9] = -m0*m9*m15 + m0*m13*m11 + m1*m8*m15 - m1*m12*m11 - m3*m8*m13 + m3*m12*m9;
    r[10] = m0*m5*m15 - m0*m13*7 - m1*m4*m15 + m1*m12*7 + m3*m4*m13 - m3*m12*m5;
    r[11] = -m0*m5*m11 + m0*m9*7 + m1*m4*m11 - m1*m8*7 - m3*m4*m9 + m3*m8*m5;

    r[12] = -m4*m9*m14 + m4*m13*m10 + m5*m8*m14 - m5*m12*m10 - m6*m8*m13 + m6*m12*m9;
    r[13] = m0*m9*m14 - m0*m13*m10 - m1*m8*m14 + m1*m12*m10 + m2*m8*m13 - m2*m12*m9;
    r[14] = -m0*m5*m14 + m0*m13*m6 + m1*m4*m14 - m1*m12*m6 - m2*m4*m13 + m2*m12*m5;
    r[15] = m0*m5*m10 - m0*m9*m6 - m1*m4*m10 + m1*m8*m6 + m2*m4*m9 - m2*m8*m5;

    float det = 1.0f/(m0*r[0] + m1*r[4] + m2*r[8] + m3*r[12]);
    for (int i = 0; i < 16; i++) r[i] *= det;
}

void matrix_inverse3_transpose(const float *m, float *r) {
    // Similar optimization using local vars
    float m0=m[0], m1=m[1], m2=m[2];
    float m4=m[4], m5=m[5], m6=m[6];
    float m8=m[8], m9=m[9], m10=m[10];

    r[0] = m5*m10 - m6*m9;
    r[1] = m6*m8 - m4*m10;
    r[2] = m4*m9 - m5*m8;

    r[3] = m2*m9 - m1*m10;
    r[4] = m0*m10 - m2*m8;
    r[5] = m1*m8 - m0*m9;

    r[6] = m1*m6 - m2*m5;
    r[7] = m2*m4 - m0*m6;
    r[8] = m0*m5 - m1*m4;

    float det = 1.0f/(m0*r[0] + m4*r[3] + m8*r[6]);
    for (int i = 0; i < 9; i++) r[i] *= det;
}
    
void matrix_mul(const float *a, const float *b, float *c) {
#if defined(__ARM_NEON__) && !defined(__APPLE__)
    const float* a1 = a+8;
	const float* b1=b+8;
    float* c1=c+8;
    asm volatile (
    "vld1.32  {d16-d19}, [%2]       \n" // Load A upper
    "vld1.32  {d20-d23}, [%3]       \n" // Load A lower
    "vld1.32  {d0-d3}, [%4]         \n" // Load B upper
    "vld1.32  {d4-d7}, [%5]         \n" // Load B lower
    
    // Column 0
    "vmul.f32 q12, q8, d0[0]        \n" 
    "vmla.f32 q12, q9, d0[1]        \n"
    "vmla.f32 q12, q10, d1[0]       \n"
    "vmla.f32 q12, q11, d1[1]       \n"
    
    // Column 1
    "vmul.f32 q13, q8, d2[0]        \n"
    "vmla.f32 q13, q9, d2[1]        \n"
    "vmla.f32 q13, q10, d3[0]       \n"
    "vmla.f32 q13, q11, d3[1]       \n"

    // Column 2
    "vmul.f32 q14, q8, d4[0]        \n"
    "vmla.f32 q14, q9, d4[1]        \n"
    "vmla.f32 q14, q10, d5[0]       \n"
    "vmla.f32 q14, q11, d5[1]       \n"

    // Column 3
    "vmul.f32 q15, q8, d6[0]        \n"
    "vmla.f32 q15, q9, d6[1]        \n"
    "vmla.f32 q15, q10, d7[0]       \n"
    "vmla.f32 q15, q11, d7[1]       \n"

    "vst1.32  {d24-d27}, [%0]       \n"
    "vst1.32  {d28-d31}, [%1]       \n"
    ::"r"(c), "r"(c1), "r"(a), "r"(a1), "r"(b), "r"(b1)
    : "q0", "q1", "q2", "q3", 
      "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15", "memory"
        );
#else
   float a00 = a[0], a01 = a[1], a02 = a[2], a03 = a[3],
        a10 = a[4], a11 = a[5], a12 = a[6], a13 = a[7],
        a20 = a[8], a21 = a[9], a22 = a[10], a23 = a[11],
        a30 = a[12], a31 = a[13], a32 = a[14], a33 = a[15];

    float b0  = b[0], b1 = b[1], b2 = b[2], b3 = b[3];
    c[0] = b0*a00 + b1*a10 + b2*a20 + b3*a30;
    c[1] = b0*a01 + b1*a11 + b2*a21 + b3*a31;
    c[2] = b0*a02 + b1*a12 + b2*a22 + b3*a32;
    c[3] = b0*a03 + b1*a13 + b2*a23 + b3*a33;

    b0 = b[4]; b1 = b[5]; b2 = b[6]; b3 = b[7];
    c[4] = b0*a00 + b1*a10 + b2*a20 + b3*a30;
    c[5] = b0*a01 + b1*a11 + b2*a21 + b3*a31;
    c[6] = b0*a02 + b1*a12 + b2*a22 + b3*a32;
    c[7] = b0*a03 + b1*a13 + b2*a23 + b3*a33;

    b0 = b[8]; b1 = b[9]; b2 = b[10]; b3 = b[11];
    c[8] = b0*a00 + b1*a10 + b2*a20 + b3*a30;
    c[9] = b0*a01 + b1*a11 + b2*a21 + b3*a31;
    c[10] = b0*a02 + b1*a12 + b2*a22 + b3*a32;
    c[11] = b0*a03 + b1*a13 + b2*a23 + b3*a33;

    b0 = b[12]; b1 = b[13]; b2 = b[14]; b3 = b[15];
    c[12] = b0*a00 + b1*a10 + b2*a20 + b3*a30;
    c[13] = b0*a01 + b1*a11 + b2*a21 + b3*a31;
    c[14] = b0*a02 + b1*a12 + b2*a22 + b3*a32;
    c[15] = b0*a03 + b1*a13 + b2*a23 + b3*a33;
#endif
}

void vector4_mult(const float *a, const float *b, float *c) {
#if defined(__ARM_NEON__) && !defined(__APPLE__)
    asm volatile (
    "vld1.32  {q0}, [%1]       \n" 
    "vld1.32  {q1}, [%2]       \n"
    "vmul.f32 q0, q0, q1       \n"
    "vst1.32  {q0}, [%0]       \n"
    ::"r"(c), "r"(a), "r"(b)
    : "q0", "q1", "memory"
    );
#else
    for (int i=0; i<4; i++)
        c[i] = a[i]*b[i];
#endif
}

void vector4_add(const float *a, const float *b, float *c) {
#if defined(__ARM_NEON__) && !defined(__APPLE__)
    asm volatile (
    "vld1.32  {q0}, [%1]       \n" 
    "vld1.32  {q1}, [%2]       \n"
    "vadd.f32 q0, q0, q1       \n"
    "vst1.32  {q0}, [%0]       \n"
    ::"r"(c), "r"(a), "r"(b)
    : "q0", "q1", "memory"
    );
#else
    for (int i=0; i<4; i++)
        c[i] = a[i]+b[i];
#endif
}

void vector4_sub(const float *a, const float *b, float *c) {
#if defined(__ARM_NEON__) && !defined(__APPLE__)
    asm volatile (
    "vld1.32  {q0}, [%1]       \n" 
    "vld1.32  {q1}, [%2]       \n"
    "vsub.f32 q0, q0, q1       \n"
    "vst1.32  {q0}, [%0]       \n"
    ::"r"(c), "r"(a), "r"(b)
    : "q0", "q1", "memory"
    );
#else
    for (int i=0; i<4; i++)
        c[i] = a[i]-b[i];
#endif
}
    
void set_identity(float* mat) {
    memset(mat, 0, 16*sizeof(float));
    mat[0] = mat[1+4] = mat[2+8] = mat[3+12] = 1.0f;
}

int is_identity(const float* mat) {
    static float i1[16];
    static int set=0;
    if(!set) {set_identity(i1); set=1;}
    return memcmp(mat, i1, 16*sizeof(float))==0?1:0;
}
