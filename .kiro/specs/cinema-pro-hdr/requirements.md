# Cinema Pro HDR 色调映射系统需求文档

## 介绍

Cinema Pro HDR 是一个专业的电影级HDR色调映射系统，旨在为电影后期制作提供高质量的色调映射解决方案。系统包含DCTL原型、OFX插件、核心库和命令行工具，支持多种色调映射曲线和专业的色彩管理工作流程。

## 术语表

- **DCTL**: DaVinci Color Transform Language，达芬奇颜色变换语言
- **OFX**: OpenFX，开放式视觉特效插件标准
- **PQ**: Perceptual Quantizer，感知量化器（ST 2084标准）
- **PPR**: Pivoted Power-Rational，枢轴幂-有理式曲线（非Sigmoid/Bezier/样条），以中灰枢轴控制暗部幂、高光有理式肩部并保证C¹连续
- **RLOG**: Rational Logarithmic，有理对数曲线（暗部对数、高光有理式、t区间平滑拼接）
- **工作域**: 亮度在PQ归一域（x∈[0,1]），色度在OKLab
- **回退路径**: 读取CPH失败/异常 → 仅用ST 2094-10基础层参数渲染
- **CPH**: Cinema Pro HDR系统的缩写
- **LUT**: Look-Up Table，查找表
- **OKLab**: 感知均匀的颜色空间
- **侧车文件**: 与主媒体文件关联的元数据文件
- **色调映射**: 将源内容的亮度与色彩统计映射到目标显示能力（可能同为HDR）的非线性变换过程
- **硬回退**: 算法异常时回退到y=x线性映射
- **标准回退**: 仅使用ST 2094-10基础层参数渲染

## 需求

### 需求 1: DCTL原型开发

**用户故事:** 作为调色师，我希望能够在DaVinci Resolve中快速应用Cinema Pro HDR色调映射，以便在调色过程中实时预览效果。

#### 验收标准

1. THE Cinema_Pro_HDR_System SHALL 提供PPR和RLOG两种色调映射曲线的DCTL实现
2. THE Cinema_Pro_HDR_System SHALL 实现PPR参数域：p∈[PQ(0.10),PQ(0.25)]，γs∈[1.0,1.6]，γh∈[0.8,1.4]，h∈[0.5,3.0]
3. THE Cinema_Pro_HDR_System SHALL 实现RLOG参数域：a∈[1,16]，b∈[0.8,1.2]，c∈[0.5,3.0]，t∈[0.4,0.7]
4. THE Cinema_Pro_HDR_System SHALL 在x→1处加入soft-knee（yknee∈[0.95,0.99], alpha∈[0.2,1.0]），在x→0后加入toe夹持（toe∈[0.0,0.01]，默认0.002）
5. WHEN 参数越界时，THE Cinema_Pro_HDR_System SHALL 复位默认并写error_code:RANGE_KNEE
6. THE Cinema_Pro_HDR_System SHALL 通过4096点端点采样验证单调性和C¹连续性
7. IF 出现NaN/Inf/越界时，THEN THE Cinema_Pro_HDR_System SHALL 回退y=x并节流日志（每秒≤10条），写入clip GUID与时间码

### 需求 2: OFX插件开发

**用户故事:** 作为后期制作工程师，我希望使用完整的OFX插件界面来精确控制色调映射参数，以便在不同的后期软件中获得一致的结果。

#### 验收标准

1. THE Cinema_Pro_HDR_System SHALL 实现UI→算法映射：Shadows Contrast → γs = 1.0 + 0.6*S，Highlights Roll-off → h = 0.5 + 2.5*R，Highlight Contrast → γh = 0.8 + 0.6*H
2. THE Cinema_Pro_HDR_System SHALL 提供Pivot参数用Nits+PQ双显示，内部存储PQ值
3. THE Cinema_Pro_HDR_System SHALL 实现Highlight Detail仅作用于x>p区域，带阈值与运动保护开关（默认r=2px, amount=0.2, thr=0.03）
4. THE Cinema_Pro_HDR_System SHALL 显示统计面板min/avg/max PqEncodedMaxRGB：逐帧计算MaxRGB（PQ归一），1%顶帽去极值（上下各0.5%），avg为截尾均值，窗口为当前可见帧或用户选择的时间段
5. THE Cinema_Pro_HDR_System SHALL 提供三套预设参数快照并支持导出/导入功能

