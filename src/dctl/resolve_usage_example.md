# DaVinci Resolve中使用Cinema Pro HDR DCTL

## 快速开始

### 1. 安装DCTL文件

#### 自动安装（推荐）
```bash
# 在项目根目录运行
./scripts/build_dctl.sh --install
```

#### 手动安装
将以下文件复制到DaVinci Resolve的LUT目录：

**Windows:**
```
%APPDATA%\Blackmagic Design\DaVinci Resolve\Support\LUT\
```

**macOS:**
```
~/Library/Application Support/Blackmagic Design/DaVinci Resolve/LUT/
```

**Linux:**
```
~/.local/share/DaVinciResolve/LUT/
```

需要复制的文件：
- `cinema_pro_hdr.dctl`
- `cinema_pro_hdr_optimized.dctl`

### 2. 在Resolve中使用

1. 打开DaVinci Resolve
2. 进入Color页面
3. 在节点图中右键点击节点
4. 选择 "OpenFX" → "ResolveFX Color" → "Custom Curve"
5. 在Custom Curve中选择 "DCTL"
6. 从下拉菜单中选择 "Cinema Pro HDR"

## 参数设置指南

### 基础设置

#### 曲线类型选择
```
cph_curve_type = 0    # PPR曲线（推荐用于大多数内容）
cph_curve_type = 1    # RLOG曲线（适合高动态范围内容）
```

#### 枢轴点设置
```
cph_pivot = 0.18      # 对应180 nits（标准设置）
cph_pivot = 0.15      # 对应约150 nits（较暗内容）
cph_pivot = 0.22      # 对应约220 nits（较亮内容）
```

### PPR参数调节

#### 阴影对比度
```
cph_gamma_s = 1.10    # 轻微增强阴影对比
cph_gamma_s = 1.25    # 中等增强（默认）
cph_gamma_s = 1.40    # 强烈增强阴影对比
```

#### 高光对比度
```
cph_gamma_h = 0.95    # 柔和高光
cph_gamma_h = 1.10    # 标准高光（默认）
cph_gamma_h = 1.25    # 增强高光对比
```

#### 高光滚降
```
cph_shoulder = 1.0    # 温和滚降
cph_shoulder = 1.5    # 中等滚降（默认）
cph_shoulder = 2.0    # 强烈滚降
```

### 高级参数

#### 高光细节
```
cph_highlight_detail = 0.0    # 关闭高光细节
cph_highlight_detail = 0.3    # 轻微增强（默认）
cph_highlight_detail = 0.6    # 强烈增强
```

#### 饱和度控制
```
# 基础饱和度
cph_sat_base = 0.9     # 降低饱和度
cph_sat_base = 1.0     # 保持原始饱和度（默认）
cph_sat_base = 1.1     # 增强饱和度

# 高光饱和度
cph_sat_hi = 0.85      # 降低高光饱和度
cph_sat_hi = 0.95      # 轻微降低（默认）
cph_sat_hi = 1.05      # 增强高光饱和度
```

## 工作流程建议

### 1. HDR到SDR工作流程

```
输入: HDR素材 (BT.2020, PQ)
↓
节点1: 输入色彩空间转换 → BT.2020
↓
节点2: Cinema Pro HDR DCTL
  - 使用PPR曲线
  - pivot = 0.18
  - 根据内容调整gamma_s和gamma_h
↓
节点3: 输出色彩空间转换 → Rec.709
↓
输出: SDR交付格式
```

### 2. HDR到HDR工作流程

```
输入: HDR素材 (BT.2020, PQ)
↓
节点1: Cinema Pro HDR DCTL
  - 使用RLOG曲线（保持更多动态范围）
  - pivot = 0.20
  - 较温和的参数设置
↓
节点2: 色彩空间管理
  - 保持BT.2020色域
  - 输出PQ或HLG
↓
输出: HDR交付格式
```

### 3. 创意调色工作流程

```
输入: 任意格式素材
↓
节点1: 基础曝光和对比度调整
↓
节点2: Cinema Pro HDR DCTL
  - 选择合适的预设作为起点
  - 根据创意需求调整参数
↓
节点3: 二级调色和局部调整
↓
节点4: 最终色彩空间转换
↓
输出: 目标交付格式
```

## 预设参数

### Cinema-Flat（自然平缓）
适用于：纪录片、自然风光、需要保持真实感的内容

