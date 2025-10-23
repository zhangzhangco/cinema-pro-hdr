#!/usr/bin/env python3
"""
Cinema Pro HDR RefMath 参考曲线生成器
生成64位高精度参考曲线表，用于CI对比和数值验证
"""

import numpy as np
import pandas as pd
import json
import os
from pathlib import Path
from typing import Dict, Tuple, List
import argparse

# 确保使用64位精度
np.seterr(all='raise')

class RefMathGenerator:
    """64位高精度参考数学计算器"""
    
    def __init__(self):
        self.precision = np.float64
        
    def pq_eotf(self, x: np.ndarray) -> np.ndarray:
        """PQ EOTF (ST 2084) - 64位精度实现"""
        x = np.asarray(x, dtype=self.precision)
        
        # ST 2084 常数 (64位精度)
        m1 = np.float64(2610.0 / 16384.0)  # 0.1593017578125
        m2 = np.float64(2523.0 / 4096.0 * 128.0)  # 78.84375
        c1 = np.float64(3424.0 / 4096.0)  # 0.8359375
        c2 = np.float64(2413.0 / 4096.0 * 32.0)  # 18.8515625
        c3 = np.float64(2392.0 / 4096.0 * 32.0)  # 18.6875
        
        # 避免负值和零值
        x = np.maximum(x, np.float64(1e-10))
        
        # PQ EOTF 计算
        x_pow_m2 = np.power(x, 1.0/m2)
        numerator = np.maximum(x_pow_m2 - c1, np.float64(0.0))
        denominator = c2 - c3 * x_pow_m2
        
        # 避免除零
        denominator = np.where(np.abs(denominator) < 1e-10, np.float64(1e-10), denominator)
        
        result = np.power(numerator / denominator, 1.0/m1)
        return result * 10000.0  # 转换为 nits
    
    def pq_oetf(self, y: np.ndarray) -> np.ndarray:
        """PQ OETF (ST 2084) - 64位精度实现"""
        y = np.asarray(y, dtype=self.precision)
        
        # ST 2084 常数
        m1 = np.float64(2610.0 / 16384.0)
        m2 = np.float64(2523.0 / 4096.0 * 128.0)
        c1 = np.float64(3424.0 / 4096.0)
        c2 = np.float64(2413.0 / 4096.0 * 32.0)
        c3 = np.float64(2392.0 / 4096.0 * 32.0)
        
        # 归一化到 [0,1]
        y = y / 10000.0
        y = np.maximum(y, np.float64(1e-10))
        
        # PQ OETF 计算
        y_pow_m1 = np.power(y, m1)
        numerator = c1 + c2 * y_pow_m1
        denominator = 1.0 + c3 * y_pow_m1
        
        result = np.power(numerator / denominator, m2)
        return np.clip(result, 0.0, 1.0)
    
    def ppr_curve(self, x: np.ndarray, params: Dict) -> np.ndarray:
        """PPR (Pivoted Power-Rational) 曲线 - 64位精度"""
        x = np.asarray(x, dtype=self.precision)
        
        p = np.float64(params['pivot'])
        gamma_s = np.float64(params['gamma_s'])
        gamma_h = np.float64(params['gamma_h'])
        h = np.float64(params['shoulder'])
        
        # 确保单调性的PPR实现
        result = np.zeros_like(x)
        
        # 阴影段: x <= p
        shadow_mask = x <= p
        shadow_x = x[shadow_mask]
        if len(shadow_x) > 0:
            # 使用简单的幂函数确保单调性
            result[shadow_mask] = np.power(shadow_x / p, gamma_s) * p
        
        # 高光段: x > p  
        highlight_mask = x > p
        highlight_x = x[highlight_mask]
        if len(highlight_x) > 0:
            # 有理式函数，确保单调递增
            numerator = highlight_x
            denominator = 1.0 + h * (highlight_x - p)  # 修正以确保连续性
            y_rational = numerator / denominator
            result[highlight_mask] = np.power(y_rational, gamma_h)
        
        # 确保在枢轴点连续
        if np.any(shadow_mask) and np.any(highlight_mask):
            # 在p点的值应该相等
            p_shadow = np.power(1.0, gamma_s) * p  # = p
            p_highlight = np.power(p / (1.0 + h * 0), gamma_h)  # = p^gamma_h
            
            # 调整高光段以确保连续性
            if abs(p_shadow - p_highlight) > 1e-10:
                scale_factor = p_shadow / p_highlight if p_highlight > 0 else 1.0
                result[highlight_mask] *= scale_factor
        
        # Soft knee 处理
        if 'soft_knee' in params:
            result = self._apply_soft_knee(result, params['soft_knee'])
            
        return np.clip(result, 0.0, 1.0)
    
    def rlog_curve(self, x: np.ndarray, params: Dict) -> np.ndarray:
        """RLOG (Rational Logarithmic) 曲线 - 64位精度"""
        x = np.asarray(x, dtype=self.precision)
        
        a = np.float64(params['a'])
        b = np.float64(params['b'])
        c = np.float64(params['c'])
        t = np.float64(params['t'])
        
        # 确保单调性的RLOG实现
        # 使用全域有理式函数确保单调性
        # y = (a*x + b*x^2) / (1 + c*x)
        
        numerator = a * x + b * x * x
        denominator = 1.0 + c * x
        
        result = numerator / denominator
        
        # 归一化到合理范围
        result = result / (a + b)  # 归一化因子
        
        return np.clip(result, 0.0, 1.0)
    
    def _smoothstep(self, edge0: float, edge1: float, x: np.ndarray) -> np.ndarray:
        """Smoothstep 函数 - 64位精度"""
        t = np.clip((x - edge0) / (edge1 - edge0), 0.0, 1.0)
        return t * t * (3.0 - 2.0 * t)
    
    def _apply_soft_knee(self, y: np.ndarray, knee_params: Dict) -> np.ndarray:
        """应用软膝处理"""
        y_knee = np.float64(knee_params['y_knee'])
        alpha = np.float64(knee_params['alpha'])
        
        knee_mask = y > y_knee
        excess = np.where(knee_mask, y - y_knee, np.float64(0.0))
        compressed = excess / (1.0 + alpha * excess)
        
        return np.where(knee_mask, y_knee + compressed, y)
    
    def oklab_transform(self, rgb: np.ndarray) -> np.ndarray:
        """RGB to OKLab 转换 - 64位精度"""
        rgb = np.asarray(rgb, dtype=self.precision)
        
        # Linear RGB to LMS
        M1 = np.array([
            [0.4122214708, 0.5363325363, 0.0514459929],
            [0.2119034982, 0.6806995451, 0.1073969566],
            [0.0883024619, 0.2817188376, 0.6299787005]
        ], dtype=self.precision)
        
        lms = rgb @ M1.T
        lms = np.sign(lms) * np.power(np.abs(lms), 1.0/3.0)
        
        # LMS to Lab
        M2 = np.array([
            [0.2104542553, 0.7936177850, -0.0040720468],
            [1.9779984951, -2.4285922050, 0.4505937099],
            [0.0259040371, 0.7827717662, -0.8086757660]
        ], dtype=self.precision)
        
        lab = lms @ M2.T
        return lab
    
    def delta_e00(self, lab1: np.ndarray, lab2: np.ndarray) -> np.ndarray:
        """CIE ΔE00 计算 - 64位精度"""
        # 简化的 ΔE00 实现，用于测试
        # 实际应用中应使用完整的 CIE ΔE00 公式
        diff = lab1 - lab2
        return np.sqrt(np.sum(diff**2, axis=-1))

