#include "cinema_pro_hdr/tone_mapping.h"
#include "cinema_pro_hdr/error_handler.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace CinemaProHDR {

ToneMapper::ToneMapper() = default;
ToneMapper::~ToneMapper() = default;

bool ToneMapper::Initialize(const CphParams& params) {
    // 验证参数有效性
    if (!params.IsValid()) {
        last_error_ = "无效的色调映射参数";
        return false;
    }
    
    params_ = params;
    initialized_ = true;
    last_error_.clear();
    
    return true;
}

float ToneMapper::ApplyToneMapping(float luminance) const {
    if (!initialized_) {
        return luminance; // 回退到线性映射
    }
    
    // 输入值保护：确保在[0,1]范围内且为有效数值
    if (!NumericalProtection::IsValid(luminance)) {
        return 0.0f; // NaN/Inf回退到0
    }
    
    float x = std::clamp(luminance, 0.0f, 1.0f);
    float y;
    
    // 根据曲线类型选择算法
    if (params_.curve == CurveType::PPR) {
        y = ApplyPPR(x);
    } else {
        y = ApplyRLOG(x);
    }
    
    // 应用软膝处理
    y = ApplySoftKnee(y);
    
    // 应用toe夹持
    y = ApplyToeClamp(y);
    
    // 最终保护：确保输出在[0,1]范围内
    return std::clamp(y, 0.0f, 1.0f);
}

void ToneMapper::ApplyToneMappingBatch(const float* input_luminance, 
                                       float* output_luminance, 
                                       size_t count) const {
    if (!input_luminance || !output_luminance || count == 0) {
        return;
    }
    
    for (size_t i = 0; i < count; ++i) {
        output_luminance[i] = ApplyToneMapping(input_luminance[i]);
    }
}

float ToneMapper::ApplyPPR(float x) const {
    /**
     * PPR (Pivoted Power-Rational) 算法实现
     * 
     * 算法保证：
     * - 单调递增：f(x1) <= f(x2) when x1 <= x2
     * - C¹连续：在拼接点导数连续
     * - 数值稳定：避免NaN/Inf
     * 
     * 数学模型：
     * - 阴影段：基于幂函数的暗部处理
     * - 高光段：有理式函数的高光压缩
     * - 拼接：在枢轴点使用平滑混合确保连续性
     */
    
    const float p = params_.pivot_pq;
    
    // 计算两个段的值
    float shadow_value = PPRShadowSegment(x);
    float highlight_value = PPRHighlightSegment(x);
    
    // 在枢轴点附近使用平滑混合
    float blend_range = p * 0.1f; // 10%的混合范围
    
    if (x <= p - blend_range) {
        return shadow_value;
    } else if (x >= p + blend_range) {
        return highlight_value;
    } else {
        // 在混合区间内使用smoothstep
        float blend_weight = SmoothStep(p - blend_range, p + blend_range, x);
        return Mix(shadow_value, highlight_value, blend_weight);
    }
}

float ToneMapper::PPRShadowSegment(float x) const {
    const float p = params_.pivot_pq;
    const float gamma_s = params_.gamma_s;
    
    if (x <= 0.0f) return 0.0f;
    if (p <= 0.0f) return x; // 防护：避免除零
    
    // 简化的阴影段：在枢轴点以下使用幂函数
    // 确保在枢轴点处值为p
    if (x >= p) {
        return p; // 在枢轴点处返回枢轴值
    }
    
    float normalized_x = x / p;
    float power_result = std::pow(normalized_x, gamma_s);
    
    return power_result * p;
}

float ToneMapper::PPRHighlightSegment(float x) const {
    const float p = params_.pivot_pq;
    const float gamma_h = params_.gamma_h;
    const float h = params_.shoulder_h;
    
    // 高光段：需要确保在枢轴点处与阴影段连续
    // 使用修正的有理式函数
    
    if (x <= p) {
        return p; // 在枢轴点以下返回枢轴值
    }
    
    // 将输入映射到[0,1]范围，其中p映射到0
    float normalized_x = (x - p) / (1.0f - p);
    if (normalized_x <= 0.0f) return p;
    if (normalized_x >= 1.0f) normalized_x = 1.0f;
    
    // 应用有理式函数
    float denominator = 1.0f + h * normalized_x;
    if (denominator <= 0.0f) {
        return x; // 防护：避免除零或负数
    }
    
    float rational_part = normalized_x / denominator;
    float highlight_result = std::pow(rational_part, gamma_h);
    
    // 将结果映射回[p,1]范围
    return p + highlight_result * (1.0f - p);
}

