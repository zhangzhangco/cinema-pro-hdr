#include "cinema_pro_hdr/error_handler.h"
#include "test_framework.h"
#include <thread>
#include <chrono>
#include <cmath>
#include <limits>

using namespace CinemaProHDR;
using namespace TestFramework;

/**
 * @brief 测试数值保护函数 - IsValid
 */
TEST(NumericalProtection_IsValid) {
    // 测试有效值
    ASSERT_TRUE(NumericalProtection::IsValid(0.0f));
    ASSERT_TRUE(NumericalProtection::IsValid(1.0f));
    ASSERT_TRUE(NumericalProtection::IsValid(-1.0f));
    ASSERT_TRUE(NumericalProtection::IsValid(3.14159f));
    
    // 测试无效值
    ASSERT_FALSE(NumericalProtection::IsValid(std::numeric_limits<float>::quiet_NaN()));
    ASSERT_FALSE(NumericalProtection::IsValid(std::numeric_limits<float>::infinity()));
    ASSERT_FALSE(NumericalProtection::IsValid(-std::numeric_limits<float>::infinity()));
    
    // 测试三维向量
    ASSERT_TRUE(NumericalProtection::IsValid(1.0f, 2.0f, 3.0f));
    ASSERT_FALSE(NumericalProtection::IsValid(1.0f, std::numeric_limits<float>::quiet_NaN(), 3.0f));
    
    return true;
}

/**
 * @brief 测试数值保护函数 - Saturate
 */
TEST(NumericalProtection_Saturate) {
    // 正常范围内的值
    ASSERT_EQ(0.5f, NumericalProtection::Saturate(0.5f));
    ASSERT_EQ(0.0f, NumericalProtection::Saturate(0.0f));
    ASSERT_EQ(1.0f, NumericalProtection::Saturate(1.0f));
    
    // 超出范围的值
    ASSERT_EQ(0.0f, NumericalProtection::Saturate(-0.5f));
    ASSERT_EQ(1.0f, NumericalProtection::Saturate(1.5f));
    
    // 无效值
    ASSERT_EQ(0.0f, NumericalProtection::Saturate(std::numeric_limits<float>::quiet_NaN()));
    ASSERT_EQ(0.0f, NumericalProtection::Saturate(std::numeric_limits<float>::infinity()));
    
    return true;
}

/**
 * @brief 测试数值保护函数 - SafeDivide
 */
TEST(NumericalProtection_SafeDivide) {
    // 正常除法
    ASSERT_EQ(5.0f, NumericalProtection::SafeDivide(10.0f, 2.0f));
    
    // 除零保护
    ASSERT_EQ(99.0f, NumericalProtection::SafeDivide(10.0f, 0.0f, 99.0f));
    ASSERT_EQ(99.0f, NumericalProtection::SafeDivide(10.0f, 1e-10f, 99.0f));
    
    // NaN/Inf保护
    float nan_val = std::numeric_limits<float>::quiet_NaN();
    float inf_val = std::numeric_limits<float>::infinity();
    ASSERT_EQ(99.0f, NumericalProtection::SafeDivide(nan_val, 2.0f, 99.0f));
    ASSERT_EQ(99.0f, NumericalProtection::SafeDivide(10.0f, inf_val, 99.0f));
    
    return true;
}

/**
 * @brief 测试数值保护函数 - SafeLog
 */
TEST(NumericalProtection_SafeLog) {
    // 正常对数
    ASSERT_NEAR(1.0f, NumericalProtection::SafeLog(std::exp(1.0f)), 1e-6f);
    
    // 无效输入保护
    ASSERT_EQ(99.0f, NumericalProtection::SafeLog(0.0f, 99.0f));
    ASSERT_EQ(99.0f, NumericalProtection::SafeLog(-1.0f, 99.0f));
    
    float nan_val = std::numeric_limits<float>::quiet_NaN();
    ASSERT_EQ(99.0f, NumericalProtection::SafeLog(nan_val, 99.0f));
    
    return true;
}

/**
 * @brief 测试数值保护函数 - SafePow
 */
