# Cinema Pro HDR 错误处理系统文档

## 概述

Cinema Pro HDR 错误处理系统是一个完整的三级回退机制，旨在确保系统在各种异常情况下的稳定性和可靠性。系统包含数值保护、日志节流、参数自动修正和分级错误处理策略。

## 核心特性

### 🛡️ 三级回退机制

系统采用分层的错误处理策略，确保在不同严重程度的错误下都能提供合适的回退方案：

1. **第一级：参数修正** (`PARAMETER_CORRECTION`)
   - 适用于：参数越界、轻微数值问题
   - 处理方式：自动钳制到有效范围，记录警告日志
   - 错误码：`RANGE_PIVOT`, `RANGE_KNEE`

2. **第二级：标准回退** (`STANDARD_FALLBACK`)
   - 适用于：配置缺失、合规性问题
   - 处理方式：使用ST 2094-10基础层参数渲染
   - 错误码：`SCHEMA_MISSING`, `DCI_BOUND`, `GAMUT_OOG`

3. **第三级：硬回退** (`HARD_FALLBACK`)
   - 适用于：严重数值异常、算法崩溃
   - 处理方式：回退到线性映射 y=x
   - 错误码：`NAN_INF`

### 🔢 数值保护功能

提供全面的数值稳定性保护，防止NaN/Inf传播：

```cpp
// 检查数值有效性
bool is_valid = NumericalProtection::IsValid(value);

// 安全数学运算
float result = NumericalProtection::SafePow(base, exponent, fallback);
float log_result = NumericalProtection::SafeLog(value, fallback);
float div_result = NumericalProtection::SafeDivide(num, den, fallback);

// 值域钳制
float clamped = NumericalProtection::Saturate(value); // [0,1]

// 修复无效值
float fixed = NumericalProtection::FixInvalid(value, fallback);
```

### 📊 日志节流策略

防止日志洪水，实现智能的错误报告聚合：

- **节流限制**：每个错误码每秒最多10条日志
- **聚合报告**：超出限制时生成包含次数和时间范围的聚合报告
- **独立计数**：不同错误码独立计数，互不影响
- **线程安全**：支持多线程并发访问

### ⚙️ 参数自动修正

智能的参数验证和修正机制：

```cpp
CphParams params;
// 设置可能无效的参数...

// 自动验证和修正
bool corrected = GlobalErrorHandler::ValidateParams(params);
if (corrected) {
    // 参数已被自动修正到有效范围
    assert(params.IsValid());
}
```

## 错误码定义

| 错误码 | 名称 | 级别 | 回退策略 | 描述 |
|--------|------|------|----------|------|
| `SUCCESS` | 成功 | INFO | - | 无错误 |
| `SCHEMA_MISSING` | 配置缺失 | ERROR | 标准回退 | 缺少必填配置块或字段 |
| `RANGE_PIVOT` | 参数越界 | WARN | 参数修正 | 枢轴或其他参数超出有效范围 |
| `RANGE_KNEE` | 膝点越界 | WARN | 参数修正 | 软膝参数超出有效范围 |
| `NAN_INF` | 数值异常 | ERROR | 硬回退 | 检测到NaN或Inf值 |
| `DET_MISMATCH` | 一致性问题 | WARN | 标准回退 | 跨平台数值不一致 |
| `HL_FLICKER` | 闪烁检测 | WARN | 标准回退 | 高光细节引起闪烁超阈值 |
| `DCI_BOUND` | DCI违规 | ERROR | 标准回退 | DCI合规检查失败 |
| `GAMUT_OOG` | 色域越界 | ERROR | 标准回退 | 色域越界检测 |

## 使用方法

### 基本错误处理

```cpp
#include "cinema_pro_hdr/error_handler.h"

// 使用全局错误处理器
auto strategy = GlobalErrorHandler::HandleError(
    ErrorCode::RANGE_PIVOT, 
    "参数超出范围",
    "pivot_pq",     // 字段名
    0.5f,           // 无效值
    "clip_123",     // 片段GUID
    "01:23:45:67"   // 时间码
);

// 检查回退策略
switch (strategy) {
    case FallbackStrategy::PARAMETER_CORRECTION:
        // 参数已修正，继续处理
        break;
    case FallbackStrategy::STANDARD_FALLBACK:
        // 使用ST 2094-10基础层
        break;
    case FallbackStrategy::HARD_FALLBACK:
        // 使用线性映射 y=x
        break;
}
```