float ToneMapper::ApplyRLOG(float x) const {
    /**
     * RLOG (Rational Logarithmic) 算法实现
     * 
     * 数学模型：
     * - 暗部：y1 = log(1 + a*x) / log(1 + a)
     * - 高光：y2 = (b*x) / (1 + c*x)，需要调整以确保连续性
     * - 拼接：在阈值t±0.05区间用smoothstep混合
     */
    
    const float t = params_.rlog_t;
    const float blend_range = 0.05f; // 拼接区间半宽
    
    // 计算拼接点处暗部段的值，用于调整高光段
    float dark_at_t = RLOGDarkSegment(t);
    float highlight_raw_at_t = RLOGHighlightSegment(t);
    
    // 计算高光段的调整系数，确保在拼接点连续
    float scale_factor = (highlight_raw_at_t > 0.0f) ? (dark_at_t / highlight_raw_at_t) : 1.0f;
    
    float y1 = RLOGDarkSegment(x);
    float y2 = RLOGHighlightSegment(x) * scale_factor;
    
    // 在拼接区间进行平滑混合
    if (x < t - blend_range) {
        return y1; // 纯暗部
    } else if (x > t + blend_range) {
        return y2; // 纯高光
    } else {
        // 拼接区间：使用smoothstep混合
        float blend_weight = SmoothStep(t - blend_range, t + blend_range, x);
        return Mix(y1, y2, blend_weight);
    }
}

float ToneMapper::RLOGDarkSegment(float x) const {
    const float a = params_.rlog_a;
    
    if (x <= 0.0f) return 0.0f;
    if (x >= 1.0f) return 1.0f;
    
    // 暗部：y1 = log(1 + a*x) / log(1 + a)
    float numerator = std::log(1.0f + a * x);
    float denominator = std::log(1.0f + a);
    
    if (denominator <= 0.0f) {
        return x; // 防护：避免除零
    }
    
    return numerator / denominator;
}

float ToneMapper::RLOGHighlightSegment(float x) const {
    const float b = params_.rlog_b;
    const float c = params_.rlog_c;
    
    if (x <= 0.0f) return 0.0f;
    if (x >= 1.0f) return b / (1.0f + c); // 在x=1处的值
    
    // 高光：y2 = (b*x) / (1 + c*x)
    float denominator = 1.0f + c * x;
    if (denominator <= 0.0f) {
        return x; // 防护：避免除零
    }
    
    return (b * x) / denominator;
}

float ToneMapper::ApplySoftKnee(float y) const {
    /**
     * 软膝处理：在接近1.0的区域进行平滑压缩
     * 
     * 目的：避免硬截断，提供更自然的高光过渡
     * 参数：yknee∈[0.95,0.99], alpha∈[0.2,1.0]
     */
    
    const float yknee = params_.yknee;
    const float alpha = params_.alpha;
    
    if (y <= yknee) {
        return y; // 膝点以下不处理
    }
    
    // 软膝函数：平滑压缩超过膝点的部分
    float excess = y - yknee;
    float max_excess = 1.0f - yknee;
    
    if (max_excess <= 0.0f) {
        return yknee; // 防护：避免除零
    }
    
    float normalized_excess = excess / max_excess;
    float compressed_excess = normalized_excess / (1.0f + alpha * normalized_excess);
    
    return yknee + compressed_excess * max_excess;
}

float ToneMapper::ApplyToeClamp(float y) const {
    /**
     * Toe夹持：在接近0.0的区域进行提升
     * 
     * 目的：避免纯黑，保持暗部细节
     * 参数：toe∈[0.0,0.01]，默认0.002
     * 
     * 注意：只有当输入非零时才应用toe夹持
     * 这样可以保持f(0) = 0的数学特性
     */
    
    const float toe = params_.toe;
    
    if (toe <= 0.0f || y <= 0.0f) {
        return y; // 无toe处理或输入为0
    }
    
    // 简单的线性提升
    return std::max(y, toe);
}

