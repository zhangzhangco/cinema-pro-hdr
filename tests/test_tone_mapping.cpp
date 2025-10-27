#include "cinema_pro_hdr/tone_mapping.h"
#include "test_framework.h"
#include <cmath>
#include <vector>
#include <algorithm>

using namespace CinemaProHDR;

/**
 * @brief 测试PPR色调映射算法的基本功能
 */
TEST(ToneMapper_PPR_Basic) {
    ToneMapper mapper;
    
    // 创建PPR参数
    CphParams params;
    params.curve = CurveType::PPR;
    params.pivot_pq = 0.18f;
    params.gamma_s = 1.25f;
    params.gamma_h = 1.10f;
    params.shoulder_h = 1.5f;
    params.yknee = 0.97f;
    params.alpha = 0.6f;
    params.toe = 0.002f;
    
    // 初始化映射器
    ASSERT_TRUE(mapper.Initialize(params));
    
    // 测试边界值
    ASSERT_EQ(0.0f, mapper.ApplyToneMapping(0.0f));
    ASSERT_LE(mapper.ApplyToneMapping(1.0f), 1.0f);
    
    // 测试单调性：输入增加，输出也应该增加
    float prev_output = -1.0f;
    for (int i = 0; i <= 100; ++i) {
        float input = static_cast<float>(i) / 100.0f;
        float output = mapper.ApplyToneMapping(input);
        
        ASSERT_GE(output, prev_output);
        prev_output = output;
    }
    
    return true;
}

/**
 * @brief 测试PPR算法的参数验证
 */
TEST(ToneMapper_PPR_ParameterValidation) {
    ToneMapper mapper;
    
    // 测试无效参数
    CphParams invalid_params;
    invalid_params.curve = CurveType::PPR;
    invalid_params.gamma_s = 2.0f; // 超出范围 [1.0, 1.6]
    invalid_params.gamma_h = 0.5f; // 超出范围 [0.8, 1.4]
    invalid_params.shoulder_h = 4.0f; // 超出范围 [0.5, 3.0]
    
    // 初始化应该失败或自动修正参数
    bool init_result = mapper.Initialize(invalid_params);
    
    // 如果初始化成功，说明参数被自动修正了
    if (init_result) {
        const CphParams& corrected = mapper.GetParams();
        ASSERT_GE(corrected.gamma_s, 1.0f);
        ASSERT_LE(corrected.gamma_s, 1.6f);
        ASSERT_GE(corrected.gamma_h, 0.8f);
        ASSERT_LE(corrected.gamma_h, 1.4f);
        ASSERT_GE(corrected.shoulder_h, 0.5f);
        ASSERT_LE(corrected.shoulder_h, 3.0f);
    }
    
    return true;
}

/**
 * @brief 测试PPR算法的单调性验证
 */
TEST(ToneMapper_PPR_MonotonicityValidation) {
    ToneMapper mapper;
    
    // 使用标准参数
    CphParams params;
    params.curve = CurveType::PPR;
    params.pivot_pq = 0.18f;
    params.gamma_s = 1.25f;
    params.gamma_h = 1.10f;
    params.shoulder_h = 1.5f;
    
    ASSERT_TRUE(mapper.Initialize(params));
    
    // 验证单调性
    ASSERT_TRUE(mapper.ValidateMonotonicity());
    
    return true;
}

/**
 * @brief 测试PPR算法的C¹连续性验证
 */
TEST(ToneMapper_PPR_C1ContinuityValidation) {
    ToneMapper mapper;
    
    // 使用标准参数
    CphParams params;
    params.curve = CurveType::PPR;
    params.pivot_pq = 0.18f;
    params.gamma_s = 1.25f;
    params.gamma_h = 1.10f;
    params.shoulder_h = 1.5f;
    
    ASSERT_TRUE(mapper.Initialize(params));
    
    // 验证C¹连续性（使用更宽松的阈值，考虑数值计算误差）
    ASSERT_TRUE(mapper.ValidateC1Continuity(1e-2f, 1.0f));
    
    return true;
}

