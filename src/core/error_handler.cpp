#include "cinema_pro_hdr/error_handler.h"
#include <iostream>
#include <sstream>
#include <algorithm>

namespace CinemaProHDR {

// ============================================================================
// LogThrottler Implementation
// ============================================================================

LogThrottler::LogThrottler() = default;

bool LogThrottler::ShouldLog(ErrorCode error_code) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto& info = throttle_map_[error_code];
    auto now = std::chrono::steady_clock::now();
    
    // 更新时间窗口
    UpdateWindow(info);
    
    // 检查是否超过限制
    if (info.count >= MAX_LOGS_PER_SECOND) {
        // 记录被节流的信息
        info.throttled_count++;
        if (info.throttled_count == 1) {
            info.first_throttled = now;
        }
        info.last_throttled = now;
        return false;
    }
    
    info.count++;
    return true;
}

std::string LogThrottler::GetAggregateReport(ErrorCode error_code) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = throttle_map_.find(error_code);
    if (it == throttle_map_.end() || it->second.throttled_count == 0) {
        return "";
    }
    
    const auto& info = it->second;
    std::ostringstream oss;
    
    oss << "聚合报告: 错误码 " << static_cast<int>(error_code) 
        << " 被节流 " << info.throttled_count << " 次";
    
    // 计算时间范围
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        info.last_throttled - info.first_throttled);
    
    if (duration.count() > 0) {
        oss << ", 时间范围: " << duration.count() << "ms";
    }
    
    return oss.str();
}

void LogThrottler::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    throttle_map_.clear();
}

void LogThrottler::UpdateWindow(ThrottleInfo& info) {
    auto now = std::chrono::steady_clock::now();
    
    // 如果是新的时间窗口，重置计数
    if (now - info.window_start >= WINDOW_DURATION) {
        info.count = 0;
        info.window_start = now;
        // 不重置throttled_count，保留用于聚合报告
    }
}

// ============================================================================
// NumericalProtection Implementation  
// ============================================================================

bool NumericalProtection::IsValid(float value) {
    return std::isfinite(value) && !std::isnan(value);
}

bool NumericalProtection::IsValid(float x, float y, float z) {
    return IsValid(x) && IsValid(y) && IsValid(z);
}

float NumericalProtection::Saturate(float value) {
    if (!IsValid(value)) {
        return 0.0f;
    }
    return std::clamp(value, 0.0f, 1.0f);
}

float NumericalProtection::SafeDivide(float numerator, float denominator, float fallback_value) {
    if (!IsValid(numerator) || !IsValid(denominator) || std::abs(denominator) < 1e-8f) {
        return fallback_value;
    }
    
    float result = numerator / denominator;
    return IsValid(result) ? result : fallback_value;
}

float NumericalProtection::SafeLog(float value, float fallback_value) {
    if (!IsValid(value) || value <= 0.0f) {
        return fallback_value;
    }
    
    float result = std::log(value);
    return IsValid(result) ? result : fallback_value;
}

float NumericalProtection::SafePow(float base, float exponent, float fallback_value) {
    if (!IsValid(base) || !IsValid(exponent)) {
        return fallback_value;
    }
    
    // 特殊情况处理
    if (base == 0.0f && exponent <= 0.0f) {
        return fallback_value;
    }
    
    if (base < 0.0f && std::floor(exponent) != exponent) {
        return fallback_value; // 负数的非整数幂
    }
    
    float result = std::pow(base, exponent);
    return IsValid(result) ? result : fallback_value;
}

float NumericalProtection::FixInvalid(float value, float fallback_value) {
    return IsValid(value) ? value : fallback_value;
}

void NumericalProtection::FixInvalid(float& x, float& y, float& z, float fallback_value) {
    x = FixInvalid(x, fallback_value);
    y = FixInvalid(y, fallback_value);
    z = FixInvalid(z, fallback_value);
}

// ============================================================================
// ErrorHandler Implementation
// ============================================================================

ErrorHandler::ErrorHandler() 
    : current_strategy_(FallbackStrategy::PARAMETER_CORRECTION) {
}

FallbackStrategy ErrorHandler::HandleError(
    ErrorCode error_code,
    const std::string& message,
    const std::string& field_name,
    float invalid_value,
    const std::string& clip_guid,
    const std::string& timecode) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 创建错误报告
    ErrorReport error(error_code, message);
    error.field_name = field_name;
    error.invalid_value = invalid_value;
    error.clip_guid = clip_guid;
    error.timecode = timecode;
    
    // 确定回退策略
    FallbackStrategy strategy = DetermineFallbackStrategy(error_code);
    current_strategy_ = strategy;
    
    // 设置采取的行动
    switch (strategy) {
        case FallbackStrategy::PARAMETER_CORRECTION:
            error.action_taken = "PARAM_CORRECT";
            break;
        case FallbackStrategy::STANDARD_FALLBACK:
            error.action_taken = "FALLBACK2094";
            break;
        case FallbackStrategy::HARD_FALLBACK:
            error.action_taken = "IDENTITY";
            break;
    }
    
    last_error_ = error;
    
    // 记录日志（如果没有被节流）
    if (throttler_.ShouldLog(error_code)) {
        LogError(error);
    }
    
    // 调用错误回调
    if (error_callback_) {
        error_callback_(error);
    }
    
    return strategy;
}

