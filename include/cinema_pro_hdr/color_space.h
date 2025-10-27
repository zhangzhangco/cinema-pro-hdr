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
    
    // OKLab color space functions
    static void RGB_to_OKLab(const float* rgb, float* oklab);
    static void OKLab_to_RGB(const float* oklab, float* rgb);
    
    // Saturation processing in OKLab
    static void ApplySaturation(float* rgb, float sat_base, float sat_hi, float pivot_pq, float x_luminance);
    static void ApplyBaseSaturation(float* oklab, float saturation);
    static void ApplyHighlightSaturation(float* oklab, float saturation, float weight);
    
    // Two-level gamut handling
    static bool ApplyGamutProcessing(float* rgb, ColorSpace target_cs, bool dci_compliance);
    static void LinearGamutCompression(float* rgb, ColorSpace target_cs);
    static void PerceptualGamutClamp(float* rgb, ColorSpace target_cs);
    
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
    
    // OKLab conversion matrices and constants
    static constexpr float RGB_TO_LMS_MATRIX[9] = {
        0.4122214708f, 0.5363325363f, 0.0514459929f,
        0.2119034982f, 0.6806995451f, 0.1073969566f,
        0.0883024619f, 0.2817188376f, 0.6299787005f
    };
    
    static constexpr float LMS_TO_RGB_MATRIX[9] = {
         4.0767416621f, -3.3077115913f,  0.2309699292f,
        -1.2684380046f,  2.6097574011f, -0.3413193965f,
        -0.0041960863f, -0.7034186147f,  1.7076147010f
    };
    
    static constexpr float LMS_TO_OKLAB_MATRIX[9] = {
        0.2104542553f, 0.7936177850f, -0.0040720468f,
        1.9779984951f, -2.4285922050f, 0.4505937099f,
        0.0259040371f, 0.7827717662f, -0.8086757660f
    };
    
    static constexpr float OKLAB_TO_LMS_MATRIX[9] = {
        0.99999999845051981432f, 0.39633779217376785678f, 0.21580375806075880339f,
        1.0000000088817607767f, -0.1055613423236563494f, -0.063854174771705903402f,
        1.0000000546724109177f, -0.089484182094965759684f, -1.2914855378640917399f
    };
    
    // Helper functions for OKLab conversion
    static float CubeRoot(float x);
    static float CubePower(float x);
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