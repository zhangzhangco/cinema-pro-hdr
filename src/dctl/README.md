# Cinema Pro HDR DCTL 实现

## 概述

本目录包含Cinema Pro HDR系统的DCTL（DaVinci Color Transform Language）实现，用于在DaVinci Resolve中提供实时HDR色调映射功能。

## 文件结构

```
src/dctl/
├── cinema_pro_hdr.dctl              # 主DCTL实现文件
├── cinema_pro_hdr_optimized.dctl    # 性能优化版本
├── parameter_mapping.h              # UI参数映射工具
├── statistics_collector.cpp         # 统计收集器实现
└── README.md                       # 本文档
```

## 功能特性

### 🎯 核心功能
- **双曲线支持**：PPR（Pivoted Power-Rational）和RLOG（Rational Logarithmic）
- **实时处理**：GPU优化，支持4K@24p实时处理
- **参数映射**：UI控件到算法参数的自动转换
- **统计收集**：实时PQ MaxRGB统计和性能监控
- **数值稳定**：完整的NaN/Inf保护和回退机制

### 🚀 性能优化
- **混合精度**：FP16+FP32混合精度计算
- **分支优化**：使用权重混合避免GPU分支
- **向量化**：GPU友好的向量化操作
- **预计算**：常数预计算减少运行时开销

### 🎨 色彩管理
- **工作域**：BT.2020+PQ归一化处理
- **OKLab饱和度**：感知均匀的饱和度调节
- **色域处理**：两级色域越界处理机制
- **DCI合规**：支持DCI合规模式

## 使用方法

### 1. 在DaVinci Resolve中安装

1. 将`cinema_pro_hdr.dctl`文件复制到Resolve的DCTL目录：
   - **Windows**: `%APPDATA%\Blackmagic Design\DaVinci Resolve\Support\LUT\`
   - **macOS**: `~/Library/Application Support/Blackmagic Design/DaVinci Resolve/LUT/`
   - **Linux**: `~/.local/share/DaVinciResolve/LUT/`

2. 重启DaVinci Resolve

3. 在Color页面的节点中右键选择"DCTL" → "Cinema Pro HDR"

### 2. 参数配置

#### 基础参数
- **Curve Type**: 0=PPR, 1=RLOG
- **Pivot**: 枢轴点，建议0.18（对应180 nits）
- **Deterministic Mode**: 确定性模式，用于跨平台一致性

#### PPR参数（当Curve Type=0时）
- **Gamma Shadows**: 阴影伽马 [1.0, 1.6]
- **Gamma Highlights**: 高光伽马 [0.8, 1.4]  
- **Shoulder**: 肩部参数 [0.5, 3.0]

#### RLOG参数（当Curve Type=1时）
- **RLOG A**: 暗部参数 [1.0, 16.0]
- **RLOG B**: 高光增益 [0.8, 1.2]
- **RLOG C**: 高光压缩 [0.5, 3.0]
- **RLOG T**: 拼接阈值 [0.4, 0.7]

#### 高级参数
- **Soft Knee**: 软膝点位置 [0.95, 0.99]
- **Knee Alpha**: 软膝强度 [0.2, 1.0]
- **Toe Clamp**: toe夹持 [0.0, 0.01]
- **Highlight Detail**: 高光细节强度 [0.0, 1.0]
- **Saturation Base**: 基础饱和度 [0.0, 2.0]
- **Saturation Highlights**: 高光饱和度 [0.0, 2.0]

### 3. 预设使用

系统提供三套预设参数：

#### Cinema-Flat（平缓自然）
```
Pivot: 0.18, Gamma S: 1.10, Gamma H: 1.05, Shoulder: 1.0
Black Lift: 0.003, Highlight Detail: 0.2
Sat Base: 1.00, Sat Hi: 0.95
```

#### Cinema-Punch（增强对比）
```
Pivot: 0.18, Gamma S: 1.40, Gamma H: 1.10, Shoulder: 1.8
Black Lift: 0.002, Highlight Detail: 0.4
Sat Base: 1.05, Sat Hi: 1.00
```

#### Cinema-Highlight（高光保护）
```
Pivot: 0.20, Gamma S: 1.20, Gamma H: 0.95, Shoulder: 1.2
Black Lift: 0.004, Highlight Detail: 0.6
Sat Base: 0.98, Sat Hi: 0.92
```

## UI参数映射

### PPR映射公式
- **Shadows Contrast** (S∈[0,1]) → γs = 1.0 + 0.6×S
- **Highlight Contrast** (H∈[0,1]) → γh = 0.8 + 0.6×H
- **Highlights Roll-off** (R∈[0,1]) → h = 0.5 + 2.5×R
- **Pivot (Nits)** → 通过PQ OETF转换为归一化值

### RLOG映射公式
- **Shadow Lift** (S∈[0,1]) → a = 1 + 15×S
- **Highlight Gain** (G∈[0,1]) → b = 0.8 + 0.4×G
- **Highlight Roll-off** (R∈[0,1]) → c = 0.5 + 2.5×R
- **Blend Threshold** (B∈[0,1]) → t = 0.4 + 0.3×B

## 性能指标

### 目标性能
- **4K@24p**: < 1.0ms/帧（中位数），< 1.2ms（P95）
- **8K@24p**: < 3.5ms/帧（中位数），< 4.0ms（P95）

### 测试平台
- **NVIDIA**: RTX 3080, Windows 11, Studio驱动
- **Apple**: M2 Max, macOS 14
- **AMD**: RX 6800 XT, Windows 11

### 性能优化建议
1. **使用优化版本**：对于高分辨率内容，使用`cinema_pro_hdr_optimized.dctl`
2. **启用确定性模式**：在需要跨平台一致性时启用
3. **合理设置参数**：避免极端参数值，保持在推荐范围内
4. **监控统计信息**：使用统计收集器监控性能

## 统计收集

### 实时统计
- **Min/Avg/Max PQ MaxRGB**: 1%顶帽去极值的截尾统计
- **处理时间**: 帧级别的处理时间统计
- **像素计数**: 处理的总像素数

### 统计API
```cpp
// C接口
void cph_dctl_add_pq_sample(float pq_max_rgb);
void cph_dctl_record_time(double time_ms);
DCTLStatistics cph_dctl_get_statistics();
void cph_dctl_reset_statistics();

