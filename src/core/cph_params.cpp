#include "cinema_pro_hdr/core.h"
#include "cinema_pro_hdr/error_handler.h"
#include <algorithm>
#include <cmath>

namespace CinemaProHDR {

bool CphParams::IsValid() const {
    // 首先检查所有参数是否为有效的浮点数（非NaN/Inf）
    if (!NumericalProtection::IsValid(pivot_pq) ||
        !NumericalProtection::IsValid(gamma_s) ||
        !NumericalProtection::IsValid(gamma_h) ||
        !NumericalProtection::IsValid(shoulder_h) ||
        !NumericalProtection::IsValid(rlog_a) ||
        !NumericalProtection::IsValid(rlog_b) ||
        !NumericalProtection::IsValid(rlog_c) ||
        !NumericalProtection::IsValid(rlog_t) ||
        !NumericalProtection::IsValid(black_lift) ||
        !NumericalProtection::IsValid(highlight_detail) ||
        !NumericalProtection::IsValid(sat_base) ||
        !NumericalProtection::IsValid(sat_hi) ||
        !NumericalProtection::IsValid(yknee) ||
        !NumericalProtection::IsValid(alpha) ||
        !NumericalProtection::IsValid(toe)) {
        return false;
    }
    
    // 检查参数范围
    if (pivot_pq < 0.05f || pivot_pq > 0.30f) return false;
    
    // 检查PPR参数
    if (gamma_s < 1.0f || gamma_s > 1.6f) return false;
    if (gamma_h < 0.8f || gamma_h > 1.4f) return false;
    if (shoulder_h < 0.5f || shoulder_h > 3.0f) return false;
    
    // 检查RLOG参数
    if (rlog_a < 1.0f || rlog_a > 16.0f) return false;
    if (rlog_b < 0.8f || rlog_b > 1.2f) return false;
    if (rlog_c < 0.5f || rlog_c > 3.0f) return false;
    if (rlog_t < 0.4f || rlog_t > 0.7f) return false;
    
    // 检查通用参数
    if (black_lift < 0.0f || black_lift > 0.02f) return false;
    if (highlight_detail < 0.0f || highlight_detail > 1.0f) return false;
    if (sat_base < 0.0f || sat_base > 2.0f) return false;
    if (sat_hi < 0.0f || sat_hi > 2.0f) return false;
    
    // 检查软膝参数
    if (yknee < 0.95f || yknee > 0.99f) return false;
    if (alpha < 0.2f || alpha > 1.0f) return false;
    if (toe < 0.0f || toe > 0.01f) return false;
    
    return true;
}

void CphParams::ClampToValidRange() {
    // 首先修复任何NaN/Inf值，使用范围中点作为默认值
    pivot_pq = NumericalProtection::FixInvalid(pivot_pq, 0.175f);  // (0.05 + 0.30) / 2
    gamma_s = NumericalProtection::FixInvalid(gamma_s, 1.3f);      // (1.0 + 1.6) / 2
    gamma_h = NumericalProtection::FixInvalid(gamma_h, 1.1f);      // (0.8 + 1.4) / 2
    shoulder_h = NumericalProtection::FixInvalid(shoulder_h, 1.75f); // (0.5 + 3.0) / 2
    
    rlog_a = NumericalProtection::FixInvalid(rlog_a, 8.5f);        // (1.0 + 16.0) / 2
    rlog_b = NumericalProtection::FixInvalid(rlog_b, 1.0f);        // (0.8 + 1.2) / 2
    rlog_c = NumericalProtection::FixInvalid(rlog_c, 1.75f);       // (0.5 + 3.0) / 2
    rlog_t = NumericalProtection::FixInvalid(rlog_t, 0.55f);       // (0.4 + 0.7) / 2
    
    black_lift = NumericalProtection::FixInvalid(black_lift, 0.01f);     // (0.0 + 0.02) / 2
    highlight_detail = NumericalProtection::FixInvalid(highlight_detail, 0.5f); // (0.0 + 1.0) / 2
    sat_base = NumericalProtection::FixInvalid(sat_base, 1.0f);          // (0.0 + 2.0) / 2
    sat_hi = NumericalProtection::FixInvalid(sat_hi, 1.0f);              // (0.0 + 2.0) / 2
    
    yknee = NumericalProtection::FixInvalid(yknee, 0.97f);        // (0.95 + 0.99) / 2
    alpha = NumericalProtection::FixInvalid(alpha, 0.6f);         // (0.2 + 1.0) / 2
    toe = NumericalProtection::FixInvalid(toe, 0.005f);           // (0.0 + 0.01) / 2
    
    // 然后将值钳制到有效范围
    pivot_pq = std::clamp(pivot_pq, 0.05f, 0.30f);
    
    // 钳制PPR参数
    gamma_s = std::clamp(gamma_s, 1.0f, 1.6f);
    gamma_h = std::clamp(gamma_h, 0.8f, 1.4f);
    shoulder_h = std::clamp(shoulder_h, 0.5f, 3.0f);
    
    // 钳制RLOG参数
    rlog_a = std::clamp(rlog_a, 1.0f, 16.0f);
    rlog_b = std::clamp(rlog_b, 0.8f, 1.2f);
    rlog_c = std::clamp(rlog_c, 0.5f, 3.0f);
    rlog_t = std::clamp(rlog_t, 0.4f, 0.7f);
    
    // 钳制通用参数
    black_lift = std::clamp(black_lift, 0.0f, 0.02f);
    highlight_detail = std::clamp(highlight_detail, 0.0f, 1.0f);
    sat_base = std::clamp(sat_base, 0.0f, 2.0f);
    sat_hi = std::clamp(sat_hi, 0.0f, 2.0f);
    
    // 钳制软膝参数
    yknee = std::clamp(yknee, 0.95f, 0.99f);
    alpha = std::clamp(alpha, 0.2f, 1.0f);
    toe = std::clamp(toe, 0.0f, 0.01f);
}

} // namespace CinemaProHDR