#!/usr/bin/env python3
"""
Cinema Pro HDR 高精度参考数学实现 (RefMath)

用于生成64位精度的参考曲线和色彩转换，作为CI对比的黄金标准。
包含PPR/RLOG色调映射、OKLab色彩空间转换和ΔE00计算。

目的：
- 提供数值精确的参考实现
- 生成稠密采样表用于CI对比
- 一键定位GPU/OFX/DCTL输出与RefMath的差异
- 避免"看起来像"的主观争执
"""

import numpy as np
import json
import csv
import argparse
from pathlib import Path
from typing import Dict, List, Tuple, Optional
import matplotlib.pyplot as plt
from dataclasses import dataclass

# 使用64位精度
np.seterr(all='raise')  # 让数值错误抛出异常

@dataclass
class CphParams:
    """Cinema Pro HDR 参数结构"""
    curve_type: int  # 0=PPR, 1=RLOG
    pivot_pq: float  # [0.05, 0.30]
    gamma_s: float   # [1.0, 1.6] 
    gamma_h: float   # [0.8, 1.4]
    shoulder_h: float # [0.5, 3.0]
    black_lift: float # [0.0, 0.02]
    highlight_detail: float # [0.0, 1.0]
    sat_base: float  # [0.0, 2.0]
    sat_hi: float    # [0.0, 2.0]
    
    # RLOG专用参数
    rlog_a: float = 8.0    # [1, 16]
    rlog_b: float = 1.0    # [0.8, 1.2]
    rlog_c: float = 1.5    # [0.5, 3.0]
    rlog_t: float = 0.55   # [0.4, 0.7]
    
    # 软膝参数
    yknee: float = 0.97    # [0.95, 0.99]
    alpha: float = 0.6     # [0.2, 1.0]
    toe: float = 0.002     # [0.0, 0.01]

class RefMathPQ:
    """高精度PQ EOTF/OETF实现 (ST 2084)"""
    
    # PQ常数 (使用64位精度)
    M1 = np.float64(0.1593017578125)      # 2610/16384
    M2 = np.float64(78.84375)             # 2523/32
    C1 = np.float64(0.8359375)            # 3424/4096
    C2 = np.float64(18.8515625)           # 2413/128
    C3 = np.float64(18.6875)              # 2392/128
    
    @classmethod
    def eotf(cls, pq_value: np.ndarray) -> np.ndarray:
        """PQ EOTF: PQ编码值 → 线性光 (cd/m²)"""
        pq_value = np.asarray(pq_value, dtype=np.float64)
        
        # 处理边界情况
        result = np.zeros_like(pq_value)
        valid_mask = (pq_value > 0) & (pq_value < 1) & np.isfinite(pq_value)
        
        if np.any(valid_mask):
            pq_pow = np.power(pq_value[valid_mask], 1.0 / cls.M2)
            numerator = np.maximum(0.0, pq_pow - cls.C1)
            denominator = cls.C2 - cls.C3 * pq_pow
            
            # 避免除零
            safe_mask = denominator > 1e-15
            if np.any(safe_mask):
                linear = np.power(numerator[safe_mask] / denominator[safe_mask], 1.0 / cls.M1)
                result[valid_mask][safe_mask] = linear * 10000.0
        
        # 边界值处理
        result[pq_value >= 1.0] = 10000.0
        result[pq_value <= 0.0] = 0.0
        
        return result
    
    @classmethod
    def oetf(cls, linear_value: np.ndarray) -> np.ndarray:
        """PQ OETF: 线性光 (cd/m²) → PQ编码值"""
        linear_value = np.asarray(linear_value, dtype=np.float64)
        
        # 处理边界情况
        result = np.zeros_like(linear_value)
        valid_mask = (linear_value > 0) & (linear_value < 10000) & np.isfinite(linear_value)
        
        if np.any(valid_mask):
            normalized = linear_value[valid_mask] / 10000.0
            pow_m1 = np.power(normalized, cls.M1)
            numerator = cls.C1 + cls.C2 * pow_m1
            denominator = 1.0 + cls.C3 * pow_m1
            
            # 避免除零
            safe_mask = denominator > 1e-15
            if np.any(safe_mask):
                pq_result = np.power(numerator[safe_mask] / denominator[safe_mask], cls.M2)
                result[valid_mask][safe_mask] = pq_result
        
        # 边界值处理
        result[linear_value >= 10000.0] = 1.0
        result[linear_value <= 0.0] = 0.0
        
        return result

