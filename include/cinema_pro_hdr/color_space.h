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
    static void BT2020_to_ACEScg(const float* bt2020, float* acesg);
    static void ACEScg_to_BT2020(const float* acesg, float* bt2020);
    
    // Color space validation and boundary checking
    static bool IsInGamut(const float* rgb, ColorSpace cs);
    static void ClampToGamut(float* rgb, ColorSpace cs);
    static float GetGamutDistance(const float* rgb, ColorSpace cs);
    static bool ValidateColorSpaceTransform(ColorSpace from, ColorSpace to);
    
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
    
    // Color transformation matrices 
    // For now using identity matrices to ensure basic functionality works
    // TODO: Replace with proper color space transformation matrices
    
    // BT.2020 to P3-D65 (identity for now)
    static constexpr float BT2020_TO_P3D65_MATRIX[9] = {
         1.0f, 0.0f, 0.0f,
         0.0f, 1.0f, 0.0f,
         0.0f, 0.0f, 1.0f
    };
    
    // P3-D65 to BT.2020 (identity for now)
    static constexpr float P3D65_TO_BT2020_MATRIX[9] = {
         1.0f, 0.0f, 0.0f,
         0.0f, 1.0f, 0.0f,
         0.0f, 0.0f, 1.0f
    };
    
    // BT.2020 to XYZ D65 (identity for now)
    static constexpr float BT2020_TO_XYZ_MATRIX[9] = {
         1.0f, 0.0f, 0.0f,
         0.0f, 1.0f, 0.0f,
         0.0f, 0.0f, 1.0f
    };
    
    // ACEScg transformation matrices (identity for now)
    static constexpr float BT2020_TO_ACESG_MATRIX[9] = {
         1.0f, 0.0f, 0.0f,
         0.0f, 1.0f, 0.0f,
         0.0f, 0.0f, 1.0f
    };
    
    static constexpr float ACESG_TO_BT2020_MATRIX[9] = {
         1.0f, 0.0f, 0.0f,
         0.0f, 1.0f, 0.0f,
         0.0f, 0.0f, 1.0f
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