### 参数验证

```cpp
CphParams params;
// ... 设置参数 ...

// 验证并自动修正参数
if (GlobalErrorHandler::ValidateParams(params)) {
    std::cout << "参数已自动修正" << std::endl;
}

// 检查参数有效性
if (params.IsValid()) {
    // 参数现在是有效的
}
```

### 数值保护

```cpp
// 在算法中使用数值保护
float ProcessValue(float input, float gamma) {
    // 检查输入有效性
    if (!NumericalProtection::IsValid(input)) {
        GlobalErrorHandler::HandleError(ErrorCode::NAN_INF, "输入值无效");
        return 0.0f;
    }
    
    // 安全的幂运算
    float result = NumericalProtection::SafePow(input, gamma, input);
    
    // 确保输出在有效范围内
    return NumericalProtection::Saturate(result);
}
```

### 错误回调

```cpp
// 设置自定义错误处理回调
GlobalErrorHandler::Instance().SetErrorCallback([](const ErrorReport& error) {
    // 自定义错误处理逻辑
    std::cerr << "错误发生: " << error.ToString() << std::endl;
    
    // 可以发送到日志系统、监控系统等
    if (error.code == ErrorCode::NAN_INF) {
        // 严重错误，可能需要特殊处理
        NotifyMonitoringSystem(error);
    }
});
```

## 日志格式

错误日志采用统一的格式，便于解析和分析：

```
[时间戳][级别][片段GUID][时间码] code=错误码, field=字段名, val=值, action=处理动作 - 错误消息
```

示例：
```
[2025-10-24 11:29:36][WARN][clip_123][01:23:45:67] code=2, field=pivot_pq, val=0.5, action=PARAM_CORRECT - 枢轴参数超出范围
```

### 聚合报告格式

当日志被节流时，系统会生成聚合报告：

```
聚合报告: 错误码 2 被节流 5 次, 时间范围: 1234ms
```

## 性能特性

### 线程安全

- 所有错误处理操作都是线程安全的
- 使用细粒度锁定，最小化性能影响
- 支持高并发场景下的错误处理

### 内存效率

- 错误报告结构体设计紧凑
- 日志节流避免内存泄漏
- 智能的聚合报告减少存储需求

### 性能开销

- 数值检查：< 1% CPU开销
- 错误处理：< 0.1ms 延迟
- 日志节流：O(1) 时间复杂度

## 最佳实践

### 1. 预防性检查

在关键计算前进行预防性检查：

```cpp
// 在色调映射前检查参数
if (!params.IsValid()) {
    GlobalErrorHandler::ValidateParams(params);
}

// 在数学运算前检查输入
if (!NumericalProtection::IsValid(input)) {
    return fallback_value;
}
```

### 2. 分层错误处理

根据错误严重程度采用不同策略：

```cpp
void ProcessFrame(const Image& input, Image& output) {
    try {
        // 主处理逻辑
        ProcessCore(input, output);
    } catch (const NumericalException& e) {
        // 数值异常 - 硬回退
        ApplyLinearMapping(input, output);
    } catch (const ParameterException& e) {
        // 参数异常 - 参数修正
        CorrectParametersAndRetry(input, output);
    }
}
```

### 3. 错误恢复

实现优雅的错误恢复机制：

```cpp
bool ProcessWithFallback(const CphParams& params, const Image& input, Image& output) {
    // 尝试完整处理
    if (TryFullProcessing(params, input, output)) {
        return true;
    }
    
    // 回退到基础层
    if (TryBasicProcessing(params, input, output)) {
        GlobalErrorHandler::HandleError(ErrorCode::SCHEMA_MISSING, "回退到基础层");
        return true;
    }
    
    // 最终回退到线性映射
    ApplyLinearMapping(input, output);
    GlobalErrorHandler::HandleError(ErrorCode::NAN_INF, "硬回退到线性映射");
    return false;
}
```