class RefMathColorSpace:
    """高精度色彩空间转换"""
    
    # 精确的色彩空间转换矩阵 (64位精度)
    BT2020_TO_XYZ = np.array([
        [0.6369580848311005, 0.14461690358620832, 0.16888097516417266],
        [0.2627045017467606, 0.6779980715188708,  0.05929741649732864],
        [0.0000000000000000, 0.028072693049087428, 1.0609850577107909]
    ], dtype=np.float64)
    
    XYZ_TO_BT2020 = np.array([
        [ 1.7166511879712674, -0.35567078377639233, -0.25336628137365974],
        [-0.6666844518324892,  1.6164812366349395,   0.01576854581391113],
        [ 0.017639857445311,  -0.042770613257808524, 0.9421031212354738]
    ], dtype=np.float64)
    
    P3D65_TO_XYZ = np.array([
        [0.4865709486482162, 0.26566890814394226, 0.1982172852343625],
        [0.2289745640697488, 0.6917385218365064,  0.079286914093745],
        [0.0000000000000000, 0.04511338185890264, 1.043944368900976]
    ], dtype=np.float64)
    
    XYZ_TO_P3D65 = np.array([
        [ 2.493496911941425,  -0.9313836179191239, -0.40271078445071684],
        [-0.8294889695615747,  1.7626640603183463,  0.023624685841943577],
        [ 0.03584583024378447, -0.07617238926804182, 0.9568845240076872]
    ], dtype=np.float64)
    
    @classmethod
    def bt2020_to_p3d65(cls, bt2020_rgb: np.ndarray) -> np.ndarray:
        """BT.2020 → P3-D65 转换"""
        bt2020_rgb = np.asarray(bt2020_rgb, dtype=np.float64)
        
        # BT.2020 → XYZ → P3-D65
        xyz = cls.BT2020_TO_XYZ @ bt2020_rgb.T
        p3d65 = cls.XYZ_TO_P3D65 @ xyz
        
        return p3d65.T
    
    @classmethod
    def p3d65_to_bt2020(cls, p3d65_rgb: np.ndarray) -> np.ndarray:
        """P3-D65 → BT.2020 转换"""
        p3d65_rgb = np.asarray(p3d65_rgb, dtype=np.float64)
        
        # P3-D65 → XYZ → BT.2020
        xyz = cls.P3D65_TO_XYZ @ p3d65_rgb.T
        bt2020 = cls.XYZ_TO_BT2020 @ xyz
        
        return bt2020.T

