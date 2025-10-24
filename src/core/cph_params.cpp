#include "cinema_pro_hdr/core.h"
#include <algorithm>
#include <cmath>

namespace CinemaProHDR {

bool CphParams::IsValid() const {
    // Check pivot range
    if (pivot_pq < 0.05f || pivot_pq > 0.30f) return false;
    
    // Check PPR parameters
    if (gamma_s < 1.0f || gamma_s > 1.6f) return false;
    if (gamma_h < 0.8f || gamma_h > 1.4f) return false;
    if (shoulder_h < 0.5f || shoulder_h > 3.0f) return false;
    
    // Check RLOG parameters
    if (rlog_a < 1.0f || rlog_a > 16.0f) return false;
    if (rlog_b < 0.8f || rlog_b > 1.2f) return false;
    if (rlog_c < 0.5f || rlog_c > 3.0f) return false;
    if (rlog_t < 0.4f || rlog_t > 0.7f) return false;
    
    // Check common parameters
    if (black_lift < 0.0f || black_lift > 0.02f) return false;
    if (highlight_detail < 0.0f || highlight_detail > 1.0f) return false;
    if (sat_base < 0.0f || sat_base > 2.0f) return false;
    if (sat_hi < 0.0f || sat_hi > 2.0f) return false;
    
    // Check soft knee parameters
    if (yknee < 0.95f || yknee > 0.99f) return false;
    if (alpha < 0.2f || alpha > 1.0f) return false;
    if (toe < 0.0f || toe > 0.01f) return false;
    
    return true;
}

void CphParams::ClampToValidRange() {
    // Clamp pivot
    pivot_pq = std::clamp(pivot_pq, 0.05f, 0.30f);
    
    // Clamp PPR parameters
    gamma_s = std::clamp(gamma_s, 1.0f, 1.6f);
    gamma_h = std::clamp(gamma_h, 0.8f, 1.4f);
    shoulder_h = std::clamp(shoulder_h, 0.5f, 3.0f);
    
    // Clamp RLOG parameters
    rlog_a = std::clamp(rlog_a, 1.0f, 16.0f);
    rlog_b = std::clamp(rlog_b, 0.8f, 1.2f);
    rlog_c = std::clamp(rlog_c, 0.5f, 3.0f);
    rlog_t = std::clamp(rlog_t, 0.4f, 0.7f);
    
    // Clamp common parameters
    black_lift = std::clamp(black_lift, 0.0f, 0.02f);
    highlight_detail = std::clamp(highlight_detail, 0.0f, 1.0f);
    sat_base = std::clamp(sat_base, 0.0f, 2.0f);
    sat_hi = std::clamp(sat_hi, 0.0f, 2.0f);
    
    // Clamp soft knee parameters
    yknee = std::clamp(yknee, 0.95f, 0.99f);
    alpha = std::clamp(alpha, 0.2f, 1.0f);
    toe = std::clamp(toe, 0.0f, 0.01f);
}

} // namespace CinemaProHDR