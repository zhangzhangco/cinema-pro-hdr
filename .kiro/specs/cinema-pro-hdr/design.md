# Cinema Pro HDR 系统设计文档

## 概述

Cinema Pro HDR是一个专业的电影级HDR色调映射系统，采用模块化架构设计，支持实时处理和离线烘焙。系统核心基于数学严格的PPR和RLOG色调映射曲线，提供DCTL原型、OFX插件、C++核心库和命令行工具的完整解决方案。

设计目标：
- 数值稳定性和跨平台一致性
- 实时性能（4K<1ms，8K<3.5ms）
- 专业级色彩管理工作流程
- DCI合规和影院级质量标准

## 系统架构

### 整体架构图

```
┌─────────────────────────────────────────────────────────────┐
│                    Cinema Pro HDR 系统                        │
├─────────────────┬─────────────────┬─────────────────────────┤
│   DCTL 原型      │    OFX 插件      │      命令行工具          │
│  (Resolve快速)   │   (完整UI)      │   (批处理/验证)         │
├─────────────────┴─────────────────┴─────────────────────────┤
│                Cinema Pro HDR Core (C++)                   │
│  ┌─────────────┬─────────────┬─────────────┬─────────────┐  │
│  │ 色彩空间转换  │ 色调映射引擎  │ 高光细节处理  │ LUT烘焙引擎 │  │
│  └─────────────┴─────────────┴─────────────┴─────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                    GPU/CPU 计算后端                         │
│        CUDA/Metal/OpenCL + FP16/FP32 混合精度              │
└─────────────────────────────────────────────────────────────┘
```

### 数据流架构

```
输入帧 → 色彩空间检测 → 工作域转换(BT.2020+PQ) → 色调映射 → 高光细节 → 饱和度处理(OKLab) → 目标域转换 → 输出帧
   ↓           ↓              ↓              ↓         ↓            ↓               ↓
统计收集 → 参数验证 → 数值稳定性检查 → 单调性验证 → 闪烁检测 → 色域越界检查 → 侧车生成
```

## 核心组件设计

### 1. 色调映射引擎

#### PPR (Pivoted Power-Rational) 实现

数学模型：
- 工作域：PQ归一化 x∈[0,1]
- 枢轴点：p ∈ [PQ(0.10), PQ(0.25)]
- 阴影段：y_shadow = mix(x/p, (x/p)^γs, smoothstep(0,p,x)) * p
- 高光段：y_high = (x/(1+h*x))^γh  
- 拼接：y = mix(y_shadow, y_high, smoothstep(p,1,x))
- 软膝：y = soft_knee(y, yknee, alpha)

参数约束：
- γs ∈ [1.0, 1.6]，γh ∈ [0.8, 1.4]，h ∈ [0.5, 3.0]
- yknee ∈ [0.95, 0.99]，alpha ∈ [0.2, 1.0]
- toe ∈ [0.0, 0.01]

#### RLOG (Rational Logarithmic) 实现

数学模型：
- 暗部：y1 = log(1 + a*x) / log(1 + a)
- 高光：y2 = (b*x) / (1 + c*x)
- 拼接：在阈值t±0.05区间用smoothstep混合

参数约束：
- a ∈ [1, 16]，b ∈ [0.8, 1.2]，c ∈ [0.5, 3.0]，t ∈ [0.4, 0.7]

### 2. 色彩空间管理

#### 工作域设计
- 亮度通道：PQ归一化域处理
- 色度通道：OKLab色彩空间
- 转换矩阵：基于固定的白点和原色坐标

#### 支持的色彩空间
- 输入：BT.2020, P3-D65, ACEScg, RCM/ACES
- 工作：BT.2020 + PQ归一化
- 输出：P3-D65, BT.2020, 项目指定

### 3. 高光细节处理

#### USM算法实现
- 作用域：仅x>p区域
- 核心参数：半径r=2px，强度amount=0.2，阈值thr=0.03
- 运动保护：帧间差分检测，能量门限抑制闪烁
- 频域约束：1-6Hz区间能量增长<20%### 4. 数值
稳定性设计

#### 异常处理策略
- NaN/Inf检测：每个计算节点后插入检查
- 回退机制：三级回退（参数修正→标准回退→硬回退）
- 日志节流：同类错误每秒≤10条，超出聚合报告

#### 精度控制
- 默认模式：FP16计算 + FP32累加
- 确定性模式：纯FP32路径，固定随机种子
- 跨平台一致性：确定性模式下误差≤1 LSB