class RefMathOKLab:
    """高精度OKLab色彩空间转换"""
    
    # OKLab转换矩阵 (64位精度)
    LINEAR_SRGB_TO_LMS = np.array([
        [0.4122214708, 0.5363325363, 0.0514459929],
        [0.2119034982, 0.6806995451, 0.1073969566],
        [0.0883024619, 0.2817188376, 0.6299787005]
    ], dtype=np.float64)
    
    LMS_TO_LINEAR_SRGB = np.array([
        [ 4.0767416621, -3.3077115913,  0.2309699292],
        [-1.2684380046,  2.6097574011, -0.3413193965],
        [-0.0041960863, -0.7034186147,  1.7076147010]
    ], dtype=np.float64)
    
    LMS_TO_OKLAB = np.array([
        [0.2104542553, 0.7936177850, -0.0040720468],
        [1.9779984951, -2.4285922050, 0.4505937099],
        [0.0259040371, 0.7827717662, -0.8086757660]
    ], dtype=np.float64)
    
    OKLAB_TO_LMS = np.array([
        [0.99999999845051981432, 0.39633779217376785678,   0.21580375806075880339],
        [1.0000000088817607767,  -0.1055613423236563494,  -0.063854174771705903402],
        [1.0000000546724109177,  -0.089484182094965759684, -1.2914855378640917399]
    ], dtype=np.float64)
    
    @classmethod
    def linear_srgb_to_oklab(cls, rgb: np.ndarray) -> np.ndarray:
        """线性sRGB → OKLab"""
        rgb = np.asarray(rgb, dtype=np.float64)
        
        # RGB → LMS
        lms = cls.LINEAR_SRGB_TO_LMS @ rgb.T
        
        # 立方根变换 (处理负值)
        lms_cbrt = np.sign(lms) * np.power(np.abs(lms), 1.0/3.0)
        
        # LMS → OKLab
        oklab = cls.LMS_TO_OKLAB @ lms_cbrt
        
        return oklab.T
    
    @classmethod
    def oklab_to_linear_srgb(cls, oklab: np.ndarray) -> np.ndarray:
        """OKLab → 线性sRGB"""
        oklab = np.asarray(oklab, dtype=np.float64)
        
        # OKLab → LMS (立方根空间)
        lms_cbrt = cls.OKLAB_TO_LMS @ oklab.T
        
        # 立方变换
        lms = np.sign(lms_cbrt) * np.power(np.abs(lms_cbrt), 3.0)
        
        # LMS → RGB
        rgb = cls.LMS_TO_LINEAR_SRGB @ lms
        
        return rgb.T
    
    @classmethod
    def adjust_saturation(cls, oklab: np.ndarray, saturation: float) -> np.ndarray:
        """在OKLab空间调整饱和度"""
        oklab = np.asarray(oklab, dtype=np.float64)
        result = oklab.copy()
        
        # 调整a和b通道 (色度)
        result[..., 1] *= saturation
        result[..., 2] *= saturation
        
        return result

