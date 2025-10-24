#include "cinema_pro_hdr/core.h"
#include <sstream>
#include <iomanip>

namespace CinemaProHDR {

ErrorReport::ErrorReport(ErrorCode c, const std::string& msg) 
    : code(c), message(msg), timestamp(std::chrono::system_clock::now()) {
}

std::string ErrorReport::ToString() const {
    std::ostringstream oss;
    
    // Format timestamp
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    auto tm = *std::localtime(&time_t);
    
    oss << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "]";
    
    // Add error level
    switch (code) {
        case ErrorCode::SUCCESS:
            oss << "[INFO]";
            break;
        case ErrorCode::RANGE_PIVOT:
        case ErrorCode::RANGE_KNEE:
        case ErrorCode::DET_MISMATCH:
        case ErrorCode::HL_FLICKER:
            oss << "[WARN]";
            break;
        default:
            oss << "[ERROR]";
            break;
    }
    
    // Add clip GUID and timecode if available
    if (!clip_guid.empty()) {
        oss << "[" << clip_guid << "]";
    }
    if (!timecode.empty()) {
        oss << "[" << timecode << "]";
    }
    
    // Add error code
    oss << " code=" << static_cast<int>(code);
    
    // Add field and value if available
    if (!field_name.empty()) {
        oss << ", field=" << field_name;
        if (invalid_value != 0.0f) {
            oss << ", val=" << invalid_value;
        }
    }
    
    // Add action taken
    if (!action_taken.empty()) {
        oss << ", action=" << action_taken;
    }
    
    // Add message
    if (!message.empty()) {
        oss << " - " << message;
    }
    
    return oss.str();
}

} // namespace CinemaProHDR