### 5. 性能优化设计

#### GPU加速策略
- 计算管线：避免分支，使用权重混合
- 内存优化：纹理缓存友好的访问模式
- 批处理：多帧并行处理支持

#### 平台适配
- NVIDIA：CUDA + Tensor Core（FP16）
- AMD：OpenCL + RDNA优化
- Apple：Metal Performance Shaders

## 接口设计

### 1. 核心库API

```cpp
namespace CinemaProHDR {
    
// 主要参数结构
struct CphParams {
    enum Curve { PPR = 0, RLOG = 1 } curve;
    float pivot_pq;        // [0.05, 0.30]
    float gamma_s;         // [1.0, 1.6] 
    float gamma_h;         // [0.8, 1.4]
    float shoulder_h;      // [0.5, 3.0]
    float black_lift;      // [0.0, 0.02]
    float highlight_detail; // [0.0, 1.0]
    float sat_base;        // [0.0, 2.0]
    float sat_hi;          // [0.0, 2.0]
    bool dci_compliance;   // DCI合规模式
    bool deterministic;    // 确定性模式
};

// 图像处理接口
class CphProcessor {
public:
    bool Initialize(const CphParams& params);
    bool ProcessFrame(const Image& input, Image& output);
    bool ValidateParams(const CphParams& params);
    Statistics GetStatistics() const;
    std::string GetLastError() const;
};

// LUT烘焙接口  
class LutBaker {
public:
    bool BakeLut(const CphParams& params, 
                 ColorSpace input_cs, 
                 ColorSpace output_cs,
                 int grid_size,
                 CubeLUT& output);
    ErrorReport GetErrorReport() const;
};

// 侧车文件处理
class SidecarManager {
public:
    bool ExportSidecar(const CphParams& params,
                       const Statistics& stats,
                       const std::string& clip_guid,
                       const std::string& timecode,
                       std::string& json_output);
    bool ValidateSchema(const std::string& json_input);
};

}
```

### 2. OFX插件接口

#### 参数映射设计
```cpp
// UI控件到算法参数的映射
struct UIMapping {
    // PPR参数
    double shadows_contrast;    // [0,1] → γs = 1.0 + 0.6*S
    double highlight_contrast;  // [0,1] → γh = 0.8 + 0.6*H  
    double highlights_rolloff;  // [0,1] → h = 0.5 + 2.5*R
    double pivot_nits;         // 显示值，内部转PQ
    
    // RLOG参数
    double shadow_lift_rlog;   // [0,1] → a = 1 + 15*S
    double highlight_gain_rlog; // [0,1] → b = 0.8 + 0.4*G
    double blend_threshold;    // [0,1] → t = 0.4 + 0.3*B
    
    // 通用参数
    double black_lift;         // [0, 0.02]
    double highlight_detail;   // [0, 1.0]
    double sat_base;          // [0, 2.0] 
    double sat_highlights;    // [0, 2.0]
    
    // 模式开关
    bool dci_compliance_mode;
    bool deterministic_mode;
};
```

#### 统计显示设计
```cpp
struct StatisticsDisplay {
    struct PQStats {
        float min_pq;     // 1%截尾最小值
        float avg_pq;     // 截尾均值  
        float max_pq;     // 1%截尾最大值
        float variance;   // 方差
    } pq_stats;
    
    bool monotonic;       // 单调性检查
    bool c1_continuous;   // C¹连续性检查
    float max_derivative_gap; // 最大导数间隙
    
    // 实时更新，基于当前可见帧或用户选择区段
};
```

### 3. DCTL接口设计

#### 参数传递机制
```c
// DCTL全局参数（通过Resolve UI传递）
__CONSTANT__ float cph_pivot = 0.18f;
__CONSTANT__ float cph_gamma_s = 1.25f; 
__CONSTANT__ float cph_gamma_h = 1.10f;
__CONSTANT__ float cph_shoulder = 1.5f;
__CONSTANT__ int cph_curve_type = 0; // 0=PPR, 1=RLOG

// 主处理函数
__DEVICE__ float3 transform(int p_Width, int p_Height, int p_X, int p_Y, float p_R, float p_G, float p_B);
```

## 数据模型设计

### 1. 侧车文件结构

基于需求中的JSON Schema，设计完整的数据模型：