class RefMathDeltaE:
    """高精度ΔE00计算"""
    
    @classmethod
    def delta_e_00(cls, lab1: np.ndarray, lab2: np.ndarray) -> np.ndarray:
        """
        计算CIE ΔE00色差
        
        Args:
            lab1, lab2: LAB色彩值 [..., 3]
            
        Returns:
            ΔE00值 [...]
        """
        lab1 = np.asarray(lab1, dtype=np.float64)
        lab2 = np.asarray(lab2, dtype=np.float64)
        
        L1, a1, b1 = lab1[..., 0], lab1[..., 1], lab1[..., 2]
        L2, a2, b2 = lab2[..., 0], lab2[..., 1], lab2[..., 2]
        
        # 计算C和h
        C1 = np.sqrt(a1**2 + b1**2)
        C2 = np.sqrt(a2**2 + b2**2)
        
        C_avg = (C1 + C2) / 2.0
        
        # G因子
        G = 0.5 * (1 - np.sqrt(C_avg**7 / (C_avg**7 + 25**7)))
        
        # 修正的a值
        a1_prime = (1 + G) * a1
        a2_prime = (1 + G) * a2
        
        # 修正的C和h
        C1_prime = np.sqrt(a1_prime**2 + b1**2)
        C2_prime = np.sqrt(a2_prime**2 + b2**2)
        
        h1_prime = np.arctan2(b1, a1_prime) * 180 / np.pi
        h2_prime = np.arctan2(b2, a2_prime) * 180 / np.pi
        
        # 确保h在[0, 360)范围内
        h1_prime = np.where(h1_prime < 0, h1_prime + 360, h1_prime)
        h2_prime = np.where(h2_prime < 0, h2_prime + 360, h2_prime)
        
        # 计算差值
        delta_L_prime = L2 - L1
        delta_C_prime = C2_prime - C1_prime
        
        # 色相差计算
        delta_h_prime = h2_prime - h1_prime
        delta_h_prime = np.where(np.abs(delta_h_prime) > 180,
                                delta_h_prime - 360 * np.sign(delta_h_prime),
                                delta_h_prime)
        
        delta_H_prime = 2 * np.sqrt(C1_prime * C2_prime) * np.sin(np.radians(delta_h_prime / 2))
        
        # 平均值
        L_avg = (L1 + L2) / 2.0
        C_prime_avg = (C1_prime + C2_prime) / 2.0
        
        h_prime_avg = (h1_prime + h2_prime) / 2.0
        h_prime_avg = np.where(np.abs(h1_prime - h2_prime) > 180,
                              h_prime_avg + 180, h_prime_avg)
        h_prime_avg = np.where(h_prime_avg >= 360, h_prime_avg - 360, h_prime_avg)
        
        # 权重函数
        T = (1 - 0.17 * np.cos(np.radians(h_prime_avg - 30)) +
             0.24 * np.cos(np.radians(2 * h_prime_avg)) +
             0.32 * np.cos(np.radians(3 * h_prime_avg + 6)) -
             0.20 * np.cos(np.radians(4 * h_prime_avg - 63)))
        
        delta_theta = 30 * np.exp(-((h_prime_avg - 275) / 25)**2)
        
        R_C = 2 * np.sqrt(C_prime_avg**7 / (C_prime_avg**7 + 25**7))
        
        S_L = 1 + (0.015 * (L_avg - 50)**2) / np.sqrt(20 + (L_avg - 50)**2)
        S_C = 1 + 0.045 * C_prime_avg
        S_H = 1 + 0.015 * C_prime_avg * T
        
        R_T = -np.sin(2 * np.radians(delta_theta)) * R_C
        
        # 最终ΔE00计算
        kL = kC = kH = 1.0  # 标准权重
        
        delta_E_00 = np.sqrt(
            (delta_L_prime / (kL * S_L))**2 +
            (delta_C_prime / (kC * S_C))**2 +
            (delta_H_prime / (kH * S_H))**2 +
            R_T * (delta_C_prime / (kC * S_C)) * (delta_H_prime / (kH * S_H))
        )
        
        return delta_E_00

