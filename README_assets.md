# Cinema Pro HDR 测试资产说明

## 概述

本目录包含Cinema Pro HDR系统的基线测试资产，用于CI视觉回归测试和数值验证。

**重要说明**：这些资产专门用于验证**PPR和RLOG算法**的跨平台实现一致性，不是通用的HDR调色标准。

### 🎯 核心目的

Cinema Pro HDR需要在多个平台实现相同的算法：
- **C++版本**（CPU计算）
- **CUDA版本**（NVIDIA GPU）
- **OpenCL版本**（AMD GPU）
- **DCTL版本**（DaVinci Resolve）
- **OFX版本**（After Effects等插件）

虽然编程语言、硬件平台、数值精度都不同，但**必须确保算出相同的结果**。

### 📊 curves.csv的作用

**不是**：限制调色创作的艺术标准
**而是**：确保算法实现的技术标准

```
同样的PPR参数 + 同样的输入 = 同样的输出（跨所有平台）
```

如果你使用其他算法（Dolby Vision、ACES等），这些数据对你没有用处。

## 目录结构

```
├── assets/
│   └── cliplist.json          # 12段基线样片清单
├── golden/
│   ├── curves.csv             # 64位RefMath曲线表
│   ├── reference_data.json    # 完整参考数据
│   └── frames/
│       ├── metadata.json      # 参考帧元数据
│       └── *.exr             # 参考帧文件 (需外部生成)
└── scripts/
    ├── generate_refmath.py    # RefMath生成器
    └── requirements.txt       # Python依赖

```

## 资产详情

### 1. 基线样片 (assets/cliplist.json)

包含12段固定样片，覆盖以下场景类型：

- **夜景** (2段): 城市霓虹、月光森林
- **逆光人像** (2段): 室内窗光、户外阳光  
- **霓虹** (2段): 商业街、酒吧内景
- **薄雾** (2段): 山景日出、湖面晨雾
- **高亮峰** (2段): 金属反射、阳光直射
- **肤色** (2段): 自然光、混合光源

每段样片规格：
- 分辨率: 4096×2160
- 帧率: 24fps
- 时长: 12秒
- 版权: 仅用于技术测试

### 2. RefMath参考曲线 (golden/curves.csv)

**这是什么**：PPR和RLOG算法的"标准答案表"

64位高精度参考曲线表，包含：

- **采样点数**: 16,384个均匀分布点
- **精度**: Float64 (双精度)
- **曲线类型**: PPR、RLOG
- **预设**: Cinema-Flat、Cinema-Punch、Cinema-Highlight

数据字段：
- `preset`: 预设名称
- `curve_type`: PPR或RLOG
- `sample_index`: 采样点索引
- `x`: 输入值 (PQ归一化)
- `y`: 输出值 (色调映射后)
- `monotonic`: 单调性验证结果
- `c1_continuous`: C¹连续性验证结果

**使用场景**：
```bash
# 开发者实现CUDA版本后验证
cuda_result = my_cuda_ppr(0.5, cinema_flat_params)  # 得到 0.391234
standard = 0.393160  # 从curves.csv查到的标准答案
if abs(cuda_result - standard) < 0.001:
    print("✅ CUDA版本实现正确")
```

**重要**：此表只适用于PPR/RLOG算法。如果你使用其他算法，需要生成自己的参考数据。

### 3. 参考数据 (golden/reference_data.json)

完整的参考数据集，包含：

- 三套预设的完整参数
- 曲线采样数据
- 验证结果汇总
- 生成器版本信息

### 4. 参考帧元数据 (golden/frames/metadata.json)

参考帧的元数据描述，包含：

- 预设与样片的组合
- 期望的统计值
- 文件命名规范

**注意**: 实际的.exr参考帧文件需要使用渲染工具生成。

## 使用方法

### 生成RefMath数据

```bash
# 安装依赖
pip install -r scripts/requirements.txt

# 生成参考数据 (默认16384采样点)
python3 scripts/generate_refmath.py --output-dir golden

# 自定义采样点数
python3 scripts/generate_refmath.py --output-dir golden --samples 32768
```

### CI集成

在CI流程中使用这些资产进行**跨平台一致性验证**：

1. **数值对比**: 将GPU/OFX/DCTL输出与RefMath曲线对比
   ```
   目标：确保所有平台实现PPR/RLOG算法时结果一致
   标准：差异 ≤1 LSB (10/12bit量化后)
   ```

