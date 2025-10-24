#include "cinema_pro_hdr/color_space.h"
#include <cmath>
#include <algorithm>

namespace CinemaProHDR {

// PQ EOTF function (ST 2084)
float ColorSpaceConverter::PQ_EOTF(float pq_value) {
    if (pq_value <= 0.0f) return 0.0f;
    if (pq_value >= 1.0f) return 10000.0f;
    
    float pq_pow = std::pow(pq_value, 1.0f / PQ_M2);
    float numerator = std::max(0.0f, pq_pow - PQ_C1);
    float denominator = PQ_C2 - PQ_C3 * pq_pow;
    
    if (denominator <= 0.0f) return 10000.0f;
    
    float linear = std::pow(numerator / denominator, 1.0f / PQ_M1);
    return linear * 10000.0f; // Convert to cd/m²
}

// PQ OETF function (ST 2084)
float ColorSpaceConverter::PQ_OETF(float linear_value) {
    if (linear_value <= 0.0f) return 0.0f;
    
    float normalized = linear_value / 10000.0f; // Normalize from cd/m²
    if (normalized >= 1.0f) return 1.0f;
    
    float pow_m1 = std::pow(normalized, PQ_M1);
    float numerator = PQ_C1 + PQ_C2 * pow_m1;
    float denominator = 1.0f + PQ_C3 * pow_m1;
    
    if (denominator <= 0.0f) return 1.0f;
    
    return std::pow(numerator / denominator, PQ_M2);
}

void ColorSpaceConverter::PQ_EOTF_RGB(const float* pq_rgb, float* linear_rgb) {
    linear_rgb[0] = PQ_EOTF(pq_rgb[0]);
    linear_rgb[1] = PQ_EOTF(pq_rgb[1]);
    linear_rgb[2] = PQ_EOTF(pq_rgb[2]);
}

void ColorSpaceConverter::PQ_OETF_RGB(const float* linear_rgb, float* pq_rgb) {
    pq_rgb[0] = PQ_OETF(linear_rgb[0]);
    pq_rgb[1] = PQ_OETF(linear_rgb[1]);
    pq_rgb[2] = PQ_OETF(linear_rgb[2]);
}

void ColorSpaceConverter::MultiplyMatrix3x3(const float* matrix, const float* input, float* output) {
    output[0] = matrix[0] * input[0] + matrix[1] * input[1] + matrix[2] * input[2];
    output[1] = matrix[3] * input[0] + matrix[4] * input[1] + matrix[5] * input[2];
    output[2] = matrix[6] * input[0] + matrix[7] * input[1] + matrix[8] * input[2];
}

void ColorSpaceConverter::BT2020_to_P3D65(const float* bt2020, float* p3d65) {
    MultiplyMatrix3x3(BT2020_TO_P3D65_MATRIX, bt2020, p3d65);
}

void ColorSpaceConverter::P3D65_to_BT2020(const float* p3d65, float* bt2020) {
    MultiplyMatrix3x3(P3D65_TO_BT2020_MATRIX, p3d65, bt2020);
}

void ColorSpaceConverter::BT2020_to_XYZ(const float* bt2020, float* xyz) {
    MultiplyMatrix3x3(BT2020_TO_XYZ_MATRIX, bt2020, xyz);
}

void ColorSpaceConverter::XYZ_to_BT2020(const float* xyz, float* bt2020) {
    // Inverse of BT2020_TO_XYZ_MATRIX
    static constexpr float XYZ_TO_BT2020_MATRIX[9] = {
         1.7166511f, -0.3556708f, -0.2533663f,
        -0.6666844f,  1.6164812f,  0.0157685f,
         0.0176399f, -0.0427706f,  0.9421031f
    };
    MultiplyMatrix3x3(XYZ_TO_BT2020_MATRIX, xyz, bt2020);
}

void ColorSpaceConverter::ToWorkingDomain(const Image& input, Image& output) {
    output = Image(input.width, input.height, input.channels);
    output.color_space = ColorSpace::BT2020_PQ;
    
    for (int y = 0; y < input.height; ++y) {
        for (int x = 0; x < input.width; ++x) {
            const float* src_pixel = input.GetPixel(x, y);
            float* dst_pixel = output.GetPixel(x, y);
            
            if (src_pixel && dst_pixel) {
                // Convert to working domain (BT.2020 + PQ normalized)
                switch (input.color_space) {
                    case ColorSpace::BT2020_PQ:
                        // Already in working domain
                        dst_pixel[0] = src_pixel[0];
                        dst_pixel[1] = src_pixel[1];
                        dst_pixel[2] = src_pixel[2];
                        break;
                        
                    case ColorSpace::P3_D65: {
                        // P3-D65 to BT.2020
                        float bt2020_rgb[3];
                        P3D65_to_BT2020(src_pixel, bt2020_rgb);
                        
                        // Apply PQ OETF
                        PQ_OETF_RGB(bt2020_rgb, dst_pixel);
                        break;
                    }
                    
                    default:
                        // Fallback: assume already in correct format
                        dst_pixel[0] = src_pixel[0];
                        dst_pixel[1] = src_pixel[1];
                        dst_pixel[2] = src_pixel[2];
                        break;
                }
                
                // Ensure values are in valid range
                NumericalUtils::SaturateRGB(dst_pixel);
            }
        }
    }
}

void ColorSpaceConverter::FromWorkingDomain(const Image& input, Image& output, ColorSpace target_cs) {
    output = Image(input.width, input.height, input.channels);
    output.color_space = target_cs;
    
    for (int y = 0; y < input.height; ++y) {
        for (int x = 0; x < input.width; ++x) {
            const float* src_pixel = input.GetPixel(x, y);
            float* dst_pixel = output.GetPixel(x, y);
            
            if (src_pixel && dst_pixel) {
                switch (target_cs) {
                    case ColorSpace::BT2020_PQ:
                        // Already in working domain
                        dst_pixel[0] = src_pixel[0];
                        dst_pixel[1] = src_pixel[1];
                        dst_pixel[2] = src_pixel[2];
                        break;
                        
                    case ColorSpace::P3_D65: {
                        // Apply PQ EOTF first
                        float linear_rgb[3];
                        PQ_EOTF_RGB(src_pixel, linear_rgb);
                        
                        // BT.2020 to P3-D65
                        BT2020_to_P3D65(linear_rgb, dst_pixel);
                        break;
                    }
                    
                    default:
                        // Fallback: direct copy
                        dst_pixel[0] = src_pixel[0];
                        dst_pixel[1] = src_pixel[1];
                        dst_pixel[2] = src_pixel[2];
                        break;
                }
                
                // Ensure values are in valid range
                NumericalUtils::SaturateRGB(dst_pixel);
            }
        }
    }
}

bool ColorSpaceConverter::IsValidColorSpace(ColorSpace cs) {
    switch (cs) {
        case ColorSpace::BT2020_PQ:
        case ColorSpace::P3_D65:
        case ColorSpace::ACESG:
        case ColorSpace::REC709:
            return true;
        default:
            return false;
    }
}

std::string ColorSpaceConverter::ColorSpaceToString(ColorSpace cs) {
    switch (cs) {
        case ColorSpace::BT2020_PQ: return "BT2020_PQ";
        case ColorSpace::P3_D65: return "P3_D65";
        case ColorSpace::ACESG: return "ACEScg";
        case ColorSpace::REC709: return "REC709";
        default: return "UNKNOWN";
    }
}

// Numerical utility functions
bool NumericalUtils::IsFinite(float value) {
    return std::isfinite(value);
}

bool NumericalUtils::IsFiniteRGB(const float* rgb) {
    return IsFinite(rgb[0]) && IsFinite(rgb[1]) && IsFinite(rgb[2]);
}

void NumericalUtils::SaturateRGB(float* rgb) {
    rgb[0] = std::clamp(rgb[0], 0.0f, 1.0f);
    rgb[1] = std::clamp(rgb[1], 0.0f, 1.0f);
    rgb[2] = std::clamp(rgb[2], 0.0f, 1.0f);
}

float NumericalUtils::SafePow(float base, float exponent) {
    if (base <= 0.0f) return 0.0f;
    if (!IsFinite(base) || !IsFinite(exponent)) return 0.0f;
    
    float result = std::pow(base, exponent);
    return IsFinite(result) ? result : 0.0f;
}

float NumericalUtils::SafeLog(float value) {
    if (value <= 0.0f) return -10.0f; // Safe minimum
    if (!IsFinite(value)) return 0.0f;
    
    float result = std::log(value);
    return IsFinite(result) ? result : 0.0f;
}

float NumericalUtils::SafeDivide(float numerator, float denominator, float fallback) {
    if (std::abs(denominator) < 1e-8f) return fallback;
    if (!IsFinite(numerator) || !IsFinite(denominator)) return fallback;
    
    float result = numerator / denominator;
    return IsFinite(result) ? result : fallback;
}

float NumericalUtils::SmoothStep(float edge0, float edge1, float x) {
    if (edge1 <= edge0) return x >= edge1 ? 1.0f : 0.0f;
    
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float NumericalUtils::Mix(float a, float b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return a * (1.0f - t) + b * t;
}

bool NumericalUtils::IsInRange(float value, float min_val, float max_val) {
    return value >= min_val && value <= max_val;
}

float NumericalUtils::ClampToRange(float value, float min_val, float max_val) {
    return std::clamp(value, min_val, max_val);
}

} // namespace CinemaProHDR