### 需求 3: 核心库实现

**用户故事:** 作为开发者，我希望有一个稳定的C++核心库来处理所有色彩空间转换和色调映射计算，以便在不同平台和应用中复用。

#### 验收标准

1. THE Cinema_Pro_HDR_System SHALL 明确处理顺序：输入域→工作域（BT.2020+PQ）→PPR/RLOG曲线（亮度）→高光细节→饱和度（OKLab）→输出域
2. THE Cinema_Pro_HDR_System SHALL 提供确定性开关--deterministic：禁用FP16近似、使用严格math路径、固定随机种子，跨NVIDIA/AMD/Apple差异≤1 LSB
3. THE Cinema_Pro_HDR_System SHALL 固定OCIO/矩阵的白点与原色坐标版本，记录在库的cs_config.json中
4. THE Cinema_Pro_HDR_System SHALL 实现PQ EOTF/OETF函数符合ST 2084标准
5. THE Cinema_Pro_HDR_System SHALL 在OKLab色彩空间中实现饱和度处理

### 需求 4: LUT导出功能

**用户故事:** 作为调色师，我希望能够将调色参数烘焙成标准LUT文件，以便在不支持实时插件的环境中使用。

#### 验收标准

1. THE Cinema_Pro_HDR_System SHALL 输出.cube时写入头部元信息：源/目标色域、白点、曲线参数快照、版本与生成时间
2. THE Cinema_Pro_HDR_System SHALL 定义误差报告：在P3-D65中随机1,000,000点+边界格点全覆盖，报告均值、95/99分位数与最大值
3. THE Cinema_Pro_HDR_System SHALL 达到误差目标：均值≤0.5，99分位≤1.0，最大≤2.0，报告单位ΔE00@P3-D65，输出top-10 worst cases
4. THE Cinema_Pro_HDR_System SHALL 支持33³和65³分辨率的.cube格式LUT导出
5. IF 误差超过阈值时，THEN THE Cinema_Pro_HDR_System SHALL 返回非0退出码

### 需求 5: 侧车文件系统

**用户故事:** 作为后期制作管理员，我希望系统能够生成标准化的元数据文件，以便在不同工具间传递调色信息。

#### 验收标准

1. THE Cinema_Pro_HDR_System SHALL 实现JSON schema与版本策略：cph_version必填，时间码格式HH:MM:SS:FF，hash_clip_guid必填
2. THE Cinema_Pro_HDR_System SHALL 生成符合ST 2094-10标准的基础统计信息
3. THE Cinema_Pro_HDR_System SHALL 在CPH扩展块中保存完整的调色参数
4. WHEN 扩展块存在但参数不合法（越界/类型错）时，THE Cinema_Pro_HDR_System SHALL 回退2094-10并记录error_code
5. THE Cinema_Pro_HDR_System SHALL 提供JSON schema验证工具

### 需求 6: 性能优化

**用户故事:** 作为技术总监，我希望系统在各种硬件平台上都能达到实时性能要求，以便满足专业制作环境的需求。

#### 验收标准

1. THE Cinema_Pro_HDR_System SHALL 使用测量方法：3次冷/热跑，每次3000帧，丢弃前100帧，统计中位数与95分位
2. THE Cinema_Pro_HDR_System SHALL 在测试平台RTX 3080（Windows 11, Studio驱动）、M2 Max（macOS 14）上验证性能
3. THE Cinema_Pro_HDR_System SHALL 使用帧尺寸4096×2160@24p，图像集包含高对比/人像/霓虹/薄雾各≥2段
4. THE Cinema_Pro_HDR_System SHALL 达到性能目标：4K中位<1.0ms，P95<1.2ms；8K中位<3.5ms，P95<4.0ms，确定性模式下验收
5. THE Cinema_Pro_HDR_System SHALL 支持NVIDIA、AMD、Apple GPU平台