/**
 * @brief 测试软膝和toe夹持功能
 */
TEST(ToneMapper_PPR_SoftKneeAndToe) {
    ToneMapper mapper;
    
    CphParams params;
    params.curve = CurveType::PPR;
    params.pivot_pq = 0.18f;
    params.gamma_s = 1.25f;
    params.gamma_h = 1.10f;
    params.shoulder_h = 1.5f;
    params.yknee = 0.95f;  // 较低的膝点，更容易测试
    params.alpha = 0.5f;
    params.toe = 0.01f;    // 较高的toe值，更容易测试
    
    ASSERT_TRUE(mapper.Initialize(params));
    
    // 测试toe夹持：输出应该不小于toe值
    float low_output = mapper.ApplyToneMapping(0.001f);
    ASSERT_GE(low_output, params.toe);
    
    // 测试软膝：高值应该被压缩
    float high_input = 0.99f;
    float high_output = mapper.ApplyToneMapping(high_input);
    ASSERT_LT(high_output, high_input); // 应该被压缩
    
    return true;
}

/**
 * @brief 测试批量处理功能
 */
TEST(ToneMapper_PPR_BatchProcessing) {
    ToneMapper mapper;
    
    CphParams params;
    params.curve = CurveType::PPR;
    params.pivot_pq = 0.18f;
    params.gamma_s = 1.25f;
    params.gamma_h = 1.10f;
    params.shoulder_h = 1.5f;
    
    ASSERT_TRUE(mapper.Initialize(params));
    
    // 准备测试数据
    const size_t count = 100;
    std::vector<float> input(count);
    std::vector<float> output(count);
    std::vector<float> reference(count);
    
    // 生成输入数据
    for (size_t i = 0; i < count; ++i) {
        input[i] = static_cast<float>(i) / static_cast<float>(count - 1);
    }
    
    // 批量处理
    mapper.ApplyToneMappingBatch(input.data(), output.data(), count);
    
    // 单独处理作为参考
    for (size_t i = 0; i < count; ++i) {
        reference[i] = mapper.ApplyToneMapping(input[i]);
    }
    
    // 比较结果
    for (size_t i = 0; i < count; ++i) {
        ASSERT_NEAR(output[i], reference[i], 1e-6f);
    }
    
    return true;
}

/**
 * @brief 测试数值稳定性
 */
TEST(ToneMapper_PPR_NumericalStability) {
    ToneMapper mapper;
    
    CphParams params;
    params.curve = CurveType::PPR;
    params.pivot_pq = 0.18f;
    params.gamma_s = 1.25f;
    params.gamma_h = 1.10f;
    params.shoulder_h = 1.5f;
    
    ASSERT_TRUE(mapper.Initialize(params));
    
    // 测试极值输入
    float result;
    
    // 测试NaN输入
    result = mapper.ApplyToneMapping(std::numeric_limits<float>::quiet_NaN());
    ASSERT_TRUE(std::isfinite(result));
    
    // 测试无穷大输入
    result = mapper.ApplyToneMapping(std::numeric_limits<float>::infinity());
    ASSERT_TRUE(std::isfinite(result));
    ASSERT_LE(result, 1.0f);
    
    // 测试负无穷大输入
    result = mapper.ApplyToneMapping(-std::numeric_limits<float>::infinity());
    ASSERT_TRUE(std::isfinite(result));
    ASSERT_GE(result, 0.0f);
    
    // 测试超出范围的输入
    result = mapper.ApplyToneMapping(-1.0f);
    ASSERT_GE(result, 0.0f);
    
    result = mapper.ApplyToneMapping(2.0f);
    ASSERT_LE(result, 1.0f);
    
    return true;
}

/**
 * @brief 测试工具函数
 */
