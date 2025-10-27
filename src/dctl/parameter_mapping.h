/**
 * Cinema Pro HDR DCTL 参数映射工具
 * 
 * 用途：UI控件参数到算法参数的转换
 * 支持：PPR和RLOG参数映射公式
 * 
 * 这个头文件定义了UI→算法的映射关系，
 * 可以被C++代码和DCTL代码共同使用
 */

#pragma once

#ifdef __cplusplus
#include <cmath>
#include <algorithm>
namespace CinemaProHDR {
namespace DCTLMapping {

// 使用std命名空间中的函数
using std::clamp;
using std::pow;
using std::max;
using std::isfinite;

#else
// DCTL环境中的函数映射
#define clamp(x, min_val, max_val) _clampf(x, min_val, max_val)
#define pow(base, exp) _powf(base, exp)
#define max(a, b) _fmaxf(a, b)
#define isfinite(x) (x == x && x != INFINITY && x != -INFINITY)
#endif

// =============================================================================
// PPR参数映射公式
// =============================================================================

/**
 * UI→算法映射：Shadows Contrast → γs
 * 范围：S∈[0,1] → γs∈[1.0,1.6]
 * 公式：γs = 1.0 + 0.6*S
 */
#ifdef __DEVICE__
__DEVICE__ 
#endif
inline float MapShadowsContrast(float shadows_contrast) {
    float s = clamp(shadows_contrast, 0.0f, 1.0f);
    return 1.0f + 0.6f * s;
}

/**
 * UI→算法映射：Highlight Contrast → γh
 * 范围：H∈[0,1] → γh∈[0.8,1.4]
 * 公式：γh = 0.8 + 0.6*H
 */
#ifdef __DEVICE__
__DEVICE__ 
#endif
inline float MapHighlightContrast(float highlight_contrast) {
    float h = clamp(highlight_contrast, 0.0f, 1.0f);
    return 0.8f + 0.6f * h;
}

/**
 * UI→算法映射：Highlights Roll-off → h
 * 范围：R∈[0,1] → h∈[0.5,3.0]
 * 公式：h = 0.5 + 2.5*R
 */
#ifdef __DEVICE__
__DEVICE__ 
#endif
inline float MapHighlightsRolloff(float highlights_rolloff) {
    float r = clamp(highlights_rolloff, 0.0f, 1.0f);
    return 0.5f + 2.5f * r;
}

/**
 * UI→算法映射：Pivot (Nits) → PQ归一化值
 * 范围：nits∈[100,1000] → pq∈[0.05,0.30]
 * 使用PQ OETF进行转换
 */
#ifdef __DEVICE__
__DEVICE__ 
#endif
inline float MapPivotNitsToPQ(float pivot_nits) {
    // PQ OETF常数
    const float PQ_M1 = 0.1593017578125f;
    const float PQ_M2 = 78.84375f;
    const float PQ_C1 = 0.8359375f;
    const float PQ_C2 = 18.8515625f;
    const float PQ_C3 = 18.6875f;
    
    // 钳制到有效范围
    pivot_nits = clamp(pivot_nits, 100.0f, 1000.0f);
    
    // 转换为PQ
    float normalized = pivot_nits / 10000.0f;
    float pow_m1 = pow(normalized, PQ_M1);
    float numerator = PQ_C1 + PQ_C2 * pow_m1;
    float denominator = 1.0f + PQ_C3 * pow_m1;
    
    if (denominator <= 0.0f) return 0.18f; // 默认值
    
    float pq_value = pow(numerator / denominator, PQ_M2);
    return clamp(pq_value, 0.05f, 0.30f);
}

/**
 * 算法→UI映射：PQ归一化值 → Pivot (Nits)
 * 用于UI显示
 */
#ifdef __DEVICE__
__DEVICE__ 
#endif
inline float MapPQToPivotNits(float pq_value) {
    // PQ EOTF常数
    const float PQ_M1 = 0.1593017578125f;
    const float PQ_M2 = 78.84375f;
    const float PQ_C1 = 0.8359375f;
    const float PQ_C2 = 18.8515625f;
    const float PQ_C3 = 18.6875f;
    
    // 钳制到有效范围
    pq_value = clamp(pq_value, 0.05f, 0.30f);
    
    // PQ EOTF
    float pq_pow = pow(pq_value, 1.0f / PQ_M2);
    float numerator = max(0.0f, pq_pow - PQ_C1);
    float denominator = PQ_C2 - PQ_C3 * pq_pow;
    
    if (denominator <= 0.0f) return 180.0f; // 默认值
    
    float linear = pow(numerator / denominator, 1.0f / PQ_M1);
    return linear * 10000.0f; // 转换为cd/m²
}

// =============================================================================
// RLOG参数映射公式
// =============================================================================

/**
 * UI→算法映射：Shadow Lift (RLOG) → a
 * 范围：S∈[0,1] → a∈[1,16]
 * 公式：a = 1 + 15*S
 */
#ifdef __DEVICE__
__DEVICE__ 
#endif
inline float MapRLOGShadowLift(float shadow_lift) {
    float s = clamp(shadow_lift, 0.0f, 1.0f);
    return 1.0f + 15.0f * s;
}

/**
 * UI→算法映射：Highlight Gain (RLOG) → b
 * 范围：G∈[0,1] → b∈[0.8,1.2]
 * 公式：b = 0.8 + 0.4*G
 */
#ifdef __DEVICE__
__DEVICE__ 
#endif
inline float MapRLOGHighlightGain(float highlight_gain) {
    float g = clamp(highlight_gain, 0.0f, 1.0f);
    return 0.8f + 0.4f * g;
}

/**
 * UI→算法映射：Highlight Roll-off (RLOG) → c
 * 范围：R∈[0,1] → c∈[0.5,3.0]
 * 公式：c = 0.5 + 2.5*R
 */
#ifdef __DEVICE__
__DEVICE__ 
#endif
inline float MapRLOGHighlightRolloff(float highlight_rolloff) {
    float r = clamp(highlight_rolloff, 0.0f, 1.0f);
    return 0.5f + 2.5f * r;
}

/**
 * UI→算法映射：Blend Threshold → t
 * 范围：B∈[0,1] → t∈[0.4,0.7]
 * 公式：t = 0.4 + 0.3*B
 */
#ifdef __DEVICE__
__DEVICE__ 
#endif
inline float MapRLOGBlendThreshold(float blend_threshold) {
    float b = clamp(blend_threshold, 0.0f, 1.0f);
    return 0.4f + 0.3f * b;
}

// =============================================================================
// 预设参数定义
// =============================================================================

/**
 * 预设参数结构体
 */
typedef struct {
    // 基础参数
    float pivot_pq;
    int curve_type;  // 0=PPR, 1=RLOG
    
    // PPR参数
    float gamma_s;
    float gamma_h;
    float shoulder_h;
    
    // RLOG参数
    float rlog_a;
    float rlog_b;
    float rlog_c;
    float rlog_t;
    
    // 通用参数
    float black_lift;
    float highlight_detail;
    float sat_base;
    float sat_hi;
    float yknee;
    float alpha;
    float toe;
} DCTLPresetParams;

/**
 * 预设1：Cinema-Flat
 * 特点：平缓的色调映射，保持自然外观
 */
#ifdef __DEVICE__
__DEVICE__ 
#endif
inline DCTLPresetParams GetCinemaFlatPreset() {
    DCTLPresetParams preset;
    
    preset.pivot_pq = 0.18f;        // PQ(180 nits)
    preset.curve_type = 0;          // PPR
    
    // PPR参数（对应UI值：S=0.17, H=0.42, R=0.20）
    preset.gamma_s = 1.10f;         // 1.0 + 0.6*0.17
    preset.gamma_h = 1.05f;         // 0.8 + 0.6*0.42
    preset.shoulder_h = 1.0f;       // 0.5 + 2.5*0.20
    
    // 通用参数
    preset.black_lift = 0.003f;
    preset.highlight_detail = 0.2f;
    preset.sat_base = 1.00f;
    preset.sat_hi = 0.95f;
    preset.yknee = 0.97f;
    preset.alpha = 0.6f;
    preset.toe = 0.002f;
    
    return preset;
}

/**
 * 预设2：Cinema-Punch
 * 特点：增强对比度，适合商业片
 */
#ifdef __DEVICE__
__DEVICE__ 
#endif
inline DCTLPresetParams GetCinemaPunchPreset() {
    DCTLPresetParams preset;
    
    preset.pivot_pq = 0.18f;        // PQ(180 nits)
    preset.curve_type = 0;          // PPR
    
    // PPR参数（对应UI值：S=0.67, H=0.50, R=0.52）
    preset.gamma_s = 1.40f;         // 1.0 + 0.6*0.67
    preset.gamma_h = 1.10f;         // 0.8 + 0.6*0.50
    preset.shoulder_h = 1.8f;       // 0.5 + 2.5*0.52
    
    // 通用参数
    preset.black_lift = 0.002f;
    preset.highlight_detail = 0.4f;
    preset.sat_base = 1.05f;
    preset.sat_hi = 1.00f;
    preset.yknee = 0.97f;
    preset.alpha = 0.6f;
    preset.toe = 0.002f;
    
    return preset;
}

/**
 * 预设3：Cinema-Highlight
 * 特点：保护高光细节，适合高动态范围场景
 */
#ifdef __DEVICE__
__DEVICE__ 
#endif
inline DCTLPresetParams GetCinemaHighlightPreset() {
    DCTLPresetParams preset;
    
    preset.pivot_pq = 0.20f;        // PQ(200 nits)
    preset.curve_type = 0;          // PPR
    
    // PPR参数（对应UI值：S=0.33, H=0.25, R=0.28）
    preset.gamma_s = 1.20f;         // 1.0 + 0.6*0.33
    preset.gamma_h = 0.95f;         // 0.8 + 0.6*0.25
    preset.shoulder_h = 1.2f;       // 0.5 + 2.5*0.28
    
    // 通用参数
    preset.black_lift = 0.004f;
    preset.highlight_detail = 0.6f;
    preset.sat_base = 0.98f;
    preset.sat_hi = 0.92f;
    preset.yknee = 0.97f;
    preset.alpha = 0.6f;
    preset.toe = 0.002f;
    
    return preset;
}

// =============================================================================
// 参数验证和钳制函数
// =============================================================================

/**
 * 验证并钳制所有参数到有效范围
 */
#ifdef __DEVICE__
__DEVICE__ 
#endif
inline DCTLPresetParams ValidateAndClampParams(DCTLPresetParams params) {
    // 钳制基础参数
    params.pivot_pq = clamp(params.pivot_pq, 0.05f, 0.30f);
    params.curve_type = (params.curve_type == 1) ? 1 : 0;
    
    // 钳制PPR参数
    params.gamma_s = clamp(params.gamma_s, 1.0f, 1.6f);
    params.gamma_h = clamp(params.gamma_h, 0.8f, 1.4f);
    params.shoulder_h = clamp(params.shoulder_h, 0.5f, 3.0f);
    
    // 钳制RLOG参数
    params.rlog_a = clamp(params.rlog_a, 1.0f, 16.0f);
    params.rlog_b = clamp(params.rlog_b, 0.8f, 1.2f);
    params.rlog_c = clamp(params.rlog_c, 0.5f, 3.0f);
    params.rlog_t = clamp(params.rlog_t, 0.4f, 0.7f);
    
    // 钳制通用参数
    params.black_lift = clamp(params.black_lift, 0.0f, 0.02f);
    params.highlight_detail = clamp(params.highlight_detail, 0.0f, 1.0f);
    params.sat_base = clamp(params.sat_base, 0.0f, 2.0f);
    params.sat_hi = clamp(params.sat_hi, 0.0f, 2.0f);
    params.yknee = clamp(params.yknee, 0.95f, 0.99f);
    params.alpha = clamp(params.alpha, 0.2f, 1.0f);
    params.toe = clamp(params.toe, 0.0f, 0.01f);
    
    return params;
}

/**
 * 检查参数是否有效（无NaN/Inf）
 */
#ifdef __DEVICE__
__DEVICE__ 
#endif
inline bool AreParamsValid(DCTLPresetParams params) {
    return isfinite(params.pivot_pq) &&
           isfinite(params.gamma_s) &&
           isfinite(params.gamma_h) &&
           isfinite(params.shoulder_h) &&
           isfinite(params.rlog_a) &&
           isfinite(params.rlog_b) &&
           isfinite(params.rlog_c) &&
           isfinite(params.rlog_t) &&
           isfinite(params.black_lift) &&
           isfinite(params.highlight_detail) &&
           isfinite(params.sat_base) &&
           isfinite(params.sat_hi) &&
           isfinite(params.yknee) &&
           isfinite(params.alpha) &&
           isfinite(params.toe);
}

// =============================================================================
// 统计信息结构
// =============================================================================

/**
 * 实时统计信息结构
 */
typedef struct {
    float min_pq_encoded_max_rgb;
    float avg_pq_encoded_max_rgb;
    float max_pq_encoded_max_rgb;
    float variance_pq_encoded_max_rgb;
    
    // 曲线验证状态
    bool is_monotonic;
    bool is_c1_continuous;
    float max_derivative_gap;
    
    // 性能指标
    float processing_time_ms;
    int processed_pixels;
} DCTLStatistics;

/**
 * 初始化统计信息
 */
#ifdef __DEVICE__
__DEVICE__ 
#endif
inline DCTLStatistics InitializeStatistics() {
    DCTLStatistics stats;
    stats.min_pq_encoded_max_rgb = 1.0f;
    stats.avg_pq_encoded_max_rgb = 0.0f;
    stats.max_pq_encoded_max_rgb = 0.0f;
    stats.variance_pq_encoded_max_rgb = 0.0f;
    stats.is_monotonic = true;
    stats.is_c1_continuous = true;
    stats.max_derivative_gap = 0.0f;
    stats.processing_time_ms = 0.0f;
    stats.processed_pixels = 0;
    return stats;
}

#ifdef __cplusplus
} // namespace DCTLMapping
} // namespace CinemaProHDR
#endif