```
cph_curve_type = 0
cph_pivot = 0.18
cph_gamma_s = 1.10
cph_gamma_h = 1.05
cph_shoulder = 1.0
cph_highlight_detail = 0.2
cph_sat_base = 1.00
cph_sat_hi = 0.95
```

### Cinema-Punch（商业风格）
适用于：商业广告、音乐视频、需要强烈视觉冲击的内容

```
cph_curve_type = 0
cph_pivot = 0.18
cph_gamma_s = 1.40
cph_gamma_h = 1.10
cph_shoulder = 1.8
cph_highlight_detail = 0.4
cph_sat_base = 1.05
cph_sat_hi = 1.00
```

### Cinema-Highlight（高光保护）
适用于：高动态范围场景、需要保护高光细节的内容

```
cph_curve_type = 0
cph_pivot = 0.20
cph_gamma_s = 1.20
cph_gamma_h = 0.95
cph_shoulder = 1.2
cph_highlight_detail = 0.6
cph_sat_base = 0.98
cph_sat_hi = 0.92
```

## 性能优化建议

### 1. 选择合适的DCTL版本

**标准版本** (`cinema_pro_hdr.dctl`)
- 适用于：4K以下分辨率
- 特点：完整功能，易于调试

**优化版本** (`cinema_pro_hdr_optimized.dctl`)
- 适用于：4K及以上分辨率
- 特点：性能优化，减少GPU负载

### 2. 参数优化

```
# 高性能设置
cph_deterministic = 0.0    # 关闭确定性模式
cph_highlight_detail = 0.0 # 关闭高光细节处理
cph_sat_base = 1.0         # 避免复杂的饱和度计算
cph_sat_hi = 1.0
```

### 3. 节点顺序优化

```
推荐顺序：
1. 基础曝光调整
2. Cinema Pro HDR DCTL
3. 色彩空间转换
4. 二级调色

避免：
- 在DCTL前后放置过多节点
- 重复的色彩空间转换
- 不必要的复杂效果
```

## 故障排除

### 常见问题

#### 1. DCTL不显示在菜单中
**解决方案：**
- 确认文件已正确复制到LUT目录
- 重启DaVinci Resolve
- 检查文件权限

#### 2. 处理速度慢
**解决方案：**
- 使用优化版本DCTL
- 降低不必要的参数复杂度
- 检查GPU内存使用情况
- 更新GPU驱动

#### 3. 颜色异常
**解决方案：**
- 检查输入色彩空间设置
- 验证参数是否在有效范围内
- 尝试使用预设参数
- 检查节点顺序

#### 4. 渲染错误
**解决方案：**
- 检查GPU兼容性
- 降低处理分辨率测试
- 查看Resolve错误日志
- 尝试标准版本DCTL

### 调试技巧

#### 1. 参数测试
```
# 从最简单的设置开始
cph_curve_type = 0
cph_pivot = 0.18
cph_gamma_s = 1.0
cph_gamma_h = 1.0
cph_shoulder = 1.0
# 其他参数保持默认值
```

#### 2. 性能监控
- 使用Resolve的性能监视器
- 观察GPU使用率和内存占用
- 记录不同参数设置下的处理时间

#### 3. 视觉验证
- 使用测试图案验证曲线行为
- 检查单调性和连续性
- 对比不同参数设置的效果

## 高级用法

### 1. 自定义参数映射

如果需要更精确的控制，可以直接修改DCTL文件中的参数值：

```c
// 在DCTL文件顶部修改
__CONSTANT__ float cph_pivot = 0.175f;     // 自定义枢轴点
__CONSTANT__ float cph_gamma_s = 1.33f;    // 自定义阴影伽马
// ... 其他参数
```

### 2. 批量处理

对于批量处理，建议：
1. 在代表性素材上确定最佳参数
2. 创建自定义预设
3. 使用Resolve的批量应用功能
4. 验证一致性

### 3. 质量控制

```
检查清单：
□ 单调性验证（无反转）
□ 高光细节保护
□ 阴影细节保持
□ 色彩饱和度自然
□ 无明显伪影
□ 性能达标
```

## 技术支持

如遇到问题，请提供以下信息：
- DaVinci Resolve版本
- 操作系统和GPU信息
- 使用的DCTL版本和参数
- 输入素材格式和分辨率
- 具体错误信息或异常行为描述

更多技术细节请参考项目文档和源代码注释。