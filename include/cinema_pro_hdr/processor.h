#pragma once

#include "core.h"

namespace CinemaProHDR {

// Main processor class
class CphProcessor {
public:
    CphProcessor();
    ~CphProcessor();
    
    // Initialization and configuration
    bool Initialize(const CphParams& params);
    bool ValidateParams(const CphParams& params);
    
    // Frame processing
    bool ProcessFrame(const Image& input, Image& output);
    
    // Statistics and monitoring
    Statistics GetStatistics() const;
    void ResetStatistics();
    
    // Error handling
    std::string GetLastError() const;
    std::vector<ErrorReport> GetErrorHistory() const;
    void ClearErrors();
    
    // Configuration
    void SetDeterministicMode(bool enabled);
    void SetDCIComplianceMode(bool enabled);
    
private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
    
    // Internal processing functions
    bool ProcessFrameInternal(const Image& input, Image& output);
    void UpdateStatistics(const Image& processed_frame);
    void LogError(ErrorCode code, const std::string& message, 
                  const std::string& field = "", float value = 0.0f);
};

// Parameter validation utilities
class ParamValidator {
public:
    static bool ValidateCphParams(const CphParams& params, std::vector<ErrorReport>& errors);
    static bool ValidatePPRParams(const CphParams& params, std::vector<ErrorReport>& errors);
    static bool ValidateRLOGParams(const CphParams& params, std::vector<ErrorReport>& errors);
    static bool ValidateCommonParams(const CphParams& params, std::vector<ErrorReport>& errors);
    
    // Range validation helpers
    static bool ValidateRange(float value, float min_val, float max_val, 
                             const std::string& param_name, std::vector<ErrorReport>& errors);
};

} // namespace CinemaProHDR