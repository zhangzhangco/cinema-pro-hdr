#include "cinema_pro_hdr/processor.h"
#include "cinema_pro_hdr/color_space.h"
#include "cinema_pro_hdr/tone_mapping.h"
#include "cinema_pro_hdr/highlight_detail.h"
#include <vector>
#include <mutex>
#include <algorithm>

namespace CinemaProHDR {

// Implementation details (PIMPL pattern)
struct CphProcessor::Impl {
    CphParams current_params;
    Statistics current_stats;
    std::vector<ErrorReport> error_history;
    std::string last_error;
    std::mutex stats_mutex;
    std::mutex error_mutex;
    bool initialized = false;
    
    // 色调映射器
    ToneMapper tone_mapper;
    
    // 高光细节处理器
    HighlightDetailProcessor highlight_processor;
    
    void LogError(ErrorCode code, const std::string& message, 
                  const std::string& field = "", float value = 0.0f) {
        std::lock_guard<std::mutex> lock(error_mutex);
        ErrorReport error(code, message);
        error.field_name = field;
        error.invalid_value = value;
        error_history.push_back(error);
        last_error = error.ToString();
    }
    
    void UpdateStatistics(const Image& processed_frame) {
        std::lock_guard<std::mutex> lock(stats_mutex);
        
        // Calculate PQ statistics
        std::vector<float> max_rgb_values;
        max_rgb_values.reserve(processed_frame.width * processed_frame.height);
        
        for (int y = 0; y < processed_frame.height; ++y) {
            for (int x = 0; x < processed_frame.width; ++x) {
                const float* pixel = processed_frame.GetPixel(x, y);
                if (pixel && NumericalUtils::IsFiniteRGB(pixel)) {
                    float max_rgb = std::max(pixel[0], std::max(pixel[1], pixel[2]));
                    max_rgb_values.push_back(max_rgb);
                }
            }
        }
        
        if (!max_rgb_values.empty()) {
            // Sort for percentile calculation
            std::sort(max_rgb_values.begin(), max_rgb_values.end());
            
            // Calculate 1% trimmed statistics
            size_t trim_count = max_rgb_values.size() / 100; // 1%
            size_t start_idx = trim_count;
            size_t end_idx = max_rgb_values.size() - trim_count;
            
            if (start_idx < end_idx) {
                current_stats.pq_stats.min_pq = max_rgb_values[start_idx];
                current_stats.pq_stats.max_pq = max_rgb_values[end_idx - 1];
                
                // Calculate trimmed mean
                float sum = 0.0f;
                for (size_t i = start_idx; i < end_idx; ++i) {
                    sum += max_rgb_values[i];
                }
                current_stats.pq_stats.avg_pq = sum / (end_idx - start_idx);
                
                // Calculate variance
                float variance_sum = 0.0f;
                for (size_t i = start_idx; i < end_idx; ++i) {
                    float diff = max_rgb_values[i] - current_stats.pq_stats.avg_pq;
                    variance_sum += diff * diff;
                }
                current_stats.pq_stats.variance = variance_sum / (end_idx - start_idx);
            }
        }
        
        current_stats.frame_count++;
        current_stats.timestamp = std::chrono::system_clock::now();
    }
};

CphProcessor::CphProcessor() : pImpl(std::make_unique<Impl>()) {
}

CphProcessor::~CphProcessor() = default;

bool CphProcessor::Initialize(const CphParams& params) {
    std::vector<ErrorReport> validation_errors;
    if (!ParamValidator::ValidateCphParams(params, validation_errors)) {
        for (const auto& error : validation_errors) {
            pImpl->LogError(error.code, error.message, error.field_name, error.invalid_value);
        }
        return false;
    }
    
    pImpl->current_params = params;
    pImpl->current_params.ClampToValidRange(); // Ensure all parameters are in valid range
    
    // 初始化色调映射器
    if (!pImpl->tone_mapper.Initialize(pImpl->current_params)) {
        pImpl->LogError(ErrorCode::SCHEMA_MISSING, "Failed to initialize tone mapper: " + 
                       pImpl->tone_mapper.GetLastError());
        return false;
    }
    
    // 初始化高光细节处理器
    if (!pImpl->highlight_processor.Initialize(pImpl->current_params)) {
        pImpl->LogError(ErrorCode::SCHEMA_MISSING, "Failed to initialize highlight detail processor: " + 
                       pImpl->highlight_processor.GetLastError());
        return false;
    }
    
    pImpl->current_stats.Reset();
    pImpl->initialized = true;
    
    return true;
}

bool CphProcessor::ValidateParams(const CphParams& params) {
    std::vector<ErrorReport> validation_errors;
    return ParamValidator::ValidateCphParams(params, validation_errors);
}

bool CphProcessor::ProcessFrame(const Image& input, Image& output) {
    if (!pImpl->initialized) {
        pImpl->LogError(ErrorCode::SCHEMA_MISSING, "Processor not initialized");
        return false;
    }
    
    if (!input.IsValid()) {
        pImpl->LogError(ErrorCode::NAN_INF, "Invalid input image");
        return false;
    }
    
    return ProcessFrameInternal(input, output);
}

bool CphProcessor::ProcessFrameInternal(const Image& input, Image& output) {
    try {
        // 转换到工作域（BT.2020+PQ归一化）
        Image working_image;
        ColorSpaceConverter::ToWorkingDomain(input, working_image);
        
        // 应用色调映射到亮度通道
        ApplyToneMappingToImage(working_image);
        
        // 应用高光细节处理（仅在x>p区域）
        if (pImpl->current_params.highlight_detail > 0.0f) {
            Image detail_enhanced;
            if (!pImpl->highlight_processor.ProcessFrame(working_image, detail_enhanced, pImpl->current_params.pivot_pq)) {
                pImpl->LogError(ErrorCode::HL_FLICKER, "Highlight detail processing failed: " + 
                               pImpl->highlight_processor.GetLastError());
                // 继续处理，使用原图像
                detail_enhanced = working_image;
            }
            working_image = detail_enhanced;
        }
        
        // 应用饱和度处理（OKLab色彩空间）
        ApplySaturationProcessing(working_image);
        
        // 转换回目标色彩空间
        ColorSpaceConverter::FromWorkingDomain(working_image, output, input.color_space);
        
        // 更新统计信息
        UpdateStatistics(output);
        
        // 验证曲线特性（仅在调试模式或首次处理时）
        if (pImpl->current_stats.frame_count == 1) {
            ValidateCurveProperties();
        }
        
        return true;
    }
    catch (const std::exception& e) {
        pImpl->LogError(ErrorCode::NAN_INF, std::string("Processing exception: ") + e.what());
        return false;
    }
}

void CphProcessor::UpdateStatistics(const Image& processed_frame) {
    pImpl->UpdateStatistics(processed_frame);
}

Statistics CphProcessor::GetStatistics() const {
    std::lock_guard<std::mutex> lock(pImpl->stats_mutex);
    return pImpl->current_stats;
}

void CphProcessor::ResetStatistics() {
    std::lock_guard<std::mutex> lock(pImpl->stats_mutex);
    pImpl->current_stats.Reset();
}

std::string CphProcessor::GetLastError() const {
    std::lock_guard<std::mutex> lock(pImpl->error_mutex);
    return pImpl->last_error;
}

std::vector<ErrorReport> CphProcessor::GetErrorHistory() const {
    std::lock_guard<std::mutex> lock(pImpl->error_mutex);
    return pImpl->error_history;
}

void CphProcessor::ClearErrors() {
    std::lock_guard<std::mutex> lock(pImpl->error_mutex);
    pImpl->error_history.clear();
    pImpl->last_error.clear();
}

void CphProcessor::SetDeterministicMode(bool enabled) {
    pImpl->current_params.deterministic = enabled;
}

void CphProcessor::SetDCIComplianceMode(bool enabled) {
    pImpl->current_params.dci_compliance = enabled;
}

void CphProcessor::ApplyToneMappingToImage(Image& working_image) {
    /**
     * 在工作域中应用色调映射
     * 
     * 处理顺序：
     * 1. 计算每个像素的最大RGB值作为亮度代表
     * 2. 应用色调映射到亮度
     * 3. 按比例缩放RGB通道
     */
    
    for (int y = 0; y < working_image.height; ++y) {
        for (int x = 0; x < working_image.width; ++x) {
            float* pixel = working_image.GetPixel(x, y);
            if (!pixel) continue;
            
            // 检查像素值的有效性
            if (!NumericalUtils::IsFiniteRGB(pixel)) {
                // NaN/Inf保护：设置为安全值
                pixel[0] = pixel[1] = pixel[2] = 0.0f;
                continue;
            }
            
            // 计算当前像素的亮度（使用MaxRGB方法）
            float max_rgb = std::max(pixel[0], std::max(pixel[1], pixel[2]));
            
            if (max_rgb <= 0.0f) {
                continue; // 黑色像素不需要处理
            }
            
            // 应用色调映射
            float mapped_luminance = pImpl->tone_mapper.ApplyToneMapping(max_rgb);
            
            // 计算缩放比例
            float scale_factor = (max_rgb > 0.0f) ? (mapped_luminance / max_rgb) : 1.0f;
            
            // 按比例缩放RGB通道
            pixel[0] *= scale_factor;
            pixel[1] *= scale_factor;
            pixel[2] *= scale_factor;
            
            // 最终保护：确保值在合理范围内
            pixel[0] = std::clamp(pixel[0], 0.0f, 1.0f);
            pixel[1] = std::clamp(pixel[1], 0.0f, 1.0f);
            pixel[2] = std::clamp(pixel[2], 0.0f, 1.0f);
        }
    }
}

void CphProcessor::ApplySaturationProcessing(Image& working_image) {
    /**
     * 在工作域中应用OKLab饱和度处理
     * 
     * 处理顺序：
     * 1. 对每个像素计算亮度值（用于高光权重计算）
     * 2. 应用基础饱和度调节：sat_base∈[0,2]全局缩放
     * 3. 应用高光区域饱和度：sat_hi∈[0,2]，权重w_hi=smoothstep(p,1,x)
     * 4. 应用两级色域处理机制
     */
    
    for (int y = 0; y < working_image.height; ++y) {
        for (int x = 0; x < working_image.width; ++x) {
            float* pixel = working_image.GetPixel(x, y);
            if (!pixel) continue;
            
            // 检查像素值的有效性
            if (!NumericalUtils::IsFiniteRGB(pixel)) {
                // NaN/Inf保护：设置为安全值
                pixel[0] = pixel[1] = pixel[2] = 0.0f;
                continue;
            }
            
            // 计算当前像素的亮度（用于高光权重计算）
            // 在PQ归一化域中，使用MaxRGB作为亮度代表
            float x_luminance = std::max(pixel[0], std::max(pixel[1], pixel[2]));
            x_luminance = std::clamp(x_luminance, 0.0f, 1.0f);
            
            // 应用OKLab饱和度处理
            ColorSpaceConverter::ApplySaturation(
                pixel, 
                pImpl->current_params.sat_base, 
                pImpl->current_params.sat_hi, 
                pImpl->current_params.pivot_pq, 
                x_luminance
            );
            
            // 应用两级色域处理
            bool was_out_of_gamut = ColorSpaceConverter::ApplyGamutProcessing(
                pixel, 
                ColorSpace::BT2020_PQ,  // 工作域色彩空间
                pImpl->current_params.dci_compliance
            );
            
            // 如果在DCI合规模式下发生了越界，记录警告
            if (was_out_of_gamut && pImpl->current_params.dci_compliance) {
                // 这里可以记录越界统计，但不阻断处理
                // 实际的错误报告将在最终输出时生成
            }
            
            // 最终保护：确保值在工作域范围内
            pixel[0] = std::clamp(pixel[0], 0.0f, 1.0f);
            pixel[1] = std::clamp(pixel[1], 0.0f, 1.0f);
            pixel[2] = std::clamp(pixel[2], 0.0f, 1.0f);
        }
    }
}

void CphProcessor::ValidateCurveProperties() {
    /**
     * 验证色调映射曲线的数学特性
     * 
     * 检查项目：
     * 1. 单调性：确保f(x1) <= f(x2) when x1 <= x2
     * 2. C¹连续性：确保导数在拼接点连续
     */
    
    // 验证单调性
    bool is_monotonic = pImpl->tone_mapper.ValidateMonotonicity();
    pImpl->current_stats.monotonic = is_monotonic;
    
    if (!is_monotonic) {
        pImpl->LogError(ErrorCode::RANGE_KNEE, "Tone mapping curve is not monotonic");
    }
    
    // 验证C¹连续性
    bool is_c1_continuous = pImpl->tone_mapper.ValidateC1Continuity();
    pImpl->current_stats.c1_continuous = is_c1_continuous;
    
    if (!is_c1_continuous) {
        pImpl->LogError(ErrorCode::RANGE_KNEE, "Tone mapping curve is not C¹ continuous");
    }
}

// Parameter validation implementation
bool ParamValidator::ValidateCphParams(const CphParams& params, std::vector<ErrorReport>& errors) {
    bool valid = true;
    
    valid &= ValidateCommonParams(params, errors);
    
    if (params.curve == CurveType::PPR) {
        valid &= ValidatePPRParams(params, errors);
    } else if (params.curve == CurveType::RLOG) {
        valid &= ValidateRLOGParams(params, errors);
    }
    
    return valid;
}

bool ParamValidator::ValidatePPRParams(const CphParams& params, std::vector<ErrorReport>& errors) {
    bool valid = true;
    
    valid &= ValidateRange(params.gamma_s, 1.0f, 1.6f, "gamma_s", errors);
    valid &= ValidateRange(params.gamma_h, 0.8f, 1.4f, "gamma_h", errors);
    valid &= ValidateRange(params.shoulder_h, 0.5f, 3.0f, "shoulder_h", errors);
    
    return valid;
}

bool ParamValidator::ValidateRLOGParams(const CphParams& params, std::vector<ErrorReport>& errors) {
    bool valid = true;
    
    valid &= ValidateRange(params.rlog_a, 1.0f, 16.0f, "rlog_a", errors);
    valid &= ValidateRange(params.rlog_b, 0.8f, 1.2f, "rlog_b", errors);
    valid &= ValidateRange(params.rlog_c, 0.5f, 3.0f, "rlog_c", errors);
    valid &= ValidateRange(params.rlog_t, 0.4f, 0.7f, "rlog_t", errors);
    
    return valid;
}

bool ParamValidator::ValidateCommonParams(const CphParams& params, std::vector<ErrorReport>& errors) {
    bool valid = true;
    
    valid &= ValidateRange(params.pivot_pq, 0.05f, 0.30f, "pivot_pq", errors);
    valid &= ValidateRange(params.black_lift, 0.0f, 0.02f, "black_lift", errors);
    valid &= ValidateRange(params.highlight_detail, 0.0f, 1.0f, "highlight_detail", errors);
    valid &= ValidateRange(params.sat_base, 0.0f, 2.0f, "sat_base", errors);
    valid &= ValidateRange(params.sat_hi, 0.0f, 2.0f, "sat_hi", errors);
    valid &= ValidateRange(params.yknee, 0.95f, 0.99f, "yknee", errors);
    valid &= ValidateRange(params.alpha, 0.2f, 1.0f, "alpha", errors);
    valid &= ValidateRange(params.toe, 0.0f, 0.01f, "toe", errors);
    
    return valid;
}

bool ParamValidator::ValidateRange(float value, float min_val, float max_val, 
                                  const std::string& param_name, std::vector<ErrorReport>& errors) {
    if (!NumericalUtils::IsInRange(value, min_val, max_val)) {
        ErrorReport error(ErrorCode::RANGE_PIVOT, "Parameter out of range");
        error.field_name = param_name;
        error.invalid_value = value;
        errors.push_back(error);
        return false;
    }
    return true;
}

} // namespace CinemaProHDR