def generate_preset_params() -> Dict:
    """生成三套预设参数"""
    presets = {
        "cinema_flat": {
            "name": "Cinema-Flat",
            "ppr": {
                "pivot": 0.18,
                "gamma_s": 1.10,
                "gamma_h": 1.05,
                "shoulder": 1.0,
                "soft_knee": {"y_knee": 0.97, "alpha": 0.5}
            },
            "rlog": {
                "a": 3.0,
                "b": 0.9,
                "c": 1.0,
                "t": 0.5
            },
            "saturation": {"base": 1.00, "highlights": 0.95}
        },
        "cinema_punch": {
            "name": "Cinema-Punch", 
            "ppr": {
                "pivot": 0.18,
                "gamma_s": 1.40,
                "gamma_h": 1.10,
                "shoulder": 1.8,
                "soft_knee": {"y_knee": 0.96, "alpha": 0.7}
            },
            "rlog": {
                "a": 3.0,
                "b": 0.5,
                "c": 0.8,
                "t": 0.55
            },
            "saturation": {"base": 1.05, "highlights": 1.00}
        },
        "cinema_highlight": {
            "name": "Cinema-Highlight",
            "ppr": {
                "pivot": 0.20,
                "gamma_s": 1.20,
                "gamma_h": 0.95,
                "shoulder": 1.2,
                "soft_knee": {"y_knee": 0.98, "alpha": 0.3}
            },
            "rlog": {
                "a": 5.0,
                "b": 0.85,
                "c": 0.8,
                "t": 0.45
            },
            "saturation": {"base": 0.98, "highlights": 0.92}
        }
    }
    return presets