// C++接口
StatisticsCollector collector;
collector.AddPqMaxRgbSample(0.75f);
collector.RecordFrameProcessingTime(0.85);
auto stats = collector.ComputeCurrentStatistics();
```

## 数值稳定性

### 保护机制
1. **输入验证**: 所有输入值的NaN/Inf检测
2. **范围钳制**: 参数自动钳制到有效范围
3. **安全数学**: 使用SafePow、SafeLog、SafeDivide
4. **回退策略**: 异常时回退到线性映射

### 确定性模式
- **用途**: 确保跨GPU平台的数值一致性
- **精度**: 误差≤1 LSB（10/12bit量化后）
- **性能**: 略低于默认模式，但保证一致性

## 故障排除

### 常见问题

#### 1. 性能不达标
**症状**: 处理时间超过目标值
**解决方案**:
- 使用优化版本DCTL
- 检查GPU驱动是否为最新版本
- 降低不必要的参数复杂度
- 确认GPU内存充足

#### 2. 颜色异常
**症状**: 出现色彩偏移或饱和度问题
**解决方案**:
- 检查输入色彩空间是否正确
- 验证参数是否在有效范围内
- 尝试重置为预设参数
- 检查是否启用了DCI合规模式

#### 3. 数值不一致
**症状**: 不同GPU输出结果不同
**解决方案**:
- 启用确定性模式
- 确保所有平台使用相同的参数
- 检查GPU驱动版本一致性
- 验证输入数据格式

#### 4. 统计信息异常
**症状**: 统计数据显示异常值
**解决方案**:
- 重置统计收集器
- 检查输入数据有效性
- 验证样本数量是否充足
- 确认统计收集线程正常运行

### 调试工具

#### 参数验证
```cpp
DCTLPresetParams params = GetCinemaFlatPreset();
params = ValidateAndClampParams(params);
bool valid = AreParamsValid(params);
```

#### 性能监控
```cpp
ScopedPerformanceMonitor monitor;
// ... 处理代码 ...
// 析构时自动记录时间
```

#### 统计报告
```cpp
std::string text_report = StatisticsReporter::GenerateTextReport();
std::string json_report = StatisticsReporter::GenerateJsonReport();
```

## 开发指南

### 添加新功能
1. 在主DCTL文件中添加算法实现
2. 更新参数映射头文件
3. 添加相应的统计收集
4. 更新预设参数
5. 编写测试用例

### 性能优化
1. 使用向量化操作
2. 避免分支和条件判断
3. 预计算常数
4. 使用快速数学函数
5. 减少内存访问

### 测试验证
1. 数值精度测试
2. 性能基准测试
3. 跨平台一致性测试
4. 视觉质量评估
5. 回归测试

## 版本历史

### v1.0 (当前版本)
- 初始DCTL实现
- PPR和RLOG算法支持
- 基础性能优化
- 统计收集系统
- 三套预设参数

### 计划功能
- 更多预设参数
- 高级高光细节算法
- 动态参数调节
- 实时波形显示
- 自动参数优化

## 许可证

本项目遵循Cinema Pro HDR项目的许可证条款。

## 联系方式

如有问题或建议，请联系Cinema Pro HDR开发团队。