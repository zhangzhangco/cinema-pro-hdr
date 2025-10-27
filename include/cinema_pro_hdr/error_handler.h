#pragma once

#include "cinema_pro_hdr/core.h"
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <functional>
#include <cmath>

namespace CinemaProHDR {

/**
 * @brief 三级回退策略枚举
 * 
 * 定义了系统遇到错误时的处理策略：
 * - PARAMETER_CORRECTION: 参数修正，钳制到有效范围
 * - STANDARD_FALLBACK: 标准回退，使用ST 2094-10基础层
 * - HARD_FALLBACK: 硬回退，回退到y=x线性映射
 */
enum class FallbackStrategy {
    PARAMETER_CORRECTION,  // 第一级：参数修正
    STANDARD_FALLBACK,     // 第二级：标准回退
    HARD_FALLBACK         // 第三级：硬回退
};

/**
 * @brief 日志节流器类
 * 
 * 实现日志节流策略，防止相同错误码的日志洪水：
 * - 每个错误码每秒最多10条日志
 * - 超出限制时输出聚合报告
 * - 线程安全的计数和时间窗口管理
 */
class LogThrottler {
public:
    LogThrottler();
    ~LogThrottler() = default;

    /**
     * @brief 检查是否应该记录日志
     * @param error_code 错误码
     * @return true 如果应该记录，false 如果应该节流
     */
    bool ShouldLog(ErrorCode error_code);

    /**
     * @brief 获取聚合报告
     * @param error_code 错误码
     * @return 聚合报告字符串，如果没有被节流的日志则返回空字符串
     */
    std::string GetAggregateReport(ErrorCode error_code);

    /**
     * @brief 重置节流器状态
     */
    void Reset();

private:
    struct ThrottleInfo {
        int count = 0;
        std::chrono::steady_clock::time_point window_start;
        int throttled_count = 0;
        std::chrono::steady_clock::time_point first_throttled;
        std::chrono::steady_clock::time_point last_throttled;
    };

    static constexpr int MAX_LOGS_PER_SECOND = 10;
    static constexpr std::chrono::seconds WINDOW_DURATION{1};

    std::unordered_map<ErrorCode, ThrottleInfo> throttle_map_;
    std::mutex mutex_;

    void UpdateWindow(ThrottleInfo& info);
};

/**
 * @brief 数值保护函数集合
 * 
 * 提供NaN/Inf检测和saturate()保护函数，确保数值稳定性
 */
class NumericalProtection {
public:
    /**
     * @brief 检查浮点数是否有效（非NaN且非Inf）
     * @param value 要检查的值
     * @return true 如果值有效
     */
    static bool IsValid(float value);

    /**
     * @brief 检查三维向量是否有效
     * @param x, y, z 向量分量
     * @return true 如果所有分量都有效
     */
    static bool IsValid(float x, float y, float z);

    /**
     * @brief 饱和函数，将值钳制到[0,1]范围
     * @param value 输入值
     * @return 钳制后的值
     */
    static float Saturate(float value);

    /**
     * @brief 安全除法，避免除零
     * @param numerator 分子
     * @param denominator 分母
     * @param fallback_value 分母为零时的回退值
     * @return 除法结果或回退值
     */
    static float SafeDivide(float numerator, float denominator, float fallback_value = 0.0f);

    /**
     * @brief 安全对数函数
     * @param value 输入值
     * @param fallback_value 无效输入时的回退值
     * @return 对数结果或回退值
     */
    static float SafeLog(float value, float fallback_value = 0.0f);

    /**
     * @brief 安全幂函数
     * @param base 底数
     * @param exponent 指数
     * @param fallback_value 无效输入时的回退值
     * @return 幂运算结果或回退值
     */
    static float SafePow(float base, float exponent, float fallback_value = 0.0f);

    /**
     * @brief 修复无效的浮点数
     * @param value 输入值
     * @param fallback_value 回退值
     * @return 有效的浮点数
     */
    static float FixInvalid(float value, float fallback_value = 0.0f);

    /**
     * @brief 修复无效的三维向量
     * @param x, y, z 向量分量的引用
     * @param fallback_value 回退值
     */
    static void FixInvalid(float& x, float& y, float& z, float fallback_value = 0.0f);
};

/**
 * @brief 错误处理器类
 * 
 * 实现完整的三级回退机制和错误报告系统：
 * - 参数验证和自动修正
 * - 分级错误处理策略
 * - 日志节流和聚合报告
 * - 线程安全的错误状态管理
 */
class ErrorHandler {
public:
    ErrorHandler();
    ~ErrorHandler() = default;

    /**
     * @brief 处理错误并应用回退策略
     * @param error_code 错误码
     * @param message 错误消息
     * @param field_name 相关字段名（可选）
     * @param invalid_value 无效值（可选）
     * @param clip_guid 片段GUID（可选）
     * @param timecode 时间码（可选）
     * @return 应用的回退策略
     */
    FallbackStrategy HandleError(
        ErrorCode error_code,
        const std::string& message,
        const std::string& field_name = "",
        float invalid_value = 0.0f,
        const std::string& clip_guid = "",
        const std::string& timecode = ""
    );

    /**
     * @brief 验证并修正参数
     * @param params 参数结构的引用
     * @return 如果参数被修正则返回true
     */
    bool ValidateAndCorrectParams(CphParams& params);

    /**
     * @brief 获取最后的错误报告
     * @return 错误报告结构
     */
    const ErrorReport& GetLastError() const { return last_error_; }

    /**
     * @brief 检查是否有活跃的错误
     * @return true 如果有未处理的错误
     */
    bool HasError() const { return last_error_.IsError(); }

    /**
     * @brief 清除错误状态
     */
    void ClearError() { last_error_ = ErrorReport(); }

    /**
     * @brief 获取当前回退策略
     * @return 当前的回退策略
     */
    FallbackStrategy GetCurrentFallbackStrategy() const { return current_strategy_; }

    /**
     * @brief 重置错误处理器状态
     */
    void Reset();

    /**
     * @brief 设置错误回调函数
     * @param callback 错误发生时调用的回调函数
     */
    void SetErrorCallback(std::function<void(const ErrorReport&)> callback);

    /**
     * @brief 获取所有聚合报告
     * @return 聚合报告的向量
     */
    std::vector<std::string> GetAggregateReports();

private:
    ErrorReport last_error_;
    FallbackStrategy current_strategy_;
    LogThrottler throttler_;
    std::function<void(const ErrorReport&)> error_callback_;
    std::mutex mutex_;

    // 参数验证辅助函数
    bool ValidateFloatRange(float& value, float min, float max, const std::string& field_name);
    void CorrectParameter(float& param, float min, float max, const std::string& field_name);
    
    // 回退策略决策
    FallbackStrategy DetermineFallbackStrategy(ErrorCode error_code);
    
    // 日志记录
    void LogError(const ErrorReport& error);
};

/**
 * @brief 全局错误处理器实例
 * 
 * 提供全局访问的错误处理器，确保整个系统使用统一的错误处理策略
 */
class GlobalErrorHandler {
public:
    static ErrorHandler& Instance();
    
    // 便捷的静态方法
    static FallbackStrategy HandleError(ErrorCode code, const std::string& message);
    static bool ValidateParams(CphParams& params);
    static void Reset();

private:
    static std::unique_ptr<ErrorHandler> instance_;
    static std::mutex instance_mutex_;
};

} // namespace CinemaProHDR