bool ErrorHandler::ValidateAndCorrectParams(CphParams& params) {
    bool corrected = false;
    
    // 验证PPR参数 - 直接传递参数引用
    corrected |= ValidateFloatRange(params.pivot_pq, 0.05f, 0.30f, "pivot_pq");
    corrected |= ValidateFloatRange(params.gamma_s, 1.0f, 1.6f, "gamma_s");
    corrected |= ValidateFloatRange(params.gamma_h, 0.8f, 1.4f, "gamma_h");
    corrected |= ValidateFloatRange(params.shoulder_h, 0.5f, 3.0f, "shoulder_h");
    corrected |= ValidateFloatRange(params.black_lift, 0.0f, 0.02f, "black_lift");
    corrected |= ValidateFloatRange(params.highlight_detail, 0.0f, 1.0f, "highlight_detail");
    corrected |= ValidateFloatRange(params.sat_base, 0.0f, 2.0f, "sat_base");
    corrected |= ValidateFloatRange(params.sat_hi, 0.0f, 2.0f, "sat_hi");
    
    // 验证RLOG参数
    corrected |= ValidateFloatRange(params.rlog_a, 1.0f, 16.0f, "rlog_a");
    corrected |= ValidateFloatRange(params.rlog_b, 0.8f, 1.2f, "rlog_b");
    corrected |= ValidateFloatRange(params.rlog_c, 0.5f, 3.0f, "rlog_c");
    corrected |= ValidateFloatRange(params.rlog_t, 0.4f, 0.7f, "rlog_t");
    
    // 验证软膝参数
    corrected |= ValidateFloatRange(params.yknee, 0.95f, 0.99f, "yknee");
    corrected |= ValidateFloatRange(params.alpha, 0.2f, 1.0f, "alpha");
    corrected |= ValidateFloatRange(params.toe, 0.0f, 0.01f, "toe");
    
    return corrected;
}

void ErrorHandler::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    last_error_ = ErrorReport();
    current_strategy_ = FallbackStrategy::PARAMETER_CORRECTION;
    throttler_.Reset();
}

void ErrorHandler::SetErrorCallback(std::function<void(const ErrorReport&)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    error_callback_ = callback;
}

std::vector<std::string> ErrorHandler::GetAggregateReports() {
    std::vector<std::string> reports;
    
    // 获取所有可能的错误码的聚合报告
    for (int i = 0; i <= static_cast<int>(ErrorCode::GAMUT_OOG); ++i) {
        ErrorCode code = static_cast<ErrorCode>(i);
        std::string report = throttler_.GetAggregateReport(code);
        if (!report.empty()) {
            reports.push_back(report);
        }
    }
    
    return reports;
}

bool ErrorHandler::ValidateFloatRange(float& value, float min, float max, 
                                    const std::string& field_name) {
    // 首先检查数值有效性
    if (!NumericalProtection::IsValid(value)) {
        HandleError(ErrorCode::NAN_INF, 
                   "参数包含NaN或Inf值", 
                   field_name, value);
        
        // 修正为默认值（范围中点）
        CorrectParameter(value, min, max, field_name);
        return true;
    }
    
    // 检查范围
    if (value < min || value > max) {
        HandleError(ErrorCode::RANGE_PIVOT, 
                   "参数超出有效范围", 
                   field_name, value);
        
        CorrectParameter(value, min, max, field_name);
        return true;
    }
    
    return false;
}

void ErrorHandler::CorrectParameter(float& param, float min, float max, const std::string& field_name) {
    float original = param;
    param = std::clamp(param, min, max);
    
    // 如果是NaN或Inf，设置为中点值
    if (!NumericalProtection::IsValid(param)) {
        param = (min + max) * 0.5f;
    }
    
    std::ostringstream oss;
    oss << "参数 " << field_name << " 从 " << original << " 修正为 " << param;
    
    // 这里可以添加详细的日志记录
    if (throttler_.ShouldLog(ErrorCode::RANGE_PIVOT)) {
        std::cout << "[参数修正] " << oss.str() << std::endl;
    }
}

FallbackStrategy ErrorHandler::DetermineFallbackStrategy(ErrorCode error_code) {
    switch (error_code) {
        // 第一级：参数修正
        case ErrorCode::RANGE_PIVOT:
        case ErrorCode::RANGE_KNEE:
            return FallbackStrategy::PARAMETER_CORRECTION;
            
        // 第二级：标准回退
        case ErrorCode::SCHEMA_MISSING:
        case ErrorCode::DCI_BOUND:
        case ErrorCode::GAMUT_OOG:
        case ErrorCode::DET_MISMATCH:
        case ErrorCode::HL_FLICKER:
            return FallbackStrategy::STANDARD_FALLBACK;
            
        // 第三级：硬回退
        case ErrorCode::NAN_INF:
        default:
            return FallbackStrategy::HARD_FALLBACK;
    }
}

void ErrorHandler::LogError(const ErrorReport& error) {
    // 输出到标准错误流
    std::cerr << error.ToString() << std::endl;
    
    // 这里可以添加更复杂的日志记录逻辑，比如：
    // - 写入日志文件
    // - 发送到日志服务
    // - 触发监控告警等
}

// ============================================================================
// GlobalErrorHandler Implementation
// ============================================================================

std::unique_ptr<ErrorHandler> GlobalErrorHandler::instance_ = nullptr;
std::mutex GlobalErrorHandler::instance_mutex_;

ErrorHandler& GlobalErrorHandler::Instance() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (!instance_) {
        instance_ = std::make_unique<ErrorHandler>();
    }
    return *instance_;
}

FallbackStrategy GlobalErrorHandler::HandleError(ErrorCode code, const std::string& message) {
    return Instance().HandleError(code, message);
}

bool GlobalErrorHandler::ValidateParams(CphParams& params) {
    return Instance().ValidateAndCorrectParams(params);
}

void GlobalErrorHandler::Reset() {
    Instance().Reset();
}

} // namespace CinemaProHDR