### 需求 7: 测试和验证

**用户故事:** 作为质量保证工程师，我希望有完整的测试套件来验证系统的数值精度和视觉质量，以确保产品质量。

#### 验收标准

1. THE Cinema_Pro_HDR_System SHALL 通过C¹测试以p±1e-3的左右差分评估，阈值<1e-3；单调以4096均匀采样+256个问题点采样
2. THE Cinema_Pro_HDR_System SHALL 通过闪烁测试：帧差RMS能量<阈值（默认0.02，单位PQ归一），1-6Hz区间能量不得因Highlight Detail打开而上升>20%
3. THE Cinema_Pro_HDR_System SHALL 确保开启/关闭Highlight Detail对比不应增加20%以上闪烁
4. THE Cinema_Pro_HDR_System SHALL 通过跨GPU对齐测试：确定性模式下验收，NVIDIA/AMD/Apple差异≤1 LSB
5. THE Cinema_Pro_HDR_System SHALL 在至少12个不同类型的测试样片上保持视觉质量

### 需求 8: 工具链支持

**用户故事:** 作为技术美术，我希望有命令行工具来批量处理和验证调色参数，以便自动化工作流程。

#### 验收标准

1. THE Cinema_Pro_HDR_System SHALL 提供cph_lut_baker支持--grid 33|65 --in-cs --out-cs --report json参数
2. THE Cinema_Pro_HDR_System SHALL 提供cph_json_check返回具体字段级错误码
3. THE Cinema_Pro_HDR_System SHALL 支持批处理脚本按时间码段拆分生成多段JSON（便于场景切换）
4. THE Cinema_Pro_HDR_System SHALL 支持参数文件的批量转换
5. THE Cinema_Pro_HDR_System SHALL 提供详细的错误报告和日志记录

### 需求 9: 文档和交付

**用户故事:** 作为最终用户，我希望有完整的文档和示例来学习和使用系统，以便快速上手并发挥系统的全部潜力。

#### 验收标准

1. THE Cinema_Pro_HDR_System SHALL 在用户手册中包含RCM/ACES放置建议（节点顺序截图）
2. THE Cinema_Pro_HDR_System SHALL 提供故障排除清单：黑位发灰、亮峰炸点、肤色偏移的处理与参数建议
3. THE Cinema_Pro_HDR_System SHALL 附带样例工程：三套预设、样片、输出LUT、侧车JSON、测试报告模板
4. THE Cinema_Pro_HDR_System SHALL 提供工程API文档，便于开发者集成
5. THE Cinema_Pro_HDR_System SHALL 提供验收测试报告模板
## 关键规格补全


### UI→算法映射

**PPR参数映射:**
- **Pivot**: 0.10–0.25（显示Nits与PQ双值，内部存PQ；默认≈PQ(0.18)）
- **Shadows Contrast**: S∈[0,1] → γs = 1.0 + 0.6*S
- **Highlight Contrast**: H∈[0,1] → γh = 0.8 + 0.6*H
- **Highlights Roll-off**: R∈[0,1] → h = 0.5 + 2.5*R
- **Highlight Detail**: λ_hd∈[0,1]（仅x>p，USM/Laplacian限幅，r=2px, amount=0.2, thr=0.03）
- **Black Lift**: blift∈[0,0.02]（随后toe夹持，默认0.002）
- **Saturation(Base)**: sat_base∈[0,2]
- **Saturation(Highlights)**: sat_hi∈[0,2]（权重w_hi=smoothstep(p,1,x)）