TEST(NumericalProtection_SafePow) {
    // 正常幂运算
    ASSERT_NEAR(8.0f, NumericalProtection::SafePow(2.0f, 3.0f), 1e-6f);
    
    // 特殊情况保护
    ASSERT_EQ(99.0f, NumericalProtection::SafePow(0.0f, -1.0f, 99.0f));
    ASSERT_EQ(99.0f, NumericalProtection::SafePow(-2.0f, 0.5f, 99.0f)); // 负数的非整数幂
    
    // NaN/Inf保护
    float nan_val = std::numeric_limits<float>::quiet_NaN();
    ASSERT_EQ(99.0f, NumericalProtection::SafePow(nan_val, 2.0f, 99.0f));
    ASSERT_EQ(99.0f, NumericalProtection::SafePow(2.0f, nan_val, 99.0f));
    
    return true;
}

/**
 * @brief 测试数值保护函数 - FixInvalid
 */
TEST(NumericalProtection_FixInvalid) {
    // 有效值不变
    ASSERT_EQ(3.14f, NumericalProtection::FixInvalid(3.14f, 0.0f));
    
    // 无效值修复
    float nan_val = std::numeric_limits<float>::quiet_NaN();
    ASSERT_EQ(42.0f, NumericalProtection::FixInvalid(nan_val, 42.0f));
    
    return true;
}

/**
 * @brief 测试日志节流器 - 基本功能
 */
TEST(LogThrottler_Basic) {
    LogThrottler throttler;
    
    // 前10条应该都能记录
    for (int i = 0; i < 10; ++i) {
        ASSERT_TRUE(throttler.ShouldLog(ErrorCode::RANGE_PIVOT));
    }
    
    // 第11条应该被节流
    ASSERT_FALSE(throttler.ShouldLog(ErrorCode::RANGE_PIVOT));
    
    return true;
}

/**
 * @brief 测试日志节流器 - 独立计数
 */
TEST(LogThrottler_IndependentCounting) {
    LogThrottler throttler;
    
    // 不同错误码应该独立计数
    for (int i = 0; i < 10; ++i) {
        ASSERT_TRUE(throttler.ShouldLog(ErrorCode::RANGE_PIVOT));
        ASSERT_TRUE(throttler.ShouldLog(ErrorCode::RANGE_KNEE));
    }
    
    // 各自的第11条都应该被节流
    ASSERT_FALSE(throttler.ShouldLog(ErrorCode::RANGE_PIVOT));
    ASSERT_FALSE(throttler.ShouldLog(ErrorCode::RANGE_KNEE));
    
    return true;
}

/**
 * @brief 测试错误处理器 - 回退策略
 */
TEST(ErrorHandler_FallbackStrategies) {
    ErrorHandler handler;
    
    // 第一级：参数修正
    auto strategy1 = handler.HandleError(ErrorCode::RANGE_PIVOT, "参数越界");
    ASSERT_TRUE(strategy1 == FallbackStrategy::PARAMETER_CORRECTION);
    
    auto strategy2 = handler.HandleError(ErrorCode::RANGE_KNEE, "膝点参数越界");
    ASSERT_TRUE(strategy2 == FallbackStrategy::PARAMETER_CORRECTION);
    
    // 第二级：标准回退
    auto strategy3 = handler.HandleError(ErrorCode::SCHEMA_MISSING, "缺少schema");
    ASSERT_TRUE(strategy3 == FallbackStrategy::STANDARD_FALLBACK);
    
    auto strategy4 = handler.HandleError(ErrorCode::DCI_BOUND, "DCI边界违规");
    ASSERT_TRUE(strategy4 == FallbackStrategy::STANDARD_FALLBACK);
    
    // 第三级：硬回退
    auto strategy5 = handler.HandleError(ErrorCode::NAN_INF, "数值异常");
    ASSERT_TRUE(strategy5 == FallbackStrategy::HARD_FALLBACK);
    
    return true;
}

/**
 * @brief 测试错误处理器 - 参数验证
 */