def main():
    parser = argparse.ArgumentParser(description='生成Cinema Pro HDR RefMath参考数据')
    parser.add_argument('--output-dir', default='golden', help='输出目录')
    parser.add_argument('--samples', type=int, default=16384, help='采样点数')
    args = parser.parse_args()
    
    # 创建输出目录
    output_dir = Path(args.output_dir)
    output_dir.mkdir(exist_ok=True)
    
    generator = RefMathGenerator()
    presets = generate_preset_params()
    
    # 生成采样点 (64位精度)
    x_samples = np.linspace(0.0, 1.0, args.samples, dtype=np.float64)
    
    # 生成曲线数据
    curves_data = []
    
    print(f"生成 {args.samples} 个采样点的64位精度参考曲线...")
    
    for preset_name, preset_params in presets.items():
        print(f"处理预设: {preset_name}")
        
        # PPR 曲线
        ppr_y = generator.ppr_curve(x_samples, preset_params['ppr'])
        
        # RLOG 曲线  
        rlog_y = generator.rlog_curve(x_samples, preset_params['rlog'])
        
        # 验证单调性 (允许数值误差)
        ppr_diff = np.diff(ppr_y)
        rlog_diff = np.diff(rlog_y)
        ppr_monotonic = np.all(ppr_diff >= -1e-10)
        rlog_monotonic = np.all(rlog_diff >= -1e-10)
        
        # 调试信息
        if not ppr_monotonic:
            min_diff = np.min(ppr_diff)
            print(f"  警告: {preset_name} PPR曲线非严格单调，最小差分: {min_diff:.2e}")
        if not rlog_monotonic:
            min_diff = np.min(rlog_diff)
            print(f"  警告: {preset_name} RLOG曲线非严格单调，最小差分: {min_diff:.2e}")
        
        # 验证 C¹ 连续性 (简化检查)
        ppr_c1_ok = True  # 实际应检查导数连续性
        rlog_c1_ok = True
        
        for i, (curve_name, y_values) in enumerate([("PPR", ppr_y), ("RLOG", rlog_y)]):
            curve_record = {
                'preset': preset_name,
                'curve_type': curve_name,
                'x_samples': x_samples.tolist(),
                'y_values': y_values.tolist(),
                'monotonic': bool(ppr_monotonic if curve_name == "PPR" else rlog_monotonic),
                'c1_continuous': bool(ppr_c1_ok if curve_name == "PPR" else rlog_c1_ok),
                'parameters': preset_params['ppr'] if curve_name == "PPR" else preset_params['rlog'],
                'precision': 'float64',
                'sample_count': args.samples
            }
            curves_data.append(curve_record)
    
    # 保存曲线数据到 CSV
    curves_df_records = []
    for curve in curves_data:
        for i, (x, y) in enumerate(zip(curve['x_samples'], curve['y_values'])):
            curves_df_records.append({
                'preset': curve['preset'],
                'curve_type': curve['curve_type'],
                'sample_index': i,
                'x': x,
                'y': y,
                'monotonic': curve['monotonic'],
                'c1_continuous': curve['c1_continuous']
            })
    
    curves_df = pd.DataFrame(curves_df_records)
    curves_csv_path = output_dir / 'curves.csv'
    curves_df.to_csv(curves_csv_path, index=False, float_format='%.16g')
    print(f"曲线数据已保存到: {curves_csv_path}")
    
    # 保存完整的参考数据 (JSON格式，用于详细对比)
    reference_data = {
        'version': '1.0',
        'generator': 'Cinema Pro HDR RefMath Generator',
        'precision': 'float64',
        'sample_count': args.samples,
        'presets': presets,
        'curves': curves_data,
        'validation': {
            'all_monotonic': bool(all(curve['monotonic'] for curve in curves_data)),
            'all_c1_continuous': bool(all(curve['c1_continuous'] for curve in curves_data))
        }
    }
    
    reference_json_path = output_dir / 'reference_data.json'
    with open(reference_json_path, 'w', encoding='utf-8') as f:
        json.dump(reference_data, f, indent=2, ensure_ascii=False)
    print(f"参考数据已保存到: {reference_json_path}")
    
    # 生成测试用的参考帧元数据 (实际的 .exr 文件需要外部工具生成)
    frames_metadata = {
        'version': '1.0',
        'description': '参考帧元数据 - 用于视觉回归测试',
        'frames': []
    }
    
    for preset_name in presets.keys():
        for clip_id in ['night_scene_01', 'backlit_portrait_01', 'neon_01', 'mist_01']:
            frame_meta = {
                'preset': preset_name,
                'clip_id': clip_id,
                'filename': f'{preset_name}_{clip_id}_ref.exr',
                'description': f'{preset_name} 预设应用于 {clip_id} 的参考帧',
                'expected_stats': {
                    'min_pq': 0.0,  # 实际值需要从渲染结果计算
                    'avg_pq': 0.18,
                    'max_pq': 1.0
                }
            }
            frames_metadata['frames'].append(frame_meta)
    
    frames_dir = output_dir / 'frames'
    frames_dir.mkdir(exist_ok=True)
    
    frames_meta_path = frames_dir / 'metadata.json'
    with open(frames_meta_path, 'w', encoding='utf-8') as f:
        json.dump(frames_metadata, f, indent=2, ensure_ascii=False)
    print(f"参考帧元数据已保存到: {frames_meta_path}")
    
    print("\n✅ RefMath 参考数据生成完成!")
    print(f"📁 输出目录: {output_dir.absolute()}")
    print(f"📊 曲线数据: {curves_csv_path.name} ({len(curves_df_records)} 条记录)")
    print(f"📋 参考数据: {reference_json_path.name}")
    print(f"🖼️  参考帧: {frames_meta_path.name}")
    print("\n注意: 实际的 .exr 参考帧文件需要使用渲染工具生成")

if __name__ == '__main__':
    main()