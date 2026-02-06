#include "matvec.h"
#include <string.h>
#include <math.h>
#include <arm_neon.h>

/*
 * GL4ES - Math Vector Optimized
 * Replaced legacy Inline ASM with ARM64 NEON Intrinsics
 * for better pipeline scheduling and auto-vectorization.
 */

// Hint untuk compiler agar pointer tidak dianggap tumpang tindih (optimization boost)
#define RESTRICT __restrict

float FASTMATH dot(const float *a, const float *b) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

float FASTMATH dot4(const float *a, const float *b) {
    // Menggunakan NEON Intrinsics untuk load dan multiply-add
    float32x4_t va = vld1q_f32(a);
    float32x4_t vb = vld1q_f32(b);
    float32x4_t vres = vmulq_f32(va, vb);
    // vaddvq_f32 adalah instruksi khusus ARM64 untuk menjumlahkan seluruh elemen vector
    return vaddvq_f32(vres);
}

void cross3(const float *a, const float *b, float* c) {
    // Cross product jarang digunakan dalam batch besar, scalar lebih cepat setup-nya
    // daripada load vector registers.
    float a0=a[0], a1=a[1], a2=a[2];
    float b0=b[0], b1=b[1], b2=b[2];
    
    c[0] = a1*b2 - a2*b1;
    c[1] = a2*b0 - a0*b2; // Optimized order
    c[2] = a0*b1 - a1*b0;
}

void matrix_vector(const float * RESTRICT a, const float * RESTRICT b, float * RESTRICT c) {
    /*
     * Transformasi Vector (b) dengan Matrix (a)
     * c = A * b
     * Kolom-Major matrix standar OpenGL.
     */
    
    float32x4_t vec_b = vld1q_f32(b); // Load b (x, y, z, w)

    // Load Matrix Columns
    float32x4_t col0 = vld1q_f32(a);      // a[0..3]
    float32x4_t col1 = vld1q_f32(a + 4);  // a[4..7]
    float32x4_t col2 = vld1q_f32(a + 8);  // a[8..11]
    float32x4_t col3 = vld1q_f32(a + 12); // a[12..15]

    // c = col0*b.x
    float32x4_t res = vmulq_n_f32(col0, vgetq_lane_f32(vec_b, 0));
    // c += col1*b.y
    res = vmlaq_n_f32(res, col1, vgetq_lane_f32(vec_b, 1));
    // c += col2*b.z
    res = vmlaq_n_f32(res, col2, vgetq_lane_f32(vec_b, 2));
    // c += col3*b.w
    res = vmlaq_n_f32(res, col3, vgetq_lane_f32(vec_b, 3));

    vst1q_f32(c, res);
}

void vector_matrix(const float * RESTRICT a, const float * RESTRICT b, float * RESTRICT c) {
    /*
     * Transformasi Vector (a) sebagai Row dengan Matrix (b)
     * c = a * B
     * Biasanya jarang dipakai di OGL fixed pipeline standar, tapi kita optimalkan.
     */
     
    float32x4_t vec_a = vld1q_f32(a); // Vector a

    // Kita harus melakukan dot product baris a dengan kolom B.
    // Tapi karena B column-major, ini lebih kompleks.
    // Cara cepat: Transpose logic on the fly atau scalar parallel.
    // Untuk A53, pendekatan hybrid terbaik:

    float32x4_t mat_col0 = vld1q_f32(b);
    float32x4_t mat_col1 = vld1q_f32(b + 4);
    float32x4_t mat_col2 = vld1q_f32(b + 8);
    float32x4_t mat_col3 = vld1q_f32(b + 12);

    float32x4_t mul0 = vmulq_f32(vec_a, mat_col0);
    float32x4_t mul1 = vmulq_f32(vec_a, mat_col1);
    float32x4_t mul2 = vmulq_f32(vec_a, mat_col2);
    float32x4_t mul3 = vmulq_f32(vec_a, mat_col3);

    // Sum across results
    c[0] = vaddvq_f32(mul0);
    c[1] = vaddvq_f32(mul1);
    c[2] = vaddvq_f32(mul2);
    c[3] = vaddvq_f32(mul3);
}

void vector3_matrix(const float * RESTRICT a, const float * RESTRICT b, float * RESTRICT c) {
    // Versi khusus vector 3 component (w diasumsikan 1)
    
    // Load a (x,y,z) - w tidak tentu
    float32x4_t vec_a = vld1q_f32(a);
    // Set w = 1.0f untuk perkalian vector
    vec_a = vsetq_lane_f32(1.0f, vec_a, 3); 

    // Sama seperti vector_matrix tapi kita pastikan elemen ke-4 diproses benar
    float32x4_t mat_col0 = vld1q_f32(b);
    float32x4_t mat_col1 = vld1q_f32(b + 4);
    float32x4_t mat_col2 = vld1q_f32(b + 8);
    float32x4_t mat_col3 = vld1q_f32(b + 12);

    float32x4_t mul0 = vmulq_f32(vec_a, mat_col0);
    float32x4_t mul1 = vmulq_f32(vec_a, mat_col1);
    float32x4_t mul2 = vmulq_f32(vec_a, mat_col2);
    float32x4_t mul3 = vmulq_f32(vec_a, mat_col3);

    c[0] = vaddvq_f32(mul0);
    c[1] = vaddvq_f32(mul1);
    c[2] = vaddvq_f32(mul2);
    c[3] = vaddvq_f32(mul3);
}

