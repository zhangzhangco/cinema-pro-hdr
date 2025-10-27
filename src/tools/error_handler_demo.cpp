/**
 * @file error_handler_demo.cpp
 * @brief Cinema Pro HDR 错误处理系统演示程序
 * 
 * 这个程序演示了Cinema Pro HDR系统的完整错误处理机制：
 * - 三级回退策略
 * - 日志节流功能
 * - NaN/Inf保护
 * - 参数自动修正
 */

#include "cinema_pro_hdr/core.h"
#include "cinema_pro_hdr/error_handler.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <limits>

using namespace CinemaProHDR;

/**
 * @brief 演示数值保护功能
 */
void DemonstrateNumericalProtection() {
    std::cout << "\n=== 数值保护功能演示 ===" << std::endl;
    
    // 测试NaN/Inf检测
    float nan_val = std::numeric_limits<float>::quiet_NaN();
    float inf_val = std::numeric_limits<float>::infinity();
    float normal_val = 3.14159f;
    
    std::cout << "NaN检测: " << std::boolalpha 
              << NumericalProtection::IsValid(nan_val) << " (应该是false)" << std::endl;
    std::cout << "Inf检测: " << std::boolalpha 
              << NumericalProtection::IsValid(inf_val) << " (应该是false)" << std::endl;
    std::cout << "正常值检测: " << std::boolalpha 
              << NumericalProtection::IsValid(normal_val) << " (应该是true)" << std::endl;
    
    // 测试Saturate函数
    std::cout << "\nSaturate函数测试:" << std::endl;
    std::cout << "Saturate(-0.5) = " << NumericalProtection::Saturate(-0.5f) << std::endl;
    std::cout << "Saturate(0.7) = " << NumericalProtection::Saturate(0.7f) << std::endl;
    std::cout << "Saturate(1.5) = " << NumericalProtection::Saturate(1.5f) << std::endl;
    std::cout << "Saturate(NaN) = " << NumericalProtection::Saturate(nan_val) << std::endl;
    
    // 测试安全数学函数
    std::cout << "\n安全数学函数测试:" << std::endl;
    std::cout << "SafeDivide(10, 2) = " << NumericalProtection::SafeDivide(10.0f, 2.0f) << std::endl;
    std::cout << "SafeDivide(10, 0, 99) = " << NumericalProtection::SafeDivide(10.0f, 0.0f, 99.0f) << std::endl;
    std::cout << "SafeLog(2.718) = " << NumericalProtection::SafeLog(2.718f) << std::endl;
    std::cout << "SafeLog(-1, 99) = " << NumericalProtection::SafeLog(-1.0f, 99.0f) << std::endl;
    std::cout << "SafePow(2, 3) = " << NumericalProtection::SafePow(2.0f, 3.0f) << std::endl;
    std::cout << "SafePow(-2, 0.5, 99) = " << NumericalProtection::SafePow(-2.0f, 0.5f, 99.0f) << std::endl;
}

/**
 * @brief 演示日志节流功能
 */
void DemonstrateLogThrottling() {
    std::cout << "\n=== 日志节流功能演示 ===" << std::endl;
    
    LogThrottler throttler;
    
    std::cout << "连续发送15个相同错误码的日志请求:" << std::endl;
    
    int logged_count = 0;
    int throttled_count = 0;
    
    for (int i = 1; i <= 15; ++i) {
        bool should_log = throttler.ShouldLog(ErrorCode::RANGE_PIVOT);
        if (should_log) {
            std::cout << "  第" << i << "条: 记录日志" << std::endl;
            logged_count++;
        } else {
            std::cout << "  第" << i << "条: 被节流" << std::endl;
            throttled_count++;
        }
    }
    
    std::cout << "\n统计结果:" << std::endl;
    std::cout << "  记录的日志: " << logged_count << " 条" << std::endl;
    std::cout << "  被节流的日志: " << throttled_count << " 条" << std::endl;
    
    // 获取聚合报告
    std::string aggregate = throttler.GetAggregateReport(ErrorCode::RANGE_PIVOT);
    if (!aggregate.empty()) {
        std::cout << "  " << aggregate << std::endl;
    }
    
    // 测试不同错误码的独立计数
    std::cout << "\n测试不同错误码的独立计数:" << std::endl;
    std::cout << "RANGE_KNEE错误码: " << std::boolalpha 
              << throttler.ShouldLog(ErrorCode::RANGE_KNEE) << " (应该是true)" << std::endl;
}