### 4. 监控和诊断

建立完善的监控体系：

```cpp
// 定期检查错误状态
void MonitorErrorState() {
    auto& handler = GlobalErrorHandler::Instance();
    
    if (handler.HasError()) {
        const auto& error = handler.GetLastError();
        
        // 记录到监控系统
        LogToMonitoring(error);
        
        // 检查是否需要告警
        if (error.code == ErrorCode::NAN_INF) {
            TriggerAlert("严重数值异常");
        }
    }
    
    // 获取聚合报告
    auto reports = handler.GetAggregateReports();
    for (const auto& report : reports) {
        LogAggregateReport(report);
    }
}
```

## 故障排除

### 常见问题

1. **参数频繁被修正**
   - 检查参数来源的有效性
   - 验证参数计算逻辑
   - 考虑调整参数范围

2. **日志被大量节流**
   - 分析错误根本原因
   - 检查是否存在系统性问题
   - 考虑提高节流阈值

3. **频繁硬回退**
   - 检查数值计算的稳定性
   - 验证输入数据的有效性
   - 考虑增加更多的数值保护

### 调试技巧

```cpp
// 启用详细错误报告
GlobalErrorHandler::Instance().SetErrorCallback([](const ErrorReport& error) {
    std::cout << "详细错误信息: " << error.ToString() << std::endl;
    std::cout << "  错误码: " << static_cast<int>(error.code) << std::endl;
    std::cout << "  字段: " << error.field_name << std::endl;
    std::cout << "  值: " << error.invalid_value << std::endl;
    std::cout << "  处理动作: " << error.action_taken << std::endl;
});

// 检查特定参数的修正历史
void DebugParameterCorrection(CphParams& params) {
    auto original_params = params;
    
    bool corrected = GlobalErrorHandler::ValidateParams(params);
    
    if (corrected) {
        std::cout << "参数修正详情:" << std::endl;
        if (original_params.pivot_pq != params.pivot_pq) {
            std::cout << "  pivot_pq: " << original_params.pivot_pq 
                      << " -> " << params.pivot_pq << std::endl;
        }
        // ... 检查其他参数 ...
    }
}
```

## 扩展指南

### 添加新的错误码

1. 在 `ErrorCode` 枚举中添加新值
2. 在 `DetermineFallbackStrategy` 中添加处理逻辑
3. 更新错误码文档表格
4. 添加相应的单元测试

### 自定义回退策略

```cpp
class CustomErrorHandler : public ErrorHandler {
public:
    FallbackStrategy DetermineFallbackStrategy(ErrorCode error_code) override {
        // 自定义回退策略逻辑
        switch (error_code) {
            case ErrorCode::CUSTOM_ERROR:
                return FallbackStrategy::CUSTOM_FALLBACK;
            default:
                return ErrorHandler::DetermineFallbackStrategy(error_code);
        }
    }
};
```

### 集成外部监控系统

```cpp
class MonitoringErrorHandler : public ErrorHandler {
public:
    void LogError(const ErrorReport& error) override {
        // 调用基类方法
        ErrorHandler::LogError(error);
        
        // 发送到外部监控系统
        SendToMonitoring(error);
        
        // 触发告警（如果需要）
        if (ShouldTriggerAlert(error)) {
            TriggerAlert(error);
        }
    }
};
```

## 总结

Cinema Pro HDR 错误处理系统提供了完整的错误管理解决方案，确保系统在各种异常情况下都能保持稳定运行。通过三级回退机制、数值保护、日志节流和参数自动修正，系统能够优雅地处理从轻微参数问题到严重数值异常的各种错误情况。

系统的设计遵循了以下原则：
- **稳定性优先**：确保系统永远不会因为错误而崩溃
- **信息透明**：提供详细的错误信息和处理记录
- **性能友好**：最小化错误处理的性能开销
- **易于扩展**：支持自定义错误处理策略和监控集成

通过合理使用这个错误处理系统，开发者可以构建更加健壮和可靠的Cinema Pro HDR应用程序。