```cpp
struct SidecarData {
    // ST 2094-10基础层
    struct ST2094_10 {
        float min_pq_encoded_max_rgb;
        float avg_pq_encoded_max_rgb; 
        float max_pq_encoded_max_rgb;
        float offset;
        float gain;
        float gamma;
    } st2094_10;
    
    // CPH扩展层
    struct CphMeta {
        int cph_version = 2;
        int cph_curve_id;           // 0=PPR, 1=RLOG
        float pivot;
        float gamma_s, gamma_h, shoulder;
        float black_lift, highlight_detail;
        float sat_base, sat_hi;
        std::string work_cs = "BT2020_PQ";
        std::string hash_clip_guid;
        std::string timecode_inout;
        std::string generator;
    } cph_meta;
};
```

### 2. 错误处理模型

```cpp
enum class ErrorCode {
    SUCCESS = 0,
    SCHEMA_MISSING,
    RANGE_PIVOT, 
    RANGE_KNEE,
    NAN_INF,
    DET_MISMATCH,
    HL_FLICKER,
    DCI_BOUND,
    GAMUT_OOG
};

struct ErrorReport {
    ErrorCode code;
    std::string message;
    std::string field_name;
    float invalid_value;
    std::string action_taken; // "FALLBACK2094", "IDENTITY", etc.
    std::string clip_guid;
    std::string timecode;
    std::chrono::system_clock::time_point timestamp;
};
```

## 测试策略

### 1. 单元测试设计

#### 数值精度测试
- 单调性验证：4096均匀采样 + 256问题点
- C¹连续性：p±1e-3左右差分，阈值<1e-3
- 量化误差：FP32↔10/12/16bit往返，误差<1 LSB

#### 跨平台一致性测试
- 确定性模式下NVIDIA/AMD/Apple差异≤1 LSB
- 随机输入1M样本的统计一致性验证

### 2. 性能测试设计

#### 基准测试套件
- 测试序列：4×12s段（夜景、人像、霓虹、薄雾）
- 分辨率：4096×2160@24p, 7680×4320@24p
- 测量方法：预热100帧，计时3000帧，统计中位数和P95

#### 目标指标
- 4K：中位数<1.0ms，P95<1.2ms
- 8K：中位数<3.5ms，P95<4.0ms

### 3. 视觉质量测试

#### 闪烁检测
- 帧差RMS能量<0.02（PQ归一化单位）
- 频域分析：1-6Hz区间能量增长<20%
- Highlight Detail开关前后对比

#### 色彩精度测试
- LUT烘焙误差：ΔE00@P3-D65
- 目标：均值≤0.5，99分位≤1.0，最大≤2.0
- 输出top-10 worst cases用于复现

## 错误处理设计

### 1. 三级回退机制

#### 第一级：参数修正
- 越界参数钳制到有效范围
- 记录警告但继续处理
- 适用：RANGE_PIVOT, RANGE_KNEE

#### 第二级：标准回退  
- 使用ST 2094-10基础层参数
- 禁用CPH扩展功能
- 适用：SCHEMA_MISSING, DCI_BOUND

#### 第三级：硬回退
- 回退到y=x线性映射
- 记录严重错误
- 适用：NAN_INF, 算法崩溃

### 2. 日志系统设计

#### 日志格式
```
[timestamp][level][clip_guid][timecode] code=ERROR_CODE, field=FIELD_NAME, val=VALUE, action=ACTION_TAKEN
```

#### 节流策略
- 同一错误码每秒最多10条
- 超出限制时输出1条聚合摘要
- 包含发生次数和时间范围

## 部署架构

### 1. 构建系统

#### CMake配置
```cmake
# 主要目标
add_library(cinema_pro_hdr_core SHARED ${CORE_SOURCES})
add_library(cinema_pro_hdr_ofx MODULE ${OFX_SOURCES}) 
add_executable(cph_lut_baker ${BAKER_SOURCES})
add_executable(cph_json_check ${VALIDATOR_SOURCES})

# 平台特定优化
if(CUDA_FOUND)
    target_compile_definitions(cinema_pro_hdr_core PRIVATE USE_CUDA)
endif()

if(APPLE)
    target_compile_definitions(cinema_pro_hdr_core PRIVATE USE_METAL)
endif()
```

### 2. CI/CD流程

#### 自动化测试
- 数值精度回归测试
- 性能基准测试  
- 跨平台一致性验证
- 视觉质量检查（ΔE报告）

#### 交付物生成
- 签名的OFX bundle
- DCTL文件包
- 命令行工具
- 文档和示例

这个设计文档提供了完整的系统架构和实现方案，涵盖了所有需求中的技术要点，为开发团队提供了清晰的实现指导。