/**
 * @brief 演示三级回退机制
 */
void DemonstrateFallbackStrategies() {
    std::cout << "\n=== 三级回退机制演示 ===" << std::endl;
    
    ErrorHandler handler;
    
    // 设置错误回调来显示详细信息
    handler.SetErrorCallback([](const ErrorReport& error) {
        std::cout << "  错误回调: " << error.ToString() << std::endl;
    });
    
    std::cout << "第一级回退 - 参数修正:" << std::endl;
    auto strategy1 = handler.HandleError(ErrorCode::RANGE_PIVOT, "枢轴参数超出范围", "pivot_pq", 0.5f);
    std::cout << "  回退策略: " << (strategy1 == FallbackStrategy::PARAMETER_CORRECTION ? "参数修正" : "其他") << std::endl;
    
    std::cout << "\n第二级回退 - 标准回退:" << std::endl;
    auto strategy2 = handler.HandleError(ErrorCode::DCI_BOUND, "DCI合规检查失败", "", 0.0f, "clip_123", "01:23:45:67");
    std::cout << "  回退策略: " << (strategy2 == FallbackStrategy::STANDARD_FALLBACK ? "标准回退" : "其他") << std::endl;
    
    std::cout << "\n第三级回退 - 硬回退:" << std::endl;
    auto strategy3 = handler.HandleError(ErrorCode::NAN_INF, "检测到NaN/Inf值");
    std::cout << "  回退策略: " << (strategy3 == FallbackStrategy::HARD_FALLBACK ? "硬回退" : "其他") << std::endl;
    
    // 显示最后的错误信息
    std::cout << "\n最后的错误报告:" << std::endl;
    std::cout << "  " << handler.GetLastError().ToString() << std::endl;
}

/**
 * @brief 演示参数验证和自动修正
 */
void DemonstrateParameterValidation() {
    std::cout << "\n=== 参数验证和自动修正演示 ===" << std::endl;
    
    // 创建包含无效参数的结构
    CphParams params;
    params.pivot_pq = -0.1f;  // 超出范围 [0.05, 0.30]
    params.gamma_s = 2.5f;    // 超出范围 [1.0, 1.6]
    params.gamma_h = std::numeric_limits<float>::quiet_NaN();  // NaN值
    params.shoulder_h = std::numeric_limits<float>::infinity(); // Inf值
    params.black_lift = -0.01f; // 超出范围 [0.0, 0.02]
    
    std::cout << "修正前的参数值:" << std::endl;
    std::cout << "  pivot_pq: " << params.pivot_pq << std::endl;
    std::cout << "  gamma_s: " << params.gamma_s << std::endl;
    std::cout << "  gamma_h: " << params.gamma_h << std::endl;
    std::cout << "  shoulder_h: " << params.shoulder_h << std::endl;
    std::cout << "  black_lift: " << params.black_lift << std::endl;
    std::cout << "  参数有效性: " << std::boolalpha << params.IsValid() << std::endl;
    
    // 使用全局错误处理器验证和修正参数
    bool corrected = GlobalErrorHandler::ValidateParams(params);
    
    std::cout << "\n修正后的参数值:" << std::endl;
    std::cout << "  pivot_pq: " << params.pivot_pq << std::endl;
    std::cout << "  gamma_s: " << params.gamma_s << std::endl;
    std::cout << "  gamma_h: " << params.gamma_h << std::endl;
    std::cout << "  shoulder_h: " << params.shoulder_h << std::endl;
    std::cout << "  black_lift: " << params.black_lift << std::endl;
    std::cout << "  参数有效性: " << std::boolalpha << params.IsValid() << std::endl;
    std::cout << "  是否进行了修正: " << std::boolalpha << corrected << std::endl;
}

/**
 * @brief 演示并发错误处理
 */
