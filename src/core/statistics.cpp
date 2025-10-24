#include "cinema_pro_hdr/core.h"
#include <cmath>

namespace CinemaProHDR {

void Statistics::Reset() {
    pq_stats.min_pq = 0.0f;
    pq_stats.avg_pq = 0.0f;
    pq_stats.max_pq = 1.0f;
    pq_stats.variance = 0.0f;
    
    monotonic = true;
    c1_continuous = true;
    max_derivative_gap = 0.0f;
    
    frame_count = 0;
    timestamp = std::chrono::system_clock::now();
}

bool Statistics::IsValid() const {
    // Check PQ stats ranges
    if (pq_stats.min_pq < 0.0f || pq_stats.min_pq > 1.0f) return false;
    if (pq_stats.avg_pq < 0.0f || pq_stats.avg_pq > 1.0f) return false;
    if (pq_stats.max_pq < 0.0f || pq_stats.max_pq > 1.0f) return false;
    if (pq_stats.variance < 0.0f) return false;
    
    // Check logical consistency
    if (pq_stats.min_pq > pq_stats.avg_pq) return false;
    if (pq_stats.avg_pq > pq_stats.max_pq) return false;
    
    // Check derivative gap
    if (max_derivative_gap < 0.0f) return false;
    
    // Check frame count
    if (frame_count < 0) return false;
    
    // Check for NaN/Inf values
    if (!std::isfinite(pq_stats.min_pq) || !std::isfinite(pq_stats.avg_pq) ||
        !std::isfinite(pq_stats.max_pq) || !std::isfinite(pq_stats.variance) ||
        !std::isfinite(max_derivative_gap)) {
        return false;
    }
    
    return true;
}

} // namespace CinemaProHDR