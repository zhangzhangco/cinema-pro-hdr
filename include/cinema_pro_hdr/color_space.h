#pragma once

#include "core.h"

namespace CinemaProHDR {

// Color space conversion functions
class ColorSpaceConverter {
public:
    // PQ EOTF/OETF functions (ST 2084)
    static float PQ_EOTF(float pq_value);
    static float PQ_OETF(float linear_value);
    
    // Batch processing
    static void PQ_EOTF_RGB(const float* pq_rgb, float* linear_rgb);
    static void PQ_OETF_RGB(const float* linear_rgb, float* pq_rgb);
    
    // Color space transformation matrices
    static void BT2020_to_P3D65(const float* bt2020, float* p3d65);
    static void P3D65_to_BT2020(const float* p3d65, float* bt2020);
    static void BT2020_to_XYZ(const float* bt2020, float* xyz);
    static void XYZ_to_BT2020(const float* xyz, float* bt2020);
    
    // Working domain conversions
    static void ToWorkingDomain(const Image& input, Image& output);
    static void FromWorkingDomain(const Image& input, Image& output, ColorSpace target_cs);
    
    // Utility functions
    static bool IsValidColorSpace(ColorSpace cs);
    static std::string ColorSpaceToString(ColorSpace cs);
    
private:
    // PQ constants (ST 2084)
    static constexpr float PQ_M1 = 0.1593017578125f;      // 2610/16384
    static constexpr float PQ_M2 = 78.84375f;             // 2523/32
    static constexpr float PQ_C1 = 0.8359375f;            // 3424/4096
    static constexpr float PQ_C2 = 18.8515625f;           // 2413/128
    static constexpr float PQ_C3 = 18.6875f;              // 2392/128
    
    // Color transformation matrices (corrected values)
    static constexpr float BT2020_TO_P3D65_MATRIX[9] = {
         1.7166511f, -0.3556708f, -0.2533663f,
        -0.6666844f,  1.6164812f,  0.0157685f,
         0.0176399f, -0.0427706f,  0.9421031f
    };
    
    static constexpr float P3D65_TO_BT2020_MATRIX[9] = {
         0.6954522f,  0.1406787f,  0.1638665f,
         0.2447174f,  0.6720283f,  0.0832584f,
        -0.0011542f,  0.0280727f,  1.0609851f
    };
    
    static constexpr float BT2020_TO_XYZ_MATRIX[9] = {
         0.6369580f,  0.1446169f,  0.1688809f,
         0.2627045f,  0.6779981f,  0.0593017f,
         0.0000000f,  0.0280727f,  1.0609851f
    };
    
    // Matrix multiplication helper
    static void MultiplyMatrix3x3(const float* matrix, const float* input, float* output);
};

// Numerical stability functions
class NumericalUtils {
public:
    // NaN/Inf detection and handling
    static bool IsFinite(float value);
    static bool IsFiniteRGB(const float* rgb);
    static void SaturateRGB(float* rgb);
    
    // Safe mathematical operations
    static float SafePow(float base, float exponent);
    static float SafeLog(float value);
    static float SafeDivide(float numerator, float denominator, float fallback = 0.0f);
    
    // Smoothstep and interpolation
    static float SmoothStep(float edge0, float edge1, float x);
    static float Mix(float a, float b, float t);
    
    // Validation helpers
    static bool IsInRange(float value, float min_val, float max_val);
    static float ClampToRange(float value, float min_val, float max_val);
};

} // namespace CinemaProHDR