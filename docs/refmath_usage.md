# Cinema Pro HDR RefMath 使用指南

## 概述

RefMath（参考数学）系统为Cinema Pro HDR提供64位精度的参考实现，用于CI中对比GPU/OFX/DCTL输出的数值精度。这个系统的目标是一键定位数值分歧，避免"看起来像"的主观争执。

## 核心组件

### 1. 高精度参考实现 (`scripts/generate_refmath.py`)

包含以下模块：

- **RefMathPQ**: ST 2084 PQ EOTF/OETF函数
- **RefMathColorSpace**: 色彩空间转换（BT.2020, P3-D65, OKLab）
- **RefMathOKLab**: OKLab色彩空间和饱和度调节
- **RefMathDeltaE**: CIE ΔE00色差计算
- **RefMathToneMapping**: PPR和RLOG色调映射曲线

### 2. CI回归测试 (`scripts/ci_regression_test.py`)

提供自动化测试框架：

- 加载RefMath参考数据
- 对比实际实现输出
- 生成误差分析报告
- 输出可视化误差图表

## 使用方法

### 生成参考数据

```bash
# 激活虚拟环境
source venv/bin/activate

# 生成RefMath参考数据
python scripts/generate_refmath.py --output-dir golden

# 可选：生成可视化图表
python scripts/generate_refmath.py --output-dir golden --plot
```

生成的文件：
- `golden/curves.csv`: 16384点稠密曲线采样
- `golden/reference_data.json`: 完整参考数据集
- `golden/validation_report.md`: 验证报告

### 运行CI回归测试

```bash
# 使用示例数据测试
python scripts/ci_regression_test.py --golden-dir golden --output-dir test_results --generate-plots

# 使用真实测试数据
python scripts/ci_regression_test.py --golden-dir golden --test-data my_test_data.json --tolerance 1e-6 --strict
```

参数说明：
- `--golden-dir`: RefMath数据目录
- `--test-data`: 测试数据文件（JSON格式）
- `--tolerance`: 允许的最大误差（默认1e-6）
- `--strict`: 使用严格模式（最大误差必须在容差内）
- `--generate-plots`: 生成误差热力图

## 数据格式

### 测试数据格式

测试数据应为JSON格式，结构如下：

```json
{
  "Cinema-Flat": {
    "ppr_y": [0.0, 0.001, 0.002, ...],
    "rlog_y": [0.0, 0.001, 0.002, ...]
  },
  "Cinema-Punch": {
    "ppr_y": [...],
    "rlog_y": [...]
  },
  "Cinema-Highlight": {
    "ppr_y": [...],
    "rlog_y": [...]
  }
}
```

### 色彩空间转换测试

```json
{
  "bt2020_to_p3d65": [[r1,g1,b1], [r2,g2,b2], ...],
  "p3d65_to_bt2020": [[r1,g1,b1], [r2,g2,b2], ...],
  "pq_eotf": [[r1,g1,b1], [r2,g2,b2], ...],
  "pq_oetf": [[r1,g1,b1], [r2,g2,b2], ...],
  "rgb_to_oklab": [[r1,g1,b1], [r2,g2,b2], ...],
  "oklab_to_rgb": [[r1,g1,b1], [r2,g2,b2], ...]
}
```

## 精度要求

### 容差标准

- **默认容差**: 1e-6（适用于大多数情况）
- **严格模式**: 最大误差必须在容差内
- **宽松模式**: 99分位数误差在容差内即可

### 量化误差

对于10/12bit量化后的对比：
- **10bit**: 1 LSB ≈ 1/1024 ≈ 9.77e-4
- **12bit**: 1 LSB ≈ 1/4096 ≈ 2.44e-4

## 报告解读

### 误差统计

- **最大误差**: 所有点中的最大绝对误差
- **平均误差**: 平均绝对误差
- **RMS误差**: 均方根误差
- **95/99分位数**: 95%/99%的点误差小于此值

### Top-10 Worst Points

列出误差最大的10个点，包含：
- 索引位置
- 输入值
- 期望输出
- 实际输出
- 绝对误差

### 误差热力图

可视化误差分布，帮助识别：
- 误差集中的区域
- 系统性偏差
- 数值不稳定区域

## 集成到CI流程

### 1. 在构建脚本中添加

```bash
#!/bin/bash
# 构建项目
make clean && make

# 运行测试生成数据
./run_tests --output test_output.json

# 对比RefMath
python scripts/ci_regression_test.py \
  --golden-dir golden \
  --test-data test_output.json \
  --tolerance 1e-6 \
  --output-dir ci_results

# 检查退出码
if [ $? -ne 0 ]; then
  echo "RefMath回归测试失败!"
  exit 1
fi
```

### 2. 在GitHub Actions中使用

```yaml
- name: RefMath回归测试
  run: |
    source venv/bin/activate
    python scripts/ci_regression_test.py \
      --golden-dir golden \
      --test-data ${{ github.workspace }}/test_output.json \
      --tolerance 1e-6 \
      --output-dir ci_results
    
- name: 上传测试报告
  uses: actions/upload-artifact@v3
  if: always()
  with:
    name: refmath-reports
    path: ci_results/
```

## 故障排除

### 常见问题

1. **单调性检查失败**
   - 检查曲线参数是否合理
   - 确认拼接点连续性
   - 验证软膝和toe夹持逻辑

2. **色彩空间转换误差过大**
   - 检查转换矩阵精度
   - 验证白点和原色坐标
   - 确认往返转换一致性

3. **PQ EOTF/OETF误差**
   - 检查ST 2084常数精度
   - 验证边界条件处理
   - 确认数值稳定性

### 调试技巧

1. **使用严格模式**定位精确的误差源
2. **查看Top-10 worst points**找到问题区域
3. **生成误差热力图**识别系统性问题
4. **对比不同容差**评估实现质量

## 扩展RefMath

### 添加新的曲线类型

1. 在`RefMathToneMapping`类中添加新方法
2. 更新`CphParams`数据结构
3. 修改`generate_curve_tables`方法
4. 更新测试对比逻辑

### 添加新的色彩空间

1. 在`RefMathColorSpace`类中添加转换矩阵
2. 实现正向和反向转换方法
3. 添加到`generate_color_space_tables`
4. 更新CI测试脚本

### 自定义误差分析

1. 继承`ErrorAnalyzer`类
2. 实现自定义误差度量
3. 集成到`ComparisonResult`
4. 更新报告生成器

## 最佳实践

1. **定期更新RefMath数据**：算法变更时重新生成
2. **保持参数一致性**：确保测试使用相同的参数集
3. **记录精度要求**：为不同用例设定合适的容差
4. **自动化测试**：集成到CI/CD流程中
5. **版本控制**：跟踪RefMath数据的变更历史

## 性能考虑

- RefMath使用64位精度，计算较慢，仅用于验证
- 生成16384点数据约需几秒钟
- CI测试通常在1分钟内完成
- 可根据需要调整采样点数量

## 相关文档

- [Cinema Pro HDR 设计文档](design.md)
- [色彩空间实现文档](color_space_implementation.md)
- [错误处理系统文档](error_handling_system.md)