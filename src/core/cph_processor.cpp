#include "cinema_pro_hdr/processor.h"
#include "cinema_pro_hdr/color_space.h"
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
        // Convert to working domain
        Image working_image;
        ColorSpaceConverter::ToWorkingDomain(input, working_image);
        
        // For now, just copy the working image to output (tone mapping will be implemented in later tasks)
        // This establishes the basic processing pipeline
        ColorSpaceConverter::FromWorkingDomain(working_image, output, input.color_space);
        
        // Update statistics
        UpdateStatistics(output);
        
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