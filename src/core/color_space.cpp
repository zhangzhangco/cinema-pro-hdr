#include "cinema_pro_hdr/color_space.h"
#include <cmath>
#include <algorithm>

namespace CinemaProHDR {

// PQ EOTF function (ST 2084)
float ColorSpaceConverter::PQ_EOTF(float pq_value) {
    // Handle edge cases
    if (!std::isfinite(pq_value) || pq_value <= 0.0f) return 0.0f;
    if (pq_value >= 1.0f) return 10000.0f;
    
    // ST 2084 EOTF formula
    float pq_pow = std::pow(pq_value, 1.0f / PQ_M2);
    float numerator = std::max(0.0f, pq_pow - PQ_C1);
    float denominator = PQ_C2 - PQ_C3 * pq_pow;
    
    if (denominator <= 0.0f) return 10000.0f;
    
    float linear = std::pow(numerator / denominator, 1.0f / PQ_M1);
    return linear * 10000.0f; // Convert to cd/m²
}

// PQ OETF function (ST 2084)
float ColorSpaceConverter::PQ_OETF(float linear_value) {
    // Handle edge cases
    if (!std::isfinite(linear_value) || linear_value <= 0.0f) return 0.0f;
    
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
    // Inverse of BT2020_TO_XYZ_MATRIX (computed using proper matrix inversion)
    static constexpr float XYZ_TO_BT2020_MATRIX[9] = {
         1.7166511f, -0.3556708f, -0.2533663f,
        -0.6666844f,  1.6164812f,  0.0157685f,
         0.0176399f, -0.0427706f,  0.9421031f
    };
    MultiplyMatrix3x3(XYZ_TO_BT2020_MATRIX, xyz, bt2020);
}

void ColorSpaceConverter::BT2020_to_ACEScg(const float* bt2020, float* acesg) {
    MultiplyMatrix3x3(BT2020_TO_ACESG_MATRIX, bt2020, acesg);
}

void ColorSpaceConverter::ACEScg_to_BT2020(const float* acesg, float* bt2020) {
    MultiplyMatrix3x3(ACESG_TO_BT2020_MATRIX, acesg, bt2020);
}

void ColorSpaceConverter::ToWorkingDomain(const Image& input, Image& output) {
    output = Image(input.width, input.height, input.channels);
    output.color_space = ColorSpace::BT2020_PQ;
    
    for (int y = 0; y < input.height; ++y) {
        for (int x = 0; x < input.width; ++x) {
            const float* src_pixel = input.GetPixel(x, y);
            float* dst_pixel = output.GetPixel(x, y);
            
            if (src_pixel && dst_pixel) {
                // Validate input pixel
                if (!NumericalUtils::IsFiniteRGB(src_pixel)) {
                    // Handle NaN/Inf input - set to black
                    dst_pixel[0] = dst_pixel[1] = dst_pixel[2] = 0.0f;
                    continue;
                }
                
                // Convert to working domain (BT.2020 + PQ normalized)
                switch (input.color_space) {
                    case ColorSpace::BT2020_PQ:
                        // Already in working domain
                        dst_pixel[0] = src_pixel[0];
                        dst_pixel[1] = src_pixel[1];
                        dst_pixel[2] = src_pixel[2];
                        break;
                        
                    case ColorSpace::P3_D65: {
                        // P3-D65 linear to BT.2020 linear
                        float bt2020_linear[3];
                        P3D65_to_BT2020(src_pixel, bt2020_linear);
                        
                        // Apply PQ OETF to get normalized PQ values
                        PQ_OETF_RGB(bt2020_linear, dst_pixel);
                        break;
                    }
                    
                    case ColorSpace::ACESG: {
                        // ACEScg to BT.2020 linear
                        float bt2020_linear[3];
                        ACEScg_to_BT2020(src_pixel, bt2020_linear);
                        
                        // Apply PQ OETF to get normalized PQ values
                        PQ_OETF_RGB(bt2020_linear, dst_pixel);
                        break;
                    }
                    
                    default:
                        // Fallback: assume already in correct format but validate
                        dst_pixel[0] = src_pixel[0];
                        dst_pixel[1] = src_pixel[1];
                        dst_pixel[2] = src_pixel[2];
                        break;
                }
                
                // Validate output and ensure values are in valid range [0, 1]
                if (!NumericalUtils::IsFiniteRGB(dst_pixel)) {
                    dst_pixel[0] = dst_pixel[1] = dst_pixel[2] = 0.0f;
                } else {
                    NumericalUtils::SaturateRGB(dst_pixel);
                }
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
                // Validate input pixel
                if (!NumericalUtils::IsFiniteRGB(src_pixel)) {
                    // Handle NaN/Inf input - set to black
                    dst_pixel[0] = dst_pixel[1] = dst_pixel[2] = 0.0f;
                    continue;
                }
                
                switch (target_cs) {
                    case ColorSpace::BT2020_PQ:
                        // Already in working domain
                        dst_pixel[0] = src_pixel[0];
                        dst_pixel[1] = src_pixel[1];
                        dst_pixel[2] = src_pixel[2];
                        break;
                        
                    case ColorSpace::P3_D65: {
                        // Apply PQ EOTF first to get linear BT.2020
                        float bt2020_linear[3];
                        PQ_EOTF_RGB(src_pixel, bt2020_linear);
                        
                        // BT.2020 linear to P3-D65 linear
                        BT2020_to_P3D65(bt2020_linear, dst_pixel);
                        break;
                    }
                    
                    case ColorSpace::ACESG: {
                        // Apply PQ EOTF first to get linear BT.2020
                        float bt2020_linear[3];
                        PQ_EOTF_RGB(src_pixel, bt2020_linear);
                        
                        // BT.2020 linear to ACEScg
                        BT2020_to_ACEScg(bt2020_linear, dst_pixel);
                        break;
                    }
                    
                    default:
                        // Fallback: direct copy with validation
                        dst_pixel[0] = src_pixel[0];
                        dst_pixel[1] = src_pixel[1];
                        dst_pixel[2] = src_pixel[2];
                        break;
                }
                
                // Validate output and clamp to target color space gamut
                if (!NumericalUtils::IsFiniteRGB(dst_pixel)) {
                    dst_pixel[0] = dst_pixel[1] = dst_pixel[2] = 0.0f;
                } else {
                    ClampToGamut(dst_pixel, target_cs);
                }
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

bool ColorSpaceConverter::IsInGamut(const float* rgb, ColorSpace cs) {
    // Check if RGB values are within valid gamut boundaries
    // For most color spaces, valid range is [0, 1] for normalized values
    
    switch (cs) {
        case ColorSpace::BT2020_PQ:
        case ColorSpace::P3_D65:
        case ColorSpace::REC709:
            // Standard gamuts: values should be in [0, 1] range
            return rgb[0] >= 0.0f && rgb[0] <= 1.0f &&
                   rgb[1] >= 0.0f && rgb[1] <= 1.0f &&
                   rgb[2] >= 0.0f && rgb[2] <= 1.0f;
                   
        case ColorSpace::ACESG:
            // ACEScg allows negative values for out-of-gamut colors
            // But we still check for reasonable bounds to detect errors
            return rgb[0] >= -0.5f && rgb[0] <= 2.0f &&
                   rgb[1] >= -0.5f && rgb[1] <= 2.0f &&
                   rgb[2] >= -0.5f && rgb[2] <= 2.0f;
                   
        default:
            return false;
    }
}

void ColorSpaceConverter::ClampToGamut(float* rgb, ColorSpace cs) {
    switch (cs) {
        case ColorSpace::BT2020_PQ:
        case ColorSpace::P3_D65:
        case ColorSpace::REC709:
            // Clamp to [0, 1] range
            rgb[0] = std::clamp(rgb[0], 0.0f, 1.0f);
            rgb[1] = std::clamp(rgb[1], 0.0f, 1.0f);
            rgb[2] = std::clamp(rgb[2], 0.0f, 1.0f);
            break;
            
        case ColorSpace::ACESG:
            // More permissive clamping for ACEScg
            rgb[0] = std::clamp(rgb[0], -0.5f, 2.0f);
            rgb[1] = std::clamp(rgb[1], -0.5f, 2.0f);
            rgb[2] = std::clamp(rgb[2], -0.5f, 2.0f);
            break;
            
        default:
            // Fallback: basic [0, 1] clamping
            NumericalUtils::SaturateRGB(rgb);
            break;
    }
}

float ColorSpaceConverter::GetGamutDistance(const float* rgb, ColorSpace cs) {
    // Calculate how far the color is outside the valid gamut
    // Returns 0.0 if in gamut, positive value if out of gamut
    
    float distance = 0.0f;
    
    switch (cs) {
        case ColorSpace::BT2020_PQ:
        case ColorSpace::P3_D65:
        case ColorSpace::REC709: {
            // Calculate distance from [0, 1] cube
            for (int i = 0; i < 3; ++i) {
                if (rgb[i] < 0.0f) {
                    distance += rgb[i] * rgb[i];  // Squared distance below 0
                } else if (rgb[i] > 1.0f) {
                    float excess = rgb[i] - 1.0f;
                    distance += excess * excess;  // Squared distance above 1
                }
            }
            break;
        }
        
        case ColorSpace::ACESG: {
            // Calculate distance from [-0.5, 2.0] cube
            for (int i = 0; i < 3; ++i) {
                if (rgb[i] < -0.5f) {
                    float deficit = rgb[i] + 0.5f;
                    distance += deficit * deficit;
                } else if (rgb[i] > 2.0f) {
                    float excess = rgb[i] - 2.0f;
                    distance += excess * excess;
                }
            }
            break;
        }
        
        default:
            // Fallback: use [0, 1] range
            for (int i = 0; i < 3; ++i) {
                if (rgb[i] < 0.0f) {
                    distance += rgb[i] * rgb[i];
                } else if (rgb[i] > 1.0f) {
                    float excess = rgb[i] - 1.0f;
                    distance += excess * excess;
                }
            }
            break;
    }
    
    return std::sqrt(distance);
}

bool ColorSpaceConverter::ValidateColorSpaceTransform(ColorSpace from, ColorSpace to) {
    // Validate that the transformation between color spaces is supported
    
    // All supported color spaces
    if (!IsValidColorSpace(from) || !IsValidColorSpace(to)) {
        return false;
    }
    
    // Same color space is always valid (identity transform)
    if (from == to) {
        return true;
    }
    
    // Check supported transformations
    // We support transformations through BT.2020 as intermediate
    switch (from) {
        case ColorSpace::BT2020_PQ:
            return to == ColorSpace::P3_D65 || to == ColorSpace::ACESG;
            
        case ColorSpace::P3_D65:
            return to == ColorSpace::BT2020_PQ;
            
        case ColorSpace::ACESG:
            return to == ColorSpace::BT2020_PQ;
            
        case ColorSpace::REC709:
            // REC709 support can be added later if needed
            return false;
            
        default:
            return false;
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

// OKLab conversion helper functions
float ColorSpaceConverter::CubeRoot(float x) {
    if (x >= 0.0f) {
        return std::pow(x, 1.0f / 3.0f);
    } else {
        return -std::pow(-x, 1.0f / 3.0f);
    }
}

float ColorSpaceConverter::CubePower(float x) {
    return x * x * x;
}

// RGB to OKLab conversion
void ColorSpaceConverter::RGB_to_OKLab(const float* rgb, float* oklab) {
    // 验证输入
    if (!NumericalUtils::IsFiniteRGB(rgb)) {
        oklab[0] = oklab[1] = oklab[2] = 0.0f;
        return;
    }
    
    // 第一步：RGB → LMS（线性光）
    float lms[3];
    MultiplyMatrix3x3(RGB_TO_LMS_MATRIX, rgb, lms);
    
    // 确保LMS值非负
    lms[0] = std::max(0.0f, lms[0]);
    lms[1] = std::max(0.0f, lms[1]);
    lms[2] = std::max(0.0f, lms[2]);
    
    // 第二步：LMS → LMS'（立方根）
    float lms_prime[3];
    lms_prime[0] = CubeRoot(lms[0]);
    lms_prime[1] = CubeRoot(lms[1]);
    lms_prime[2] = CubeRoot(lms[2]);
    
    // 第三步：LMS' → OKLab
    MultiplyMatrix3x3(LMS_TO_OKLAB_MATRIX, lms_prime, oklab);
    
    // 验证输出
    if (!NumericalUtils::IsFiniteRGB(oklab)) {
        oklab[0] = oklab[1] = oklab[2] = 0.0f;
    }
}

// OKLab to RGB conversion
void ColorSpaceConverter::OKLab_to_RGB(const float* oklab, float* rgb) {
    // 验证输入
    if (!NumericalUtils::IsFiniteRGB(oklab)) {
        rgb[0] = rgb[1] = rgb[2] = 0.0f;
        return;
    }
    
    // 第一步：OKLab → LMS'
    float lms_prime[3];
    MultiplyMatrix3x3(OKLAB_TO_LMS_MATRIX, oklab, lms_prime);
    
    // 第二步：LMS' → LMS（立方）
    float lms[3];
    lms[0] = CubePower(lms_prime[0]);
    lms[1] = CubePower(lms_prime[1]);
    lms[2] = CubePower(lms_prime[2]);
    
    // 第三步：LMS → RGB
    MultiplyMatrix3x3(LMS_TO_RGB_MATRIX, lms, rgb);
    
    // 验证输出
    if (!NumericalUtils::IsFiniteRGB(rgb)) {
        rgb[0] = rgb[1] = rgb[2] = 0.0f;
    }
}

// 应用基础饱和度调节（在OKLab空间中）
void ColorSpaceConverter::ApplyBaseSaturation(float* oklab, float saturation) {
    // 验证输入
    if (!NumericalUtils::IsFiniteRGB(oklab) || !NumericalUtils::IsFinite(saturation)) {
        return;
    }
    
    // 钳制饱和度参数到有效范围
    saturation = std::clamp(saturation, 0.0f, 2.0f);
    
    // 在OKLab中，a和b通道控制色度，L通道控制亮度
    // 饱和度调节：缩放a和b通道，保持L通道不变
    oklab[1] *= saturation;  // a通道
    oklab[2] *= saturation;  // b通道
    
    // L通道保持不变：oklab[0] = oklab[0]
}

// 应用高光区域饱和度调节
void ColorSpaceConverter::ApplyHighlightSaturation(float* oklab, float saturation, float weight) {
    // 验证输入
    if (!NumericalUtils::IsFiniteRGB(oklab) || 
        !NumericalUtils::IsFinite(saturation) || 
        !NumericalUtils::IsFinite(weight)) {
        return;
    }
    
    // 钳制参数到有效范围
    saturation = std::clamp(saturation, 0.0f, 2.0f);
    weight = std::clamp(weight, 0.0f, 1.0f);
    
    // 计算高光饱和度的增量效果
    // 当前饱和度 = 1.0（基础），目标饱和度 = saturation
    // 应用权重混合：current + weight * (target - current)
    float target_a = oklab[1] * saturation;
    float target_b = oklab[2] * saturation;
    
    oklab[1] = NumericalUtils::Mix(oklab[1], target_a, weight);
    oklab[2] = NumericalUtils::Mix(oklab[2], target_b, weight);
}

// 主要的饱和度处理函数
void ColorSpaceConverter::ApplySaturation(float* rgb, float sat_base, float sat_hi, float pivot_pq, float x_luminance) {
    // 验证输入参数
    if (!NumericalUtils::IsFiniteRGB(rgb) || 
        !NumericalUtils::IsFinite(sat_base) || 
        !NumericalUtils::IsFinite(sat_hi) ||
        !NumericalUtils::IsFinite(pivot_pq) ||
        !NumericalUtils::IsFinite(x_luminance)) {
        return;
    }
    
    // 钳制参数到有效范围
    sat_base = std::clamp(sat_base, 0.0f, 2.0f);
    sat_hi = std::clamp(sat_hi, 0.0f, 2.0f);
    pivot_pq = std::clamp(pivot_pq, 0.05f, 0.30f);
    x_luminance = std::clamp(x_luminance, 0.0f, 1.0f);
    
    // 转换到OKLab色彩空间
    float oklab[3];
    RGB_to_OKLab(rgb, oklab);
    
    // 应用基础饱和度调节（全局）
    ApplyBaseSaturation(oklab, sat_base);
    
    // 计算高光区域权重：w_hi = smoothstep(p, 1, x)
    float w_hi = NumericalUtils::SmoothStep(pivot_pq, 1.0f, x_luminance);
    
    // 应用高光区域饱和度调节
    ApplyHighlightSaturation(oklab, sat_hi, w_hi);
    
    // 转换回RGB
    OKLab_to_RGB(oklab, rgb);
    
    // 确保输出值有效
    if (!NumericalUtils::IsFiniteRGB(rgb)) {
        rgb[0] = rgb[1] = rgb[2] = 0.0f;
    }
}

// 线性色域压制（第一级处理）
void ColorSpaceConverter::LinearGamutCompression(float* rgb, ColorSpace target_cs) {
    // 验证输入
    if (!NumericalUtils::IsFiniteRGB(rgb)) {
        return;
    }
    
    // 根据目标色彩空间应用线性压制
    switch (target_cs) {
        case ColorSpace::P3_D65:
        case ColorSpace::BT2020_PQ:
        case ColorSpace::REC709: {
            // 对于标准色域，使用简单的线性压制
            // 找到最大的越界值
            float max_val = std::max({rgb[0], rgb[1], rgb[2]});
            if (max_val > 1.0f) {
                // 线性缩放到[0,1]范围
                float scale = 1.0f / max_val;
                rgb[0] *= scale;
                rgb[1] *= scale;
                rgb[2] *= scale;
            }
            
            // 处理负值
            rgb[0] = std::max(0.0f, rgb[0]);
            rgb[1] = std::max(0.0f, rgb[1]);
            rgb[2] = std::max(0.0f, rgb[2]);
            break;
        }
        
        case ColorSpace::ACESG: {
            // ACEScg允许更宽的范围，使用更宽松的压制
            float scale = 1.0f;
            float max_val = std::max({rgb[0], rgb[1], rgb[2]});
            if (max_val > 2.0f) {
                scale = 2.0f / max_val;
                rgb[0] *= scale;
                rgb[1] *= scale;
                rgb[2] *= scale;
            }
            
            // 处理过度负值
            rgb[0] = std::max(-0.5f, rgb[0]);
            rgb[1] = std::max(-0.5f, rgb[1]);
            rgb[2] = std::max(-0.5f, rgb[2]);
            break;
        }
        
        default:
            // 默认处理：基本钳制
            NumericalUtils::SaturateRGB(rgb);
            break;
    }
}

// 感知色域夹持（第二级处理，在OKLab中进行）
void ColorSpaceConverter::PerceptualGamutClamp(float* rgb, ColorSpace target_cs) {
    // 验证输入
    if (!NumericalUtils::IsFiniteRGB(rgb)) {
        return;
    }
    
    // 转换到OKLab进行感知均匀的处理
    float oklab[3];
    RGB_to_OKLab(rgb, oklab);
    
    // 在OKLab空间中进行感知夹持
    // 保持亮度L不变，调整色度a,b使其回到有效色域
    
    // 迭代方法：逐步减少色度直到回到色域内
    const int max_iterations = 10;
    const float reduction_factor = 0.9f;
    
    for (int i = 0; i < max_iterations; ++i) {
        // 转换回RGB检查是否在色域内
        float test_rgb[3];
        OKLab_to_RGB(oklab, test_rgb);
        
        if (IsInGamut(test_rgb, target_cs)) {
            // 已经在色域内，使用当前值
            rgb[0] = test_rgb[0];
            rgb[1] = test_rgb[1];
            rgb[2] = test_rgb[2];
            return;
        }
        
        // 减少色度（a, b通道）
        oklab[1] *= reduction_factor;
        oklab[2] *= reduction_factor;
    }
    
    // 如果迭代后仍然越界，使用最终的钳制
    float final_rgb[3];
    OKLab_to_RGB(oklab, final_rgb);
    ClampToGamut(final_rgb, target_cs);
    
    rgb[0] = final_rgb[0];
    rgb[1] = final_rgb[1];
    rgb[2] = final_rgb[2];
}

// 两级色域处理的主函数
bool ColorSpaceConverter::ApplyGamutProcessing(float* rgb, ColorSpace target_cs, bool dci_compliance) {
    // 验证输入
    if (!NumericalUtils::IsFiniteRGB(rgb)) {
        return false;
    }
    
    // 记录原始值以检测是否发生了越界
    float original_rgb[3] = {rgb[0], rgb[1], rgb[2]};
    bool was_out_of_gamut = !IsInGamut(rgb, target_cs);
    
    // 第一级：线性压制（3×3矩阵方法）
    LinearGamutCompression(rgb, target_cs);
    
    // 第二级：感知夹持（OKLab方法）
    // 在DCI合规模式下总是启用，否则仅在仍然越界时启用
    if (dci_compliance || !IsInGamut(rgb, target_cs)) {
        PerceptualGamutClamp(rgb, target_cs);
        
        // 在DCI模式下，对sat_hi应用保守衰减（5-10%）
        if (dci_compliance && was_out_of_gamut) {
            // 这里可以通过减少饱和度来进一步保守处理
            // 但这需要在调用此函数之前处理，因为我们在这里已经是RGB域了
        }
    }
    
    // 最终验证和安全钳制
    if (!NumericalUtils::IsFiniteRGB(rgb)) {
        rgb[0] = rgb[1] = rgb[2] = 0.0f;
        return false;
    }
    
    // 确保最终结果在色域内
    ClampToGamut(rgb, target_cs);
    
    return was_out_of_gamut;  // 返回是否发生了越界处理
}

} // namespace CinemaProHDR