void DemonstrateConcurrentErrorHandling() {
    std::cout << "\n=== 并发错误处理演示 ===" << std::endl;
    
    ErrorHandler handler;
    std::atomic<int> total_errors{0};
    
    // 启动多个线程同时产生错误
    std::vector<std::thread> threads;
    const int num_threads = 4;
    const int errors_per_thread = 20;
    
    std::cout << "启动 " << num_threads << " 个线程，每个线程产生 " << errors_per_thread << " 个错误..." << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&handler, &total_errors, errors_per_thread, i]() {
            for (int j = 0; j < errors_per_thread; ++j) {
                // 交替产生不同类型的错误
                ErrorCode code = (j % 2 == 0) ? ErrorCode::RANGE_PIVOT : ErrorCode::RANGE_KNEE;
                std::string message = "线程" + std::to_string(i) + "错误" + std::to_string(j);
                
                handler.HandleError(code, message);
                total_errors++;
                
                // 短暂延迟以模拟实际处理时间
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "并发测试完成:" << std::endl;
    std::cout << "  总错误数: " << total_errors.load() << std::endl;
    std::cout << "  处理时间: " << duration.count() << " ms" << std::endl;
    std::cout << "  系统状态: " << (handler.HasError() ? "有错误" : "正常") << std::endl;
    
    // 获取聚合报告
    auto reports = handler.GetAggregateReports();
    if (!reports.empty()) {
        std::cout << "  聚合报告:" << std::endl;
        for (const auto& report : reports) {
            std::cout << "    " << report << std::endl;
        }
    }
}

/**
 * @brief 演示完整的错误处理工作流程
 */
void DemonstrateCompleteWorkflow() {
    std::cout << "\n=== 完整错误处理工作流程演示 ===" << std::endl;
    
    // 模拟一个典型的处理流程
    std::cout << "模拟Cinema Pro HDR处理流程中的错误处理..." << std::endl;
    
    // 1. 参数加载和验证
    std::cout << "\n步骤1: 参数加载和验证" << std::endl;
    CphParams params;
    params.pivot_pq = std::numeric_limits<float>::quiet_NaN(); // 模拟损坏的参数
    
    if (!params.IsValid()) {
        std::cout << "  检测到无效参数，启动自动修正..." << std::endl;
        GlobalErrorHandler::ValidateParams(params);
        std::cout << "  参数修正完成，pivot_pq = " << params.pivot_pq << std::endl;
    }
    
    // 2. 处理过程中的数值保护
    std::cout << "\n步骤2: 处理过程中的数值保护" << std::endl;
    float input_value = 0.8f;
    float gamma = params.gamma_s;
    
    // 模拟可能产生NaN的计算
    float result = NumericalProtection::SafePow(input_value, gamma);
    std::cout << "  安全幂运算: " << input_value << "^" << gamma << " = " << result << std::endl;
    
    // 3. 错误恢复和回退
    std::cout << "\n步骤3: 错误恢复和回退" << std::endl;
    if (std::rand() % 2 == 0) { // 随机模拟错误
        auto strategy = GlobalErrorHandler::HandleError(ErrorCode::GAMUT_OOG, "色域越界检测");
        std::cout << "  检测到色域越界，应用回退策略: ";
        switch (strategy) {
            case FallbackStrategy::PARAMETER_CORRECTION:
                std::cout << "参数修正" << std::endl;
                break;
            case FallbackStrategy::STANDARD_FALLBACK:
                std::cout << "标准回退 (使用ST 2094-10基础层)" << std::endl;
                break;
            case FallbackStrategy::HARD_FALLBACK:
                std::cout << "硬回退 (线性映射 y=x)" << std::endl;
                break;
        }
    }
    
    // 4. 最终状态报告
    std::cout << "\n步骤4: 最终状态报告" << std::endl;
    auto& handler = GlobalErrorHandler::Instance();
    if (handler.HasError()) {
        std::cout << "  处理过程中发生错误: " << handler.GetLastError().ToString() << std::endl;
    } else {
        std::cout << "  处理完成，无错误" << std::endl;
    }
    
    std::cout << "  当前回退策略: ";
    switch (handler.GetCurrentFallbackStrategy()) {
        case FallbackStrategy::PARAMETER_CORRECTION:
            std::cout << "参数修正模式" << std::endl;
            break;
        case FallbackStrategy::STANDARD_FALLBACK:
            std::cout << "标准回退模式" << std::endl;
            break;
        case FallbackStrategy::HARD_FALLBACK:
            std::cout << "硬回退模式" << std::endl;
            break;
    }
}

int main() {
    std::cout << "Cinema Pro HDR 错误处理系统演示程序" << std::endl;
    std::cout << "======================================" << std::endl;
    
    try {
        // 演示各个功能模块
        DemonstrateNumericalProtection();
        DemonstrateLogThrottling();
        DemonstrateFallbackStrategies();
        DemonstrateParameterValidation();
        DemonstrateConcurrentErrorHandling();
        DemonstrateCompleteWorkflow();
        
        std::cout << "\n======================================" << std::endl;
        std::cout << "所有演示完成！错误处理系统工作正常。" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "演示程序异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}