2. **视觉回归**: 使用参考帧检测视觉变化
   ```
   目标：代码修改后确保视觉效果不变
   方法：对比渲染结果与golden参考帧
   ```

3. **ΔE验证**: 计算色彩差异并与基线对比
   ```
   目标：确保色彩准确性在可接受范围内
   标准：ΔE00均值≤0.5，99分位≤1.0，最大≤2.0
   ```

4. **闪烁检测**: 分析帧间能量变化
   ```
   目标：确保视频播放时不会产生闪烁
   标准：1-6Hz频域能量增长<20%
   ```

### 验证流程

```bash
# 1. 验证曲线单调性和连续性
python3 scripts/validate_curves.py golden/curves.csv

# 2. 生成误差栅格图
python3 scripts/generate_error_heatmap.py --reference golden/curves.csv --test output/test_curves.csv

# 3. 输出top-10最差点
python3 scripts/find_worst_points.py --reference golden/curves.csv --test output/test_curves.csv
```

## 预设参数

**说明**：这三套预设展示了PPR/RLOG算法在不同参数下的效果。调色师可以根据创作需求选择或调整参数。

### Cinema-Flat (电影平坦)
- **用途**: 保守的色调映射，保持更多原始动态范围
- **适用场景**: 需要后期大幅调整的素材，或追求自然效果的场景
- **PPR参数**: p=0.18, γs=1.10, γh=1.05, h=1.0
- **饱和度**: base=1.00, highlights=0.95

### Cinema-Punch (电影冲击)  
- **用途**: 增强对比度和视觉冲击力
- **适用场景**: 动作片、商业广告等需要强烈视觉效果的内容
- **PPR参数**: p=0.18, γs=1.40, γh=1.10, h=1.8
- **饱和度**: base=1.05, highlights=1.00

### Cinema-Highlight (电影高光)
- **用途**: 专注于高光细节保持
- **适用场景**: 金属反射、阳光直射等高光丰富的场景
- **PPR参数**: p=0.20, γs=1.20, γh=0.95, h=1.2  
- **饱和度**: base=0.98, highlights=0.92

**重要**：这些预设只是示例。实际使用时，调色师可以根据创作需求调整任何参数，算法会保证数学上的正确性（单调性、连续性等）。

## 质量标准

### 数值精度
- RefMath与实现差异: ≤1 LSB (10/12bit量化后)
- 单调性: 4096点采样全部通过
- C¹连续性: 导数间隙<1e-3

### 视觉质量
- ΔE00均值: ≤0.5
- ΔE00 99分位: ≤1.0  
- ΔE00最大值: ≤2.0
- 闪烁能量增长: <20% (1-6Hz)

## 版权声明

所有测试资产仅用于Cinema Pro HDR技术开发和测试，不得用于商业用途。

© 2025 Cinema Pro HDR Project - 技术测试专用资产
#
# 常见误解澄清

### ❌ 误解1：这是HDR调色的行业标准
**✅ 实际**：这是PPR/RLOG算法的实现标准，不限制调色创作

### ❌ 误解2：所有HDR项目都要用这套参数
**✅ 实际**：只有选择使用PPR/RLOG算法的项目才需要这些数据

### ❌ 误解3：curves.csv适用于所有调色算法
**✅ 实际**：只适用于PPR/RLOG算法，其他算法需要自己的参考数据

### ❌ 误解4：这限制了调色师的创作自由
**✅ 实际**：调色师仍可自由调整参数，只是确保算法计算正确

## 技术类比

**就像钢琴调音**：
- 🎵 **音乐创作**没有标准（每首歌都不同）
- 🎹 **钢琴调音**有标准（A4=440Hz）
- 🎯 **目的**：确保不同钢琴演奏同一首曲子时音高一致

**Cinema Pro HDR做的是"钢琴调音"**：
- 🎨 不限制你创作什么"音乐"（调色效果）
- ⚙️ 但确保你的"钢琴"（算法）是准确的
- 🎯 让创作意图能在不同设备上准确还原

## 适用人群

### 🎯 这些资产对你有用，如果你是：
- PPR/RLOG算法的实现者
- Cinema Pro HDR系统的开发者
- 需要验证跨平台一致性的工程师
- 研究PPR/RLOG算法的学者

### 🚫 这些资产对你没用，如果你是：
- 使用其他调色算法的开发者
- 只关心最终调色效果的艺术家
- 不涉及算法实现的用户