TEST(ToneMappingUtils_Functions) {
    // 测试PPR评估函数
    float result = ToneMappingUtils::EvaluatePPR(0.5f, 0.18f, 1.25f, 1.10f, 1.5f);
    ASSERT_TRUE(std::isfinite(result));
    ASSERT_GE(result, 0.0f);
    ASSERT_LE(result, 1.0f);
    
    // 测试软膝函数
    result = ToneMappingUtils::SoftKnee(0.98f, 0.95f, 0.5f);
    ASSERT_TRUE(std::isfinite(result));
    ASSERT_LE(result, 0.98f); // 应该被压缩
    
    // 测试toe夹持函数
    result = ToneMappingUtils::ToeClamp(0.001f, 0.01f);
    ASSERT_GE(result, 0.01f); // 应该被提升到toe值
    
    return true;
}

/**
 * @brief 测试枢轴点附近的行为
 */
TEST(ToneMapper_PPR_PivotBehavior) {
    ToneMapper mapper;
    
    CphParams params;
    params.curve = CurveType::PPR;
    params.pivot_pq = 0.18f;
    params.gamma_s = 1.25f;
    params.gamma_h = 1.10f;
    params.shoulder_h = 1.5f;
    
    ASSERT_TRUE(mapper.Initialize(params));
    
    // 测试枢轴点附近的平滑过渡
    float pivot = params.pivot_pq;
    float before_pivot = mapper.ApplyToneMapping(pivot - 0.01f);
    float at_pivot = mapper.ApplyToneMapping(pivot);
    float after_pivot = mapper.ApplyToneMapping(pivot + 0.01f);
    
    // 应该是单调递增的
    ASSERT_LE(before_pivot, at_pivot);
    ASSERT_LE(at_pivot, after_pivot);
    
    // 在枢轴点附近应该相对平滑（没有大的跳跃）
    float diff1 = at_pivot - before_pivot;
    float diff2 = after_pivot - at_pivot;
    float ratio = std::max(diff1, diff2) / std::min(diff1, diff2);
    ASSERT_LT(ratio, 3.0f); // 差异不应该超过3倍
    
    return true;
}

/**
 * @brief 测试RLOG色调映射算法的基本功能
 */
TEST(ToneMapper_RLOG_Basic) {
    ToneMapper mapper;
    
    // 创建RLOG参数
    CphParams params;
    params.curve = CurveType::RLOG;
    params.rlog_a = 8.0f;
    params.rlog_b = 1.0f;
    params.rlog_c = 1.5f;
    params.rlog_t = 0.55f;
    params.yknee = 0.97f;
    params.alpha = 0.6f;
    params.toe = 0.002f;
    
    // 初始化映射器
    ASSERT_TRUE(mapper.Initialize(params));
    
    // 测试边界值
    ASSERT_EQ(0.0f, mapper.ApplyToneMapping(0.0f));
    ASSERT_LE(mapper.ApplyToneMapping(1.0f), 1.0f);
    
    // 测试单调性：输入增加，输出也应该增加
    float prev_output = -1.0f;
    for (int i = 0; i <= 100; ++i) {
        float input = static_cast<float>(i) / 100.0f;
        float output = mapper.ApplyToneMapping(input);
        
        ASSERT_GE(output, prev_output);
        prev_output = output;
    }
    
    return true;
}

/**
 * @brief 测试RLOG算法的参数验证
 */
TEST(ToneMapper_RLOG_ParameterValidation) {
    ToneMapper mapper;
    
    // 测试无效参数
    CphParams invalid_params;
    invalid_params.curve = CurveType::RLOG;
    invalid_params.rlog_a = 20.0f; // 超出范围 [1, 16]
    invalid_params.rlog_b = 0.5f;  // 超出范围 [0.8, 1.2]
    invalid_params.rlog_c = 4.0f;  // 超出范围 [0.5, 3.0]
    invalid_params.rlog_t = 0.8f;  // 超出范围 [0.4, 0.7]
    
    // 初始化应该失败或自动修正参数
    bool init_result = mapper.Initialize(invalid_params);
    
    // 如果初始化成功，说明参数被自动修正了
    if (init_result) {
        const CphParams& corrected = mapper.GetParams();
        ASSERT_GE(corrected.rlog_a, 1.0f);
        ASSERT_LE(corrected.rlog_a, 16.0f);
        ASSERT_GE(corrected.rlog_b, 0.8f);
        ASSERT_LE(corrected.rlog_b, 1.2f);
        ASSERT_GE(corrected.rlog_c, 0.5f);
        ASSERT_LE(corrected.rlog_c, 3.0f);
        ASSERT_GE(corrected.rlog_t, 0.4f);
        ASSERT_LE(corrected.rlog_t, 0.7f);
    }
    
    return true;
}

