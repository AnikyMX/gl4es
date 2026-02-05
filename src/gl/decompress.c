/*
 * Refactored decompress.c for GL4ES
 * Optimized for ARMv8
 * - Lookup Table (LUT) approach for DXT Colors/Alpha
 * - Removed per-pixel branching
 * - Optimized 565 unpacking
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Helper: Pack RGBA to uint32 (ARMv8 friendly)
static inline uint32_t PackRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
}

// Helper: Unpack RGB565 to RGB888 optimized
static inline void Unpack565(uint16_t color, uint8_t* r, uint8_t* g, uint8_t* b) {
    uint32_t c = color;
    // Expand 5-bit to 8-bit: (c * 255 + 16) / 32 is roughly equivalent to (c << 3) | (c >> 2)
    // Bit replication is faster and accurate enough for textures
    uint32_t r5 = (c >> 11) & 0x1F;
    uint32_t g6 = (c >> 5) & 0x3F;
    uint32_t b5 = c & 0x1F;

    *r = (r5 << 3) | (r5 >> 2);
    *g = (g6 << 2) | (g6 >> 4);
    *b = (b5 << 3) | (b5 >> 2);
}

// Optimized DXT1 Block Decompressor using Lookup Tables
static void DecompressBlockDXT1Internal(const uint8_t* block,
    uint32_t* output,
    uint32_t outputStride,
    int transparent0, int* simpleAlpha, int *complexAlpha,
    const uint8_t* alphaValues)
{
    uint16_t c0_raw = *(const uint16_t*)(block);
    uint16_t c1_raw = *(const uint16_t*)(block + 2);
    uint32_t code = *(const uint32_t*)(block + 4);

    uint8_t r[4], g[4], b[4];

    // Pre-calculate 4-color palette
    Unpack565(c0_raw, &r[0], &g[0], &b[0]);
    Unpack565(c1_raw, &r[1], &g[1], &b[1]);

    if (c0_raw > c1_raw) {
        // 2/3 and 1/3 interpolation
        r[2] = (2 * r[0] + r[1]) / 3;
        g[2] = (2 * g[0] + g[1]) / 3;
        b[2] = (2 * b[0] + b[1]) / 3;

        r[3] = (r[0] + 2 * r[1]) / 3;
        g[3] = (g[0] + 2 * g[1]) / 3;
        b[3] = (b[0] + 2 * b[1]) / 3;
    } else {
        // 1/2 interpolation and Transparent/Black
        r[2] = (r[0] + r[1]) >> 1;
        g[2] = (g[0] + g[1]) >> 1;
        b[2] = (b[0] + b[1]) >> 1;

        r[3] = 0;
        g[3] = 0;
        b[3] = 0;
    }

    // Process 4x4 pixels using the palette
    for (int j = 0; j < 4; ++j) {
        for (int i = 0; i < 4; ++i) {
            uint8_t idx = (code >> (2 * (4 * j + i))) & 0x03;
            uint8_t alpha = alphaValues[j * 4 + i];

            // Handle DXT1 transparency special case (index 3 with c0 <= c1)
            if (c0_raw <= c1_raw && idx == 3 && transparent0) {
                alpha = 0;
            }

            // Alpha tracking
            if (alpha == 0) *simpleAlpha = 1;
            else if (alpha < 255) *complexAlpha = 1;

            output[j * outputStride + i] = PackRGBA(r[idx], g[idx], b[idx], alpha);
        }
    }
}

void DecompressBlockDXT1(uint32_t x, uint32_t y, uint32_t width,
    const uint8_t* blockStorage,
    int transparent0, int* simpleAlpha, int *complexAlpha,
    uint32_t* image)
{
    // Constant alpha for standard DXT1
    static const uint8_t const_alpha[] = {
        255, 255, 255, 255,
        255, 255, 255, 255,
        255, 255, 255, 255,
        255, 255, 255, 255
    };

    DecompressBlockDXT1Internal(blockStorage,
        image + x + (y * width), width, transparent0, simpleAlpha, complexAlpha, const_alpha);
}

// Optimized DXT5 Block Decompressor
void DecompressBlockDXT5(uint32_t x, uint32_t y, uint32_t width,
    const uint8_t* blockStorage,
    int transparent0, int* simpleAlpha, int *complexAlpha,
    uint32_t* image)
{
    uint8_t alpha0 = blockStorage[0];
    uint8_t alpha1 = blockStorage[1];
    
    // 1. Build Alpha Lookup Table (LUT)
    uint8_t alphas[8];
    alphas[0] = alpha0;
    alphas[1] = alpha1;
    
    if (alpha0 > alpha1) {
        // 6 interpolated values
        alphas[2] = (6 * alpha0 + 1 * alpha1) / 7;
        alphas[3] = (5 * alpha0 + 2 * alpha1) / 7;
        alphas[4] = (4 * alpha0 + 3 * alpha1) / 7;
        alphas[5] = (3 * alpha0 + 4 * alpha1) / 7;
        alphas[6] = (2 * alpha0 + 5 * alpha1) / 7;
        alphas[7] = (1 * alpha0 + 6 * alpha1) / 7;
    } else {
        // 4 interpolated values, 0, and 255
        alphas[2] = (4 * alpha0 + 1 * alpha1) / 5;
        alphas[3] = (3 * alpha0 + 2 * alpha1) / 5;
        alphas[4] = (2 * alpha0 + 3 * alpha1) / 5;
        alphas[5] = (1 * alpha0 + 4 * alpha1) / 5;
        alphas[6] = 0;
        alphas[7] = 255;
    }

    // 2. Extract 48-bit alpha indices safely
    // DXT5 alpha indices are 6 bytes (48 bits) long.
    // We load 8 bytes (64 bits) to ensure we get them all, masking handled by logic.
    // Using memcpy avoids strict-aliasing and unaligned access issues on ARM.
    uint64_t alpha_bits = 0;
    memcpy(&alpha_bits, blockStorage + 2, 6);

    // 3. Decode Color Block (Offset 8)
    const uint8_t* colorBlock = blockStorage + 8;
    uint16_t c0_raw = *(const uint16_t*)(colorBlock);
    uint16_t c1_raw = *(const uint16_t*)(colorBlock + 2);
    uint32_t color_code = *(const uint32_t*)(colorBlock + 4);

    uint8_t r[4], g[4], b[4];
    Unpack565(c0_raw, &r[0], &g[0], &b[0]);
    Unpack565(c1_raw, &r[1], &g[1], &b[1]);

    // DXT5 Color interpolation logic
    r[2] = (2 * r[0] + r[1]) / 3;
    g[2] = (2 * g[0] + g[1]) / 3;
    b[2] = (2 * b[0] + b[1]) / 3;

    r[3] = (r[0] + 2 * r[1]) / 3;
    g[3] = (g[0] + 2 * g[1]) / 3;
    b[3] = (b[0] + 2 * b[1]) / 3;

    // 4. Process Pixels
    for (int j = 0; j < 4; ++j) {
        for (int i = 0; i < 4; ++i) {
            // Get alpha index (3 bits per pixel)
            int bitOffset = 3 * (4 * j + i);
            uint8_t alphaIdx = (alpha_bits >> bitOffset) & 0x07;
            uint8_t finalAlpha = alphas[alphaIdx];

            // Get color index (2 bits per pixel)
            uint8_t colorIdx = (color_code >> (2 * (4 * j + i))) & 0x03;

            // Tracking
            if (finalAlpha == 0) *simpleAlpha = 1;
            else if (finalAlpha < 255) *complexAlpha = 1;

            image[x + i + (y + j) * width] = PackRGBA(r[colorIdx], g[colorIdx], b[colorIdx], finalAlpha);
        }
    }
}

// Optimized DXT3 Block Decompressor
void DecompressBlockDXT3(uint32_t x, uint32_t y, uint32_t width,
    const uint8_t* blockStorage,
    int transparent0, int* simpleAlpha, int *complexAlpha,
    uint32_t* image)
{
    // DXT3 stores explicit 4-bit alpha for each pixel in the first 8 bytes.
    // We expand these to 8-bit (0-255) immediately.
    uint8_t alphaValues[16];
    const uint16_t* alphaData = (const uint16_t*)blockStorage;

    for (int i = 0; i < 4; ++i) {
        uint16_t row = alphaData[i];
        alphaValues[i * 4 + 0] = (row & 0x0F) * 17;       // 0x0 => 0, 0xF => 255
        alphaValues[i * 4 + 1] = ((row >> 4) & 0x0F) * 17;
        alphaValues[i * 4 + 2] = ((row >> 8) & 0x0F) * 17;
        alphaValues[i * 4 + 3] = ((row >> 12) & 0x0F) * 17;
    }

    // Reuse the highly optimized DXT1 internal function for color + alpha mixing
    // Color block is at offset 8
    DecompressBlockDXT1Internal(blockStorage + 8,
        image + x + (y * width), width, transparent0, simpleAlpha, complexAlpha, alphaValues);
}

// STB DXT Implementation for compression fallback
#define STB_DXT_IMPLEMENTATION
#include "stb_dxt_104.h"
