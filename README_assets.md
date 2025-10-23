# Cinema Pro HDR 测试资产说明

## 概述

本目录包含Cinema Pro HDR系统的基线测试资产，用于CI视觉回归测试和数值验证。

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

在CI流程中使用这些资产：

1. **数值对比**: 将GPU/OFX/DCTL输出与RefMath曲线对比
2. **视觉回归**: 使用参考帧检测视觉变化
3. **ΔE验证**: 计算色彩差异并与基线对比
4. **闪烁检测**: 分析帧间能量变化

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

### Cinema-Flat (电影平坦)
- 用途: 保守的色调映射，保持更多原始动态范围
- PPR: p=0.18, γs=1.10, γh=1.05, h=1.0
- 饱和度: base=1.00, highlights=0.95

### Cinema-Punch (电影冲击)  
- 用途: 增强对比度和视觉冲击力
- PPR: p=0.18, γs=1.40, γh=1.10, h=1.8
- 饱和度: base=1.05, highlights=1.00

### Cinema-Highlight (电影高光)
- 用途: 专注于高光细节保持
- PPR: p=0.20, γs=1.20, γh=0.95, h=1.2  
- 饱和度: base=0.98, highlights=0.92

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