class RefMathToneMapping:
    """高精度色调映射算法"""
    
    @classmethod
    def ppr_curve(cls, x: np.ndarray, params: CphParams) -> np.ndarray:
        """
        PPR (Pivoted Power-Rational) 色调映射曲线
        
        数学模型：
        - 阴影段：基于枢轴的幂函数 y = (x/p)^γs * p
        - 高光段：有理式函数 y = (x/(1+h*x))^γh  
        - 在枢轴点平滑拼接
        """
        x = np.asarray(x, dtype=np.float64)
        p = np.float64(params.pivot_pq)
        gamma_s = np.float64(params.gamma_s)
        gamma_h = np.float64(params.gamma_h)
        h = np.float64(params.shoulder_h)
        
        result = np.zeros_like(x)
        
        # 阴影段 (x <= p): y = (x/p)^γs * p
        shadow_mask = x <= p
        if np.any(shadow_mask):
            x_shadow = x[shadow_mask]
            # 处理x=0的情况
            zero_mask = x_shadow == 0
            nonzero_mask = x_shadow > 0
            
            if np.any(nonzero_mask):
                ratio = x_shadow[nonzero_mask] / p
                result[shadow_mask][nonzero_mask] = np.power(ratio, gamma_s) * p
            
            # x=0时结果为0
            result[shadow_mask][zero_mask] = 0.0
        
        # 高光段 (x > p): y = (x/(1+h*x))^γh
        highlight_mask = x > p
        if np.any(highlight_mask):
            x_highlight = x[highlight_mask]
            denominator = 1.0 + h * x_highlight
            ratio = x_highlight / denominator
            result[highlight_mask] = np.power(ratio, gamma_h)
        
        # 应用软膝
        result = cls._apply_soft_knee(result, params)
        
        # 应用toe夹持
        result = cls._apply_toe_clamp(result, params)
        
        return result
    
    @classmethod
    def rlog_curve(cls, x: np.ndarray, params: CphParams) -> np.ndarray:
        """
        RLOG (Rational Logarithmic) 色调映射曲线
        
        数学模型：
        - 暗部段: y1 = log(1 + a*x) / log(1 + a)
        - 高光段: y2 = (b*x) / (1 + c*x)
        - 在阈值t附近平滑拼接
        """
        x = np.asarray(x, dtype=np.float64)
        a = np.float64(params.rlog_a)
        b = np.float64(params.rlog_b)
        c = np.float64(params.rlog_c)
        t = np.float64(params.rlog_t)
        
        # 暗部段: y1 = log(1 + a*x) / log(1 + a)
        log_denom = np.log(1.0 + a)
        y1 = np.log(1.0 + a * x) / log_denom
        
        # 高光段: y2 = (b*x) / (1 + c*x)
        denominator = 1.0 + c * x
        y2 = (b * x) / denominator
        
        # 确保在拼接点连续性，调整参数使y1(t) ≈ y2(t)
        y1_at_t = np.log(1.0 + a * t) / log_denom
        y2_at_t = (b * t) / (1.0 + c * t)
        
        # 如果差异太大，调整b参数来匹配
        if abs(y1_at_t - y2_at_t) > 0.1:
            # 重新计算b使得在t点连续
            b_adjusted = y1_at_t * (1.0 + c * t) / t if t > 0 else b
            y2 = (b_adjusted * x) / denominator
        
        # 在t±0.05区间平滑拼接
        blend_width = 0.05
        t_low = t - blend_width
        t_high = t + blend_width
        
        # 计算混合权重 (使用smoothstep)
        weight = np.zeros_like(x)
        
        # x < t_low: 完全使用y1
        low_mask = x < t_low
        weight[low_mask] = 0.0
        
        # x > t_high: 完全使用y2  
        high_mask = x > t_high
        weight[high_mask] = 1.0
        
        # t_low <= x <= t_high: 平滑过渡
        blend_mask = (x >= t_low) & (x <= t_high)
        if np.any(blend_mask):
            t_norm = (x[blend_mask] - t_low) / (2 * blend_width)
            weight[blend_mask] = cls._smoothstep(t_norm)
        
        # 混合结果
        result = (1.0 - weight) * y1 + weight * y2
        
        # 应用软膝和toe夹持
        result = cls._apply_soft_knee(result, params)
        result = cls._apply_toe_clamp(result, params)
        
        return result
    
    @classmethod
    def _apply_soft_knee(cls, y: np.ndarray, params: CphParams) -> np.ndarray:
        """应用软膝处理"""
        yknee = np.float64(params.yknee)
        alpha = np.float64(params.alpha)
        
        # 软膝函数: 在接近1.0时平滑过渡
        knee_mask = y > yknee
        if np.any(knee_mask):
            y_knee = y[knee_mask]
            excess = y_knee - yknee
            # 软膝公式: y_new = yknee + excess / (1 + alpha * excess)
            y[knee_mask] = yknee + excess / (1.0 + alpha * excess)
        
        return y
    
    @classmethod
    def _apply_toe_clamp(cls, y: np.ndarray, params: CphParams) -> np.ndarray:
        """应用toe夹持"""
        toe = np.float64(params.toe)
        
        # 在接近0时应用最小值夹持
        y = np.maximum(y, toe)
        
        return y
    
    @classmethod
    def _smoothstep(cls, t: np.ndarray) -> np.ndarray:
        """平滑步进函数"""
        t = np.clip(t, 0.0, 1.0)
        return t * t * (3.0 - 2.0 * t)