**RLOG参数映射:**
- **Shadow Lift (RLOG)**: S∈[0,1] → a = 1 + 15*S
- **Highlight Gain (RLOG)**: G∈[0,1] → b = 0.8 + 0.4*G
- **Highlight Roll-off (RLOG)**: R∈[0,1] → c = 0.5 + 2.5*R
- **Blend Threshold**: B∈[0,1] → t = 0.4 + 0.3*B
- 在拼接区[t-0.05, t+0.05]做smoothstep混合

### 预设快照（首版建议）

| 预设 | p | γs | γh | h | blift | λ_hd | sat_base | sat_hi |
|------|---|----|----|---|-------|------|----------|--------|
| Cinema-Flat | PQ(0.18) | 1.10 | 1.05 | 1.0 | 0.003 | 0.2 | 1.00 | 0.95 |
| Cinema-Punch | PQ(0.18) | 1.40 | 1.10 | 1.8 | 0.002 | 0.4 | 1.05 | 1.00 |
| Cinema-Highlight | PQ(0.20) | 1.20 | 0.95 | 1.2 | 0.004 | 0.6 | 0.98 | 0.92 |

### JSON侧车Schema（完整版）

```json
{
  "$id": "https://example.com/cph.schema.json",
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "title": "Cinema Pro HDR Sidecar",
  "type": "object",
  "additionalProperties": false,
  "required": ["st2094_10", "cph_meta"],
  "properties": {
    "st2094_10": {
      "type": "object",
      "additionalProperties": false,
      "required": ["minPqEncodedMaxRGB","avgPqEncodedMaxRGB","maxPqEncodedMaxRGB","offset","gain","gamma"],
      "properties": {
        "minPqEncodedMaxRGB": {"type":"number","minimum":0,"maximum":10000},
        "avgPqEncodedMaxRGB": {"type":"number","minimum":0,"maximum":10000},
        "maxPqEncodedMaxRGB": {"type":"number","minimum":0,"maximum":10000},
        "offset": {"type":"number","minimum":-1,"maximum":1},
        "gain": {"type":"number","minimum":0,"maximum":4},
        "gamma": {"type":"number","minimum":0.5,"maximum":2.5}
      }
    },
    "cph_meta": {
      "type": "object",
      "additionalProperties": false,
      "required": ["cph_version","cph_curve_id","pivot","work_cs"],
      "properties": {
        "cph_version": {"type":"integer","minimum":2,"maximum":2},
        "cph_curve_id": {"type":"integer","enum":[0,1]},
        "pivot": {"type":"number","minimum":0.05,"maximum":0.30},
        "gamma_s": {"type":"number","minimum":1.0,"maximum":1.6},
        "gamma_h": {"type":"number","minimum":0.8,"maximum":1.4},
        "shoulder": {"type":"number","minimum":0.5,"maximum":3.0},
        "black_lift": {"type":"number","minimum":0.0,"maximum":0.02},
        "highlight_detail": {"type":"number","minimum":0.0,"maximum":1.0},
        "sat_base": {"type":"number","minimum":0.0,"maximum":2.0},
        "sat_hi": {"type":"number","minimum":0.0,"maximum":2.0},
        "work_cs": {"type":"string","enum":["BT2020_PQ"]},
        "hash_clip_guid": {"type":"string","format":"uuid"},
        "timecode_inout": {"type":"string","pattern":"^[0-2][0-9]:[0-5][0-9]:[0-5][0-9]:[0-5][0-9]-[0-2][0-9]:[0-5][0-9]:[0-5][0-9]:[0-5][0-9]$"},
        "generator": {"type":"string"}
      }
    }
  }
}
```

**校验规则要点**: 字段类型/范围校验；缺失/越界 → error_code并回退基础层。

### 性能测量方法