/**
 * @brief 测试RLOG算法的单调性验证
 */
TEST(ToneMapper_RLOG_MonotonicityValidation) {
    ToneMapper mapper;
    
    // 使用标准参数
    CphParams params;
    params.curve = CurveType::RLOG;
    params.rlog_a = 8.0f;
    params.rlog_b = 1.0f;
    params.rlog_c = 1.5f;
    params.rlog_t = 0.55f;
    
    ASSERT_TRUE(mapper.Initialize(params));
    
    // 验证单调性
    ASSERT_TRUE(mapper.ValidateMonotonicity());
    
    return true;
}

/**
 * @brief 测试RLOG算法的拼接区间处理
 */
TEST(ToneMapper_RLOG_BlendingRegion) {
    ToneMapper mapper;
    
    CphParams params;
    params.curve = CurveType::RLOG;
    params.rlog_a = 8.0f;
    params.rlog_b = 1.0f;
    params.rlog_c = 1.5f;
    params.rlog_t = 0.55f;
    
    ASSERT_TRUE(mapper.Initialize(params));
    
    // 测试拼接区间的平滑过渡
    float t = params.rlog_t;
    float blend_range = 0.05f;
    
    float before_blend = mapper.ApplyToneMapping(t - blend_range - 0.01f);
    float in_blend_left = mapper.ApplyToneMapping(t - blend_range / 2.0f);
    float in_blend_center = mapper.ApplyToneMapping(t);
    float in_blend_right = mapper.ApplyToneMapping(t + blend_range / 2.0f);
    float after_blend = mapper.ApplyToneMapping(t + blend_range + 0.01f);
    
    // 应该是单调递增的
    ASSERT_LE(before_blend, in_blend_left);
    ASSERT_LE(in_blend_left, in_blend_center);
    ASSERT_LE(in_blend_center, in_blend_right);
    ASSERT_LE(in_blend_right, after_blend);
    
    // 在拼接区间内应该相对平滑（没有大的跳跃）
    float max_step = 0.0f;
    std::vector<float> values = {before_blend, in_blend_left, in_blend_center, in_blend_right, after_blend};
    for (size_t i = 1; i < values.size(); ++i) {
        float step = values[i] - values[i-1];
        max_step = std::max(max_step, step);
    }
    
    // 最大步长不应该太大（相对于输入范围）
    ASSERT_LT(max_step, 0.1f);
    
    return true;
}

/**
 * @brief 测试RLOG工具函数
 */
TEST(ToneMappingUtils_RLOG_Functions) {
    // 测试RLOG评估函数
    float result = ToneMappingUtils::EvaluateRLOG(0.5f, 8.0f, 1.0f, 1.5f, 0.55f);
    ASSERT_TRUE(std::isfinite(result));
    ASSERT_GE(result, 0.0f);
    ASSERT_LE(result, 1.0f);
    
    // 测试边界值
    result = ToneMappingUtils::EvaluateRLOG(0.0f, 8.0f, 1.0f, 1.5f, 0.55f);
    ASSERT_EQ(0.0f, result);
    
    result = ToneMappingUtils::EvaluateRLOG(1.0f, 8.0f, 1.0f, 1.5f, 0.55f);
    ASSERT_TRUE(std::isfinite(result));
    ASSERT_LE(result, 1.0f);
    
    return true;
}