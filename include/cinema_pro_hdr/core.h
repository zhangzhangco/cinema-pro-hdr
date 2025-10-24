#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <memory>

namespace CinemaProHDR {

// Forward declarations
struct CphParams;
struct Image;
struct Statistics;
struct ErrorReport;

// Error codes enumeration
enum class ErrorCode {
    SUCCESS = 0,
    SCHEMA_MISSING,
    RANGE_PIVOT,
    RANGE_KNEE,
    NAN_INF,
    DET_MISMATCH,
    HL_FLICKER,
    DCI_BOUND,
    GAMUT_OOG
};

// Curve types
enum class CurveType {
    PPR = 0,    // Pivoted Power-Rational
    RLOG = 1    // Rational Logarithmic
};

// Color spaces
enum class ColorSpace {
    BT2020_PQ,
    P3_D65,
    ACESG,
    REC709
};

// Main parameter structure
struct CphParams {
    CurveType curve = CurveType::PPR;
    float pivot_pq = 0.18f;        // [0.05, 0.30]
    float gamma_s = 1.25f;         // [1.0, 1.6] 
    float gamma_h = 1.10f;         // [0.8, 1.4]
    float shoulder_h = 1.5f;       // [0.5, 3.0]
    float black_lift = 0.002f;     // [0.0, 0.02]
    float highlight_detail = 0.2f; // [0.0, 1.0]
    float sat_base = 1.0f;         // [0.0, 2.0]
    float sat_hi = 0.95f;          // [0.0, 2.0]
    bool dci_compliance = false;   // DCI compliance mode
    bool deterministic = false;    // Deterministic mode
    
    // RLOG specific parameters
    float rlog_a = 8.0f;          // [1, 16]
    float rlog_b = 1.0f;          // [0.8, 1.2]
    float rlog_c = 1.5f;          // [0.5, 3.0]
    float rlog_t = 0.55f;         // [0.4, 0.7]
    
    // Soft knee parameters
    float yknee = 0.97f;          // [0.95, 0.99]
    float alpha = 0.6f;           // [0.2, 1.0]
    float toe = 0.002f;           // [0.0, 0.01]
    
    // Validation
    bool IsValid() const;
    void ClampToValidRange();
};

// Image data structure
struct Image {
    int width = 0;
    int height = 0;
    int channels = 3;  // RGB
    std::vector<float> data;
    ColorSpace color_space = ColorSpace::BT2020_PQ;
    
    Image() = default;
    Image(int w, int h, int c = 3);
    
    // Pixel access
    float* GetPixel(int x, int y);
    const float* GetPixel(int x, int y) const;
    
    // Utility functions
    bool IsValid() const;
    void Clear();
    size_t GetDataSize() const { return width * height * channels; }
};

// Statistics structure
struct Statistics {
    struct PQStats {
        float min_pq = 0.0f;      // 1% trimmed minimum
        float avg_pq = 0.0f;      // Trimmed mean
        float max_pq = 1.0f;      // 1% trimmed maximum
        float variance = 0.0f;    // Variance
    } pq_stats;
    
    bool monotonic = true;        // Monotonicity check
    bool c1_continuous = true;    // C¹ continuity check
    float max_derivative_gap = 0.0f; // Maximum derivative gap
    
    // Frame statistics
    int frame_count = 0;
    std::chrono::system_clock::time_point timestamp;
    
    void Reset();
    bool IsValid() const;
};

// Error reporting structure
struct ErrorReport {
    ErrorCode code = ErrorCode::SUCCESS;
    std::string message;
    std::string field_name;
    float invalid_value = 0.0f;
    std::string action_taken;
    std::string clip_guid;
    std::string timecode;
    std::chrono::system_clock::time_point timestamp;
    
    ErrorReport() = default;
    ErrorReport(ErrorCode c, const std::string& msg);
    
    bool IsError() const { return code != ErrorCode::SUCCESS; }
    std::string ToString() const;
};

} // namespace CinemaProHDR