- **平台**: Windows 11 + RTX3080（Studio驱动）、macOS 14 + M2 Max
- **序列**: 4× 12s段（夜景、人像、霓虹、薄雾）；分辨率4096×2160@24p
- **方式**: 预热100帧后计时3000帧，记录中位数、P95（ms/帧）
- **目标**: 4K中位<1.0ms，P95<1.2ms；8K中位<3.5ms，P95<4.0ms

### 失败保护统一条款

- 所有中间量saturate()；捕获NaN/Inf/除零；节流日志（≤10条/秒）；可选"严格模式"触发硬回退
- 异常回退路径：解析失败/参数异常/算法异常 → y=x或仅用2094-10基础层，带error_code
- 日志格式：[ts][level][clip_guid][tc] code=..., field=..., val=..., action=FALLBACK2094|IDENTITY
- 节流策略：同一code每秒最多10条，超出写1条聚合摘要

### 错误码表

| code | 级别 | 含义 | 处理 |
|------|------|------|------|
| `SCHEMA_MISSING` | error | 缺少必填块/字段 | 回退2094-10；记录clip_guid/tc |
| `RANGE_PIVOT` | warn | `pivot`越界 | 钳制到边界；记录 |
| `RANGE_KNEE` | warn | `yknee/alpha/toe`越界 | 复位默认；记录 |
| `NAN_INF` | error | 运算出现NaN/Inf | 回退`y=x`（硬回退） |
| `DET_MISMATCH` | warn | 确定性开关关闭导致跨GPU差异 | 提示；建议开启 |
| `HL_FLICKER` | warn | 高光细节引起的闪烁超阈值 | 自动降低λ_hd或关闭细节层 |

### RCM/ACES放置建议

- **RCM**: Color Space Transform之前 → 进入工作域（BT2020+PQ）→ CPH节点 → 回时间线域
- **ACES**: ACESCCT → PQ归一 → CPH节点 → PQ→ACESCCT
### 需求 10
: DCI合规模式

**用户故事:** 作为发行/放映侧，我需要一键切换到符合DCI HDR交付约束的输出，以便顺利过审与稳定放映。

#### 验收标准

1. THE Cinema_Pro_HDR_System SHALL 提供DCI_Compliance_Mode开关；开启时输出色容积限制为P3-D65（白点299.6 cd/m²≈300 nits目标），最低活动黑位按影院目标0.005 cd/m²约束
2. THE Cinema_Pro_HDR_System SHALL 固定变换路径为：工作域（BT.2020+PQ）→P3-D65→X″Y″Z″（12-bit）量化策略说明写入交付报告
3. WHEN 导出DCP/DCDM时，THE Cinema_Pro_HDR_System SHALL 在输出报告中列出：白点/黑位测得值、EOTF=ST2084标识、峰值跟踪容差、色域边界抽样检查结果
4. IF DCI_Compliance_Mode下出现指标越界，THEN THE Cinema_Pro_HDR_System SHALL 返回非0退出码并在报告中标注失败样本帧与坐标（至少10个）

### 需求 11: 色域越界处理

**用户故事:** 作为调色师，我要确保回到目标域时颜色不出界，同时尽量保持感知外观。

#### 验收标准

1. THE Cinema_Pro_HDR_System SHALL 在从工作域返回目标域时执行两级处理：一级线性压制（3×3矩阵）和二级感知夹持（OKLab中以最小ΔE方式）
2. THE Cinema_Pro_HDR_System SHALL 在--report中输出越界像素占比、最大越界幅度与示例色点（≥10个）
3. WHEN DCI_Compliance_Mode=ON时，THE Cinema_Pro_HDR_System SHALL 默认启用二级感知夹持并将sat_hi施加5-10%的保守衰减（可配置）
4. THE Cinema_Pro_HDR_System SHALL 在Bake环节不使用随机扰动；在Deterministic模式下不同GPU输出字节级一致
5. WHEN 越界/合规检查失败时，THE Cinema_Pro_HDR_System SHALL 增加error_code DCI_BOUND/GAMUT_OOG，日志按节流策略记录