void vector3_matrix4(const float *a, const float *b, float *c) {
    // Transformasi affine 3x3 subset
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
    // Menggunakan NEON Reciprocal Square Root Estimate + Newton Raphson Step
    // Ini jauh lebih cepat daripada 1.0f / sqrtf() standar
    
    float32x4_t v = vld1q_f32(a);
    // Kita hanya butuh 3 komponen (x,y,z), set w=0 agar tidak ganggu dot product
    v = vsetq_lane_f32(0.0f, v, 3);

    // Dot product (Length Squared)
    float32x4_t dp = vmulq_f32(v, v);
    float sum_sq = vaddvq_f32(dp); // sum = x*x + y*y + z*z

    // Load sum ke seluruh lane vector register
    float32x4_t vsum = vdupq_n_f32(sum_sq);

    // Estimate 1.0 / sqrt(sum)
    float32x4_t rsq = vrsqrteq_f32(vsum);
    // Refine estimate (Newton-Raphson step)
    rsq = vmulq_f32(rsq, vrsqrtsq_f32(vmulq_f32(vsum, rsq), rsq));
    
    // Result = vector * invSqrt
    float32x4_t res = vmulq_f32(v, rsq);
    
    // Simpan kembali 3 komponen pertama
    vst1q_lane_f32(a, res, 0);
    vst1q_lane_f32(a+1, res, 1);
    vst1q_lane_f32(a+2, res, 2);
}

void vector4_normalize(float *a) {
    float32x4_t v = vld1q_f32(a);
    // 3 komponen pertama saja untuk magnitude (biasanya w=1 atau 0)
    float32x4_t v3 = vsetq_lane_f32(0.0f, v, 3);
    
    float32x4_t dp = vmulq_f32(v3, v3);
    float sum_sq = vaddvq_f32(dp);
    
    float32x4_t vsum = vdupq_n_f32(sum_sq);
    float32x4_t rsq = vrsqrteq_f32(vsum);
    rsq = vmulq_f32(rsq, vrsqrtsq_f32(vmulq_f32(vsum, rsq), rsq));
    
    float32x4_t res = vmulq_f32(v, rsq); // Normalize seluruh komponen
    vst1q_f32(a, res);
}

void FASTMATH matrix_transpose(const float *a, float *b) {
    // NEON Transpose 4x4
    float32x4_t r0 = vld1q_f32(a);
    float32x4_t r1 = vld1q_f32(a + 4);
    float32x4_t r2 = vld1q_f32(a + 8);
    float32x4_t r3 = vld1q_f32(a + 12);

    // Transpose operations (Zip / Unzip)
    float32x4x2_t t0 = vzipq_f32(r0, r2);
    float32x4x2_t t1 = vzipq_f32(r1, r3);

    float32x4x2_t res0 = vzipq_f32(t0.val[0], t1.val[0]);
    float32x4x2_t res1 = vzipq_f32(t0.val[1], t1.val[1]);

    vst1q_f32(b, res0.val[0]);
    vst1q_f32(b + 4, res0.val[1]);
    vst1q_f32(b + 8, res1.val[0]);
    vst1q_f32(b + 12, res1.val[1]);
}

void matrix_inverse(const float *m, float *r) {
    // Inversi matriks 4x4 sangat sensitif terhadap presisi.
    // Algoritma scalar Krammer/Cofactor lebih aman untuk stabilitas daripada NEON hack
    // yang bisa loss precision. Kita pertahankan scalar tapi rapihkan indexing-nya.

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
    r[10] = m0*m5*m15 - m0*m13*m7 - m1*m4*m15 + m1*m12*m7 + m3*m4*m13 - m3*m12*m5;
    r[11] = -m0*m5*m11 + m0*m9*m7 + m1*m4*m11 - m1*m8*m7 - m3*m4*m9 + m3*m8*m5;

    r[12] = -m4*m9*m14 + m4*m13*m10 + m5*m8*m14 - m5*m12*m10 - m6*m8*m13 + m6*m12*m9;
    r[13] = m0*m9*m14 - m0*m13*m10 - m1*m8*m14 + m1*m12*m10 + m2*m8*m13 - m2*m12*m9;
    r[14] = -m0*m5*m14 + m0*m13*m6 + m1*m4*m14 - m1*m12*m6 - m2*m4*m13 + m2*m12*m5;
    r[15] = m0*m5*m10 - m0*m9*m6 - m1*m4*m10 + m1*m8*m6 + m2*m4*m9 - m2*m8*m5;

    float det = 1.0f/(m0*r[0] + m1*r[4] + m2*r[8] + m3*r[12]);
    for (int i = 0; i < 16; i++) r[i] *= det;
}