class RefMathGenerator:
    """参考数学数据生成器"""
    
    def __init__(self, output_dir: str = "golden"):
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(exist_ok=True)
    
    def generate_curve_tables(self, num_samples: int = 16384) -> Dict:
        """生成稠密采样的曲线表"""
        
        # 创建三套预设参数
        presets = {
            "Cinema-Flat": CphParams(
                curve_type=0, pivot_pq=0.18, gamma_s=1.10, gamma_h=1.05, 
                shoulder_h=1.0, black_lift=0.003, highlight_detail=0.2,
                sat_base=1.00, sat_hi=0.95
            ),
            "Cinema-Punch": CphParams(
                curve_type=0, pivot_pq=0.18, gamma_s=1.40, gamma_h=1.10,
                shoulder_h=1.8, black_lift=0.002, highlight_detail=0.4,
                sat_base=1.05, sat_hi=1.00
            ),
            "Cinema-Highlight": CphParams(
                curve_type=0, pivot_pq=0.20, gamma_s=1.20, gamma_h=0.95,
                shoulder_h=1.2, black_lift=0.004, highlight_detail=0.6,
                sat_base=0.98, sat_hi=0.92
            )
        }
        
        # 生成输入采样点 (PQ归一化域)
        x_samples = np.linspace(0.0, 1.0, num_samples, dtype=np.float64)
        
        curve_data = {}
        
        for preset_name, params in presets.items():
            print(f"生成 {preset_name} 曲线数据...")
            
            # PPR曲线
            ppr_y = RefMathToneMapping.ppr_curve(x_samples, params)
            
            # RLOG曲线 (使用RLOG参数)
            rlog_params = CphParams(
                curve_type=1, pivot_pq=params.pivot_pq,
                gamma_s=params.gamma_s, gamma_h=params.gamma_h,
                shoulder_h=params.shoulder_h, black_lift=params.black_lift,
                highlight_detail=params.highlight_detail,
                sat_base=params.sat_base, sat_hi=params.sat_hi,
                rlog_a=8.0, rlog_b=1.0, rlog_c=1.5, rlog_t=0.55
            )
            rlog_y = RefMathToneMapping.rlog_curve(x_samples, rlog_params)
            
            curve_data[preset_name] = {
                "x_samples": x_samples.tolist(),
                "ppr_y": ppr_y.tolist(),
                "rlog_y": rlog_y.tolist(),
                "params": {
                    "pivot_pq": params.pivot_pq,
                    "gamma_s": params.gamma_s,
                    "gamma_h": params.gamma_h,
                    "shoulder_h": params.shoulder_h,
                    "black_lift": params.black_lift,
                    "highlight_detail": params.highlight_detail,
                    "sat_base": params.sat_base,
                    "sat_hi": params.sat_hi
                }
            }
        
        return curve_data
    
    def generate_color_space_tables(self, num_samples: int = 1000) -> Dict:
        """生成色彩空间转换表"""
        
        print("生成色彩空间转换数据...")
        
        # 生成随机RGB样本 (在[0,1]范围内)
        np.random.seed(42)  # 确保可重现
        rgb_samples = np.random.rand(num_samples, 3).astype(np.float64)
        
        # BT.2020 ↔ P3-D65 转换
        p3d65_samples = RefMathColorSpace.bt2020_to_p3d65(rgb_samples)
        bt2020_roundtrip = RefMathColorSpace.p3d65_to_bt2020(p3d65_samples)
        
        # PQ EOTF/OETF 转换
        linear_samples = RefMathPQ.eotf(rgb_samples)
        pq_roundtrip = RefMathPQ.oetf(linear_samples)
        
        # OKLab 转换
        oklab_samples = RefMathOKLab.linear_srgb_to_oklab(rgb_samples)
        rgb_from_oklab = RefMathOKLab.oklab_to_linear_srgb(oklab_samples)
        
        return {
            "bt2020_samples": rgb_samples.tolist(),
            "p3d65_converted": p3d65_samples.tolist(),
            "bt2020_roundtrip": bt2020_roundtrip.tolist(),
            "pq_samples": rgb_samples.tolist(),
            "linear_converted": linear_samples.tolist(),
            "pq_roundtrip": pq_roundtrip.tolist(),
            "oklab_samples": rgb_samples.tolist(),
            "oklab_converted": oklab_samples.tolist(),
            "rgb_from_oklab": rgb_from_oklab.tolist(),
            "conversion_errors": {
                "bt2020_p3d65_max_error": float(np.max(np.abs(rgb_samples - bt2020_roundtrip))),
                "pq_eotf_oetf_max_error": float(np.max(np.abs(rgb_samples - pq_roundtrip))),
                "oklab_roundtrip_max_error": float(np.max(np.abs(rgb_samples - rgb_from_oklab)))
            }
        }
    
    def generate_delta_e_reference(self, num_samples: int = 1000) -> Dict:
        """生成ΔE00参考数据"""
        
        print("生成ΔE00参考数据...")
        
        # 生成LAB色彩样本
        np.random.seed(123)
        lab1_samples = np.random.rand(num_samples, 3).astype(np.float64)
        lab1_samples[:, 0] *= 100  # L: [0, 100]
        lab1_samples[:, 1] = (lab1_samples[:, 1] - 0.5) * 200  # a: [-100, 100]
        lab1_samples[:, 2] = (lab1_samples[:, 2] - 0.5) * 200  # b: [-100, 100]
        
        # 生成略有差异的第二组样本
        lab2_samples = lab1_samples + np.random.normal(0, 5, lab1_samples.shape)
        
        # 计算ΔE00
        delta_e_values = RefMathDeltaE.delta_e_00(lab1_samples, lab2_samples)
        
        return {
            "lab1_samples": lab1_samples.tolist(),
            "lab2_samples": lab2_samples.tolist(),
            "delta_e_00": delta_e_values.tolist(),
            "statistics": {
                "mean_delta_e": float(np.mean(delta_e_values)),
                "max_delta_e": float(np.max(delta_e_values)),
                "min_delta_e": float(np.min(delta_e_values)),
                "std_delta_e": float(np.std(delta_e_values))
            }
        }
    
    def save_reference_data(self):
        """保存所有参考数据"""
        
        print("开始生成RefMath参考数据...")
        
        # 生成曲线表
        curve_data = self.generate_curve_tables(16384)
        
        # 保存为CSV格式 (便于CI脚本读取)
        curves_csv_path = self.output_dir / "curves.csv"
        with open(curves_csv_path, 'w', newline='', encoding='utf-8') as f:
            writer = csv.writer(f)
            
            # 写入头部
            writer.writerow([
                "x", "Cinema-Flat_PPR", "Cinema-Flat_RLOG",
                "Cinema-Punch_PPR", "Cinema-Punch_RLOG", 
                "Cinema-Highlight_PPR", "Cinema-Highlight_RLOG"
            ])
            
            # 写入数据
            x_samples = curve_data["Cinema-Flat"]["x_samples"]
            for i, x in enumerate(x_samples):
                row = [x]
                for preset in ["Cinema-Flat", "Cinema-Punch", "Cinema-Highlight"]:
                    row.append(curve_data[preset]["ppr_y"][i])
                    row.append(curve_data[preset]["rlog_y"][i])
                writer.writerow(row)
        
        print(f"曲线数据已保存到: {curves_csv_path}")
        
        # 生成色彩空间转换数据
        color_data = self.generate_color_space_tables(10000)
        
        # 生成ΔE00参考数据
        delta_e_data = self.generate_delta_e_reference(5000)
        
        # 保存完整的参考数据为JSON
        reference_data = {
            "metadata": {
                "generator": "Cinema Pro HDR RefMath",
                "version": "1.0",
                "precision": "float64",
                "samples_per_curve": 16384,
                "color_samples": 10000,
                "delta_e_samples": 5000,
                "generated_at": "2025-01-24T00:00:00Z"
            },
            "curves": curve_data,
            "color_spaces": color_data,
            "delta_e": delta_e_data
        }
        
        json_path = self.output_dir / "reference_data.json"
        with open(json_path, 'w', encoding='utf-8') as f:
            json.dump(reference_data, f, indent=2, ensure_ascii=False)
        
        print(f"完整参考数据已保存到: {json_path}")
        
        # 生成验证报告
        self._generate_validation_report(reference_data)
    
    def _generate_validation_report(self, reference_data: Dict):
        """生成验证报告"""
        
        report_path = self.output_dir / "validation_report.md"
        
        with open(report_path, 'w', encoding='utf-8') as f:
            f.write("# Cinema Pro HDR RefMath 验证报告\n\n")
            f.write(f"生成时间: {reference_data['metadata']['generated_at']}\n")
            f.write(f"精度: {reference_data['metadata']['precision']}\n\n")
            
            f.write("## 曲线数据统计\n\n")
            for preset_name, preset_data in reference_data['curves'].items():
                f.write(f"### {preset_name}\n\n")
                ppr_y = np.array(preset_data['ppr_y'])
                rlog_y = np.array(preset_data['rlog_y'])
                
                f.write(f"- PPR曲线范围: [{np.min(ppr_y):.6f}, {np.max(ppr_y):.6f}]\n")
                f.write(f"- RLOG曲线范围: [{np.min(rlog_y):.6f}, {np.max(rlog_y):.6f}]\n")
                f.write(f"- PPR单调性: {'✓' if np.all(np.diff(ppr_y) >= 0) else '✗'}\n")
                f.write(f"- RLOG单调性: {'✓' if np.all(np.diff(rlog_y) >= 0) else '✗'}\n\n")
            
            f.write("## 色彩空间转换精度\n\n")
            errors = reference_data['color_spaces']['conversion_errors']
            f.write(f"- BT.2020 ↔ P3-D65 最大往返误差: {errors['bt2020_p3d65_max_error']:.2e}\n")
            f.write(f"- PQ EOTF/OETF 最大往返误差: {errors['pq_eotf_oetf_max_error']:.2e}\n")
            f.write(f"- OKLab 最大往返误差: {errors['oklab_roundtrip_max_error']:.2e}\n\n")
            
            f.write("## ΔE00 统计\n\n")
            stats = reference_data['delta_e']['statistics']
            f.write(f"- 平均ΔE00: {stats['mean_delta_e']:.3f}\n")
            f.write(f"- 最大ΔE00: {stats['max_delta_e']:.3f}\n")
            f.write(f"- 最小ΔE00: {stats['min_delta_e']:.3f}\n")
            f.write(f"- 标准差: {stats['std_delta_e']:.3f}\n\n")
            
            f.write("## 使用说明\n\n")
            f.write("此参考数据用于CI中对比GPU/OFX/DCTL实现的数值精度：\n\n")
            f.write("1. `curves.csv`: 包含16384点的稠密曲线采样\n")
            f.write("2. `reference_data.json`: 完整的参考数据集\n")
            f.write("3. 对比时允许的最大误差: 1 LSB (10/12bit量化后)\n")
            f.write("4. 生成误差栅格图和top-10 worst points用于调试\n\n")
        
        print(f"验证报告已保存到: {report_path}")

def main():
    """主函数"""
    parser = argparse.ArgumentParser(description="生成Cinema Pro HDR高精度参考数学数据")
    parser.add_argument("--output-dir", default="golden", help="输出目录")
    parser.add_argument("--samples", type=int, default=16384, help="曲线采样点数")
    parser.add_argument("--plot", action="store_true", help="生成可视化图表")
    
    args = parser.parse_args()
    
    # 创建生成器并运行
    generator = RefMathGenerator(args.output_dir)
    generator.save_reference_data()
    
    if args.plot:
        # 生成可视化图表 (可选)
        print("生成可视化图表...")
        # TODO: 实现图表生成
    
    print("RefMath参考数据生成完成!")

if __name__ == "__main__":
    main()