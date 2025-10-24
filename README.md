# Cinema Pro HDR

一套专业的HDR色调映射算法实现，提供跨平台一致的PPR和RLOG算法。

## 🎯 项目目标

**不是**：制定HDR调色的行业标准  
**而是**：提供可靠的算法实现，确保创作意图跨设备准确还原

### 核心价值
- 🔬 **算法可靠性**：数学上保证单调性和C¹连续性
- 🌐 **跨平台一致**：CPU、GPU、插件系统结果完全一致  
- 🎨 **创作自由度**：参数可调，适应不同艺术需求
- 📐 **工程化实现**：完整的测试、验证、部署工具链

## 🧮 核心算法

### PPR (Pivoted Power-Rational)
- **暗部**：幂函数控制对比度
- **亮部**：有理函数自然收肩
- **拼接**：C¹连续平滑过渡

### RLOG (Rational Logarithmic)  
- **暗部**：对数特性保持细节
- **亮部**：有理式压缩防过曝
- **混合**：平滑区间处理

## 🚀 多平台实现

| 平台 | 用途 | 状态 |
|------|------|------|
| C++ Core | 算法核心库 | 🚧 开发中 |
| DCTL | DaVinci Resolve | 📋 计划中 |
| OFX | After Effects等 | 📋 计划中 |
| CUDA | NVIDIA GPU加速 | 📋 计划中 |
| OpenCL | AMD GPU加速 | 📋 计划中 |

## 📊 质量保证

- **数值精度**：64位RefMath基准，跨平台差异≤1 LSB
- **视觉质量**：ΔE00验证，闪烁检测
- **算法正确性**：单调性、连续性自动验证
- **回归测试**：12段基线样片，3套预设全覆盖

## 🛠️ 快速开始

```bash
# 克隆项目
git clone https://github.com/zhangzhangco/cinema-pro-hdr.git
cd cinema-pro-hdr

# 构建项目
mkdir build && cd build
cmake ..
make

# 运行测试
./tests/run_all_tests

# 生成参考数据
python3 scripts/generate_refmath.py
```

## 📁 项目结构

```
├── src/                    # 核心算法实现
├── tests/                  # 单元测试和验证
├── assets/                 # 测试样片规格
├── golden/                 # 参考数据和基准
├── scripts/                # 工具脚本
└── 专利/                   # 技术文档和专利材料
```

## 📖 文档

- [资产说明](README_assets.md) - 测试数据和验证流程
- [实施计划](.kiro/specs/cinema-pro-hdr/tasks.md) - 开发路线图
- [技术白皮书](专利/) - 算法原理和专利文档

## 🤝 适用场景

### ✅ 适合你，如果：
- 需要实现PPR/RLOG算法
- 要求跨平台数值一致性
- 开发HDR相关工具或插件
- 研究色调映射算法

### ❌ 不适合你，如果：
- 只需要现成的调色工具
- 使用其他算法（ACES、Dolby Vision等）
- 不涉及算法实现

## 📄 许可证

本项目仅用于技术研究和测试，商业使用请联系作者。

---

**重要说明**：本项目专注于算法实现的技术标准，不限制调色创作的艺术自由。