void matrix_inverse3_transpose(const float *m, float *r) {
    r[0] = m[5]*m[10] - m[6]*m[9];
    r[1] = m[6]*m[8] - m[4]*m[10];
    r[2] = m[4]*m[9] - m[5]*m[8];

    r[3] = m[2]*m[9] - m[1]*m[10];
    r[4] = m[0]*m[10] - m[2]*m[8];
    r[5] = m[1]*m[8] - m[0]*m[9];

    r[6] = m[1]*m[6] - m[2]*m[5];
    r[7] = m[2]*m[4] - m[0]*m[6];
    r[8] = m[0]*m[5] - m[1]*m[4];

    float det = 1.0f/(m[0]*r[0] + m[4]*r[3] + m[8]*r[6]);
    for (int i = 0; i < 9; i++) r[i] *= det;
}
    
void matrix_mul(const float * RESTRICT a, const float * RESTRICT b, float * RESTRICT c) {
    /*
     * THE HEAVY HITTER: Matrix Multiplication 4x4
     * C = A * B
     * Dioptimalkan untuk memory bandwidth rendah Helio P35.
     * Strategi: Keep Matrix A columns in registers, stream Matrix B.
     */

    // Load Matrix A Columns (akan dipakai berulang kali)
    float32x4_t a_col0 = vld1q_f32(a);
    float32x4_t a_col1 = vld1q_f32(a + 4);
    float32x4_t a_col2 = vld1q_f32(a + 8);
    float32x4_t a_col3 = vld1q_f32(a + 12);

    // Proses 4 kolom Matrix B (stream)
    for(int i=0; i<4; i++) {
        // Load kolom ke-i dari B
        float32x4_t b_col = vld1q_f32(b + (i*4));
        
        // Mulai akumulasi: C_col = A_col0 * B[0,i]
        float32x4_t res = vmulq_n_f32(a_col0, vgetq_lane_f32(b_col, 0));
        
        // Tambahkan: C_col += A_col1 * B[1,i]
        res = vmlaq_n_f32(res, a_col1, vgetq_lane_f32(b_col, 1));
        
        // Tambahkan: C_col += A_col2 * B[2,i]
        res = vmlaq_n_f32(res, a_col2, vgetq_lane_f32(b_col, 2));
        
        // Tambahkan: C_col += A_col3 * B[3,i]
        res = vmlaq_n_f32(res, a_col3, vgetq_lane_f32(b_col, 3));
        
        // Simpan hasil ke kolom C
        vst1q_f32(c + (i*4), res);
    }
}

void vector4_mult(const float *a, const float *b, float *c) {
    // Implemented Missing NEON
    vst1q_f32(c, vmulq_f32(vld1q_f32(a), vld1q_f32(b)));
}

void vector4_add(const float *a, const float *b, float *c) {
    // Implemented Missing NEON
    vst1q_f32(c, vaddq_f32(vld1q_f32(a), vld1q_f32(b)));
}

void vector4_sub(const float *a, const float *b, float *c) {
    // Implemented Missing NEON
    vst1q_f32(c, vsubq_f32(vld1q_f32(a), vld1q_f32(b)));
}
    
void set_identity(float* mat) {
    // memset is optimized in libc, keep it.
    memset(mat, 0, 16*sizeof(float));
    mat[0] = mat[5] = mat[10] = mat[15] = 1.0f;
}

int is_identity(const float* mat) {
    /* * OPTIMIZATION:
     * Hindari memcmp dan static variable initialization.
     * Langsung cek elemen floating point.
     * Ini jauh lebih cepat karena tidak mengakses global memory (cache miss).
     */
    
    // Check diagonal 1.0
    if (mat[0] != 1.0f || mat[5] != 1.0f || mat[10] != 1.0f || mat[15] != 1.0f) 
        return 0;

    // Check others 0.0 (Unrolled for instruction interleaving)
    // Column 0
    if (mat[1]!=0.0f || mat[2]!=0.0f || mat[3]!=0.0f) return 0;
    // Column 1
    if (mat[4]!=0.0f || mat[6]!=0.0f || mat[7]!=0.0f) return 0;
    // Column 2
    if (mat[8]!=0.0f || mat[9]!=0.0f || mat[11]!=0.0f) return 0;
    // Column 3
    if (mat[12]!=0.0f || mat[13]!=0.0f || mat[14]!=0.0f) return 0;

    return 1;
}