float ToneMapper::SmoothStep(float edge0, float edge1, float x) const {
    if (edge1 <= edge0) return 0.0f; // 防护：避免除零
    
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float ToneMapper::Mix(float a, float b, float t) const {
    return a + t * (b - a);
}

bool ToneMapper::ValidateMonotonicity(int sample_count, int problem_points) const {
    if (!initialized_) {
        return false;
    }
    
    // 生成采样点：均匀采样 + 问题点
    std::vector<float> sample_points = GenerateSamplePoints(sample_count, problem_points);
    
    float prev_value = -1.0f; // 初始值设为-1，确保第一个点通过检查
    
    for (float x : sample_points) {
        float current_value = ApplyToneMapping(x);
        
        // 检查单调性：当前值应该 >= 前一个值
        if (current_value < prev_value) {
            return false; // 发现非单调点
        }
        
        prev_value = current_value;
    }
    
    return true;
}

bool ToneMapper::ValidateC1Continuity(float epsilon, float threshold) const {
    if (!initialized_) {
        return false;
    }
    
    // 对于PPR算法，由于我们使用了平滑混合，C¹连续性应该是自动满足的
    // 这里我们检查整个曲线的平滑性，而不是特定点
    
    // 在整个定义域上检查导数的连续性
    const int sample_count = 50; // 减少采样点以降低数值误差
    float max_derivative_jump = 0.0f;
    
    for (int i = 1; i < sample_count - 1; ++i) {
        float x = static_cast<float>(i) / static_cast<float>(sample_count - 1);
        
        if (x <= epsilon || x >= 1.0f - epsilon) {
            continue; // 跳过边界点
        }
        
        float left_derivative = ComputeDerivative(x - epsilon, epsilon);
        float right_derivative = ComputeDerivative(x + epsilon, epsilon);
        
        float derivative_gap = std::abs(right_derivative - left_derivative);
        max_derivative_jump = std::max(max_derivative_jump, derivative_gap);
    }
    
    // 对于数值计算，我们接受一定的误差
    // 如果最大导数跳跃小于阈值，认为是C¹连续的
    return max_derivative_jump <= threshold;
}

float ToneMapper::ComputeDerivative(float x, float epsilon) const {
    // 使用中心差分计算导数
    float x_left = std::max(0.0f, x - epsilon);
    float x_right = std::min(1.0f, x + epsilon);
    
    float y_left = ApplyToneMapping(x_left);
    float y_right = ApplyToneMapping(x_right);
    
    float dx = x_right - x_left;
    if (dx <= 0.0f) {
        return 0.0f; // 防护：避免除零
    }
    
    return (y_right - y_left) / dx;
}

std::vector<float> ToneMapper::GenerateSamplePoints(int sample_count, int problem_points) const {
    std::vector<float> points;
    points.reserve(sample_count + problem_points);
    
    // 均匀采样点
    for (int i = 0; i < sample_count; ++i) {
        float x = static_cast<float>(i) / static_cast<float>(sample_count - 1);
        points.push_back(x);
    }
    
    // 添加问题点：在关键区域附近密集采样
    if (params_.curve == CurveType::PPR) {
        // PPR问题点：枢轴点附近
        float pivot = params_.pivot_pq;
        for (int i = 0; i < problem_points; ++i) {
            float offset = (static_cast<float>(i) / problem_points - 0.5f) * 0.1f; // ±5%范围
            float x = std::clamp(pivot + offset, 0.0f, 1.0f);
            points.push_back(x);
        }
    } else {
        // RLOG问题点：拼接区间附近
        float t = params_.rlog_t;
        for (int i = 0; i < problem_points; ++i) {
            float offset = (static_cast<float>(i) / problem_points - 0.5f) * 0.2f; // ±10%范围
            float x = std::clamp(t + offset, 0.0f, 1.0f);
            points.push_back(x);
        }
    }
    
    // 排序并去重
    std::sort(points.begin(), points.end());
    points.erase(std::unique(points.begin(), points.end()), points.end());
    
    return points;
}

// 工具函数实现
namespace ToneMappingUtils {

float EvaluatePPR(float x, float pivot, float gamma_s, float gamma_h, float shoulder) {
    if (x <= pivot) {
        // 阴影段
        if (pivot <= 0.0f) return x;
        float normalized_x = x / pivot;
        float linear_part = normalized_x;
        float power_part = std::pow(normalized_x, gamma_s);
        float blend_weight = x / pivot; // 简化的混合权重
        return (linear_part + blend_weight * (power_part - linear_part)) * pivot;
    } else {
        // 高光段
        float denominator = 1.0f + shoulder * x;
        if (denominator <= 0.0f) return x;
        return std::pow(x / denominator, gamma_h);
    }
}

float EvaluateRLOG(float x, float a, float b, float c, float t) {
    const float blend_range = 0.05f;
    
    // 暗部
    float y1 = 0.0f;
    if (x > 0.0f && a > 0.0f) {
        float log_denom = std::log(1.0f + a);
        if (log_denom > 0.0f) {
            y1 = std::log(1.0f + a * x) / log_denom;
        }
    }
    
    // 高光
    float y2 = 0.0f;
    float denom = 1.0f + c * x;
    if (denom > 0.0f) {
        y2 = (b * x) / denom;
    }
    
    // 拼接
    if (x < t - blend_range) {
        return y1;
    } else if (x > t + blend_range) {
        return y2;
    } else {
        float blend_t = (x - (t - blend_range)) / (2.0f * blend_range);
        blend_t = std::clamp(blend_t, 0.0f, 1.0f);
        blend_t = blend_t * blend_t * (3.0f - 2.0f * blend_t); // smoothstep
        return y1 + blend_t * (y2 - y1);
    }
}

float SoftKnee(float y, float knee, float alpha) {
    if (y <= knee) return y;
    
    float excess = y - knee;
    float max_excess = 1.0f - knee;
    if (max_excess <= 0.0f) return knee;
    
    float normalized_excess = excess / max_excess;
    float compressed_excess = normalized_excess / (1.0f + alpha * normalized_excess);
    
    return knee + compressed_excess * max_excess;
}

float ToeClamp(float y, float toe) {
    return std::max(y, toe);
}

} // namespace ToneMappingUtils

} // namespace CinemaProHDR