TEST(ErrorHandler_ParameterValidation) {
    ErrorHandler handler;
    
    // 创建包含无效参数的结构
    CphParams params;
    params.pivot_pq = -0.1f;  // 应该在[0.05, 0.30]
    params.gamma_s = 2.0f;    // 应该在[1.0, 1.6]
    params.gamma_h = 0.5f;    // 应该在[0.8, 1.4]
    
    bool corrected = handler.ValidateAndCorrectParams(params);
    ASSERT_TRUE(corrected);
    
    // 验证参数被正确修正
    ASSERT_TRUE(params.pivot_pq >= 0.05f && params.pivot_pq <= 0.30f);
    ASSERT_TRUE(params.gamma_s >= 1.0f && params.gamma_s <= 1.6f);
    ASSERT_TRUE(params.gamma_h >= 0.8f && params.gamma_h <= 1.4f);
    
    return true;
}

/**
 * @brief 测试错误处理器 - NaN/Inf处理
 */
TEST(ErrorHandler_NaNInfHandling) {
    ErrorHandler handler;
    
    CphParams params;
    
    // 设置NaN参数
    params.pivot_pq = std::numeric_limits<float>::quiet_NaN();
    params.gamma_s = std::numeric_limits<float>::infinity();
    
    bool corrected = handler.ValidateAndCorrectParams(params);
    ASSERT_TRUE(corrected);
    
    // 验证NaN/Inf被修正为有效值
    ASSERT_TRUE(NumericalProtection::IsValid(params.pivot_pq));
    ASSERT_TRUE(NumericalProtection::IsValid(params.gamma_s));
    ASSERT_TRUE(params.pivot_pq >= 0.05f && params.pivot_pq <= 0.30f);
    ASSERT_TRUE(params.gamma_s >= 1.0f && params.gamma_s <= 1.6f);
    
    return true;
}

/**
 * @brief 测试错误处理器 - 错误状态管理
 */
TEST(ErrorHandler_ErrorStateManagement) {
    ErrorHandler handler;
    
    // 初始状态无错误
    ASSERT_FALSE(handler.HasError());
    
    // 处理错误后有错误状态
    handler.HandleError(ErrorCode::RANGE_PIVOT, "测试错误");
    ASSERT_TRUE(handler.HasError());
    
    // 清除错误状态
    handler.ClearError();
    ASSERT_FALSE(handler.HasError());
    
    return true;
}

/**
 * @brief 测试全局错误处理器 - 单例访问
 */
TEST(GlobalErrorHandler_Singleton) {
    auto& handler1 = GlobalErrorHandler::Instance();
    auto& handler2 = GlobalErrorHandler::Instance();
    
    // 应该是同一个实例
    ASSERT_TRUE(&handler1 == &handler2);
    
    return true;
}

/**
 * @brief 测试全局错误处理器 - 静态方法
 */
TEST(GlobalErrorHandler_StaticMethods) {
    // 重置状态
    GlobalErrorHandler::Reset();
    
    // 测试静态错误处理
    auto strategy = GlobalErrorHandler::HandleError(ErrorCode::RANGE_PIVOT, "全局测试");
    ASSERT_TRUE(strategy == FallbackStrategy::PARAMETER_CORRECTION);
    
    // 测试静态参数验证
    CphParams params;
    params.pivot_pq = -1.0f; // 越界值
    
    bool corrected = GlobalErrorHandler::ValidateParams(params);
    ASSERT_TRUE(corrected);
    ASSERT_TRUE(params.pivot_pq >= 0.05f);
    
    return true;
}

/**
 * @brief 测试CphParams集成
 */
TEST(ErrorHandler_CphParamsIntegration) {
    CphParams params;
    
    // 测试IsValid方法是否与错误处理器一致
    params.pivot_pq = std::numeric_limits<float>::quiet_NaN();
    ASSERT_FALSE(params.IsValid());
    
    // 使用错误处理器修正后应该有效
    GlobalErrorHandler::ValidateParams(params);
    ASSERT_TRUE(params.IsValid());
    
    return true;
}