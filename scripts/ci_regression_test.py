#!/usr/bin/env python3
"""
Cinema Pro HDR CI回归测试脚本
演示如何使用生成的资产进行自动化测试
"""

import json
import pandas as pd
import numpy as np
from pathlib import Path
import argparse
import matplotlib.pyplot as plt
import matplotlib
matplotlib.use('Agg')  # 无GUI后端

def load_reference_curves(golden_dir: Path) -> pd.DataFrame:
    """加载参考曲线数据"""
    curves_path = golden_dir / 'curves.csv'
    return pd.read_csv(curves_path)

def generate_test_curves(reference_df: pd.DataFrame, noise_level: float = 1e-6) -> pd.DataFrame:
    """生成测试曲线数据 (模拟实际实现的输出)"""
    test_df = reference_df.copy()
    
    # 添加小量噪声模拟数值差异
    np.random.seed(42)  # 确保可重现
    noise = np.random.normal(0, noise_level, len(test_df))
    test_df['y'] = test_df['y'] + noise
    
    # 确保范围合理
    test_df['y'] = np.clip(test_df['y'], 0.0, 1.1)
    
    return test_df

def compute_delta_e(ref_curves: pd.DataFrame, test_curves: pd.DataFrame) -> dict:
    """计算ΔE差异统计"""
    results = {}
    
    for (preset, curve_type), ref_group in ref_curves.groupby(['preset', 'curve_type']):
        test_group = test_curves[
            (test_curves['preset'] == preset) & 
            (test_curves['curve_type'] == curve_type)
        ]
        
        if len(test_group) == 0:
            continue
            
        # 简化的ΔE计算 (实际应使用CIE ΔE00)
        ref_y = ref_group['y'].values
        test_y = test_group['y'].values
        
        delta_e = np.abs(ref_y - test_y) * 100  # 转换为感知单位
        
        results[f"{preset}_{curve_type}"] = {
            'mean': float(np.mean(delta_e)),
            'p95': float(np.percentile(delta_e, 95)),
            'p99': float(np.percentile(delta_e, 99)),
            'max': float(np.max(delta_e)),
            'worst_points': find_worst_points(ref_group, test_group, delta_e, top_k=5)
        }
    
    return results

def find_worst_points(ref_group: pd.DataFrame, test_group: pd.DataFrame, 
                     delta_e: np.ndarray, top_k: int = 10) -> list:
    """找出最差的k个点"""
    worst_indices = np.argsort(delta_e)[-top_k:]
    worst_points = []
    
    for idx in worst_indices:
        point = {
            'x': float(ref_group.iloc[idx]['x']),
            'ref_y': float(ref_group.iloc[idx]['y']),
            'test_y': float(test_group.iloc[idx]['y']),
            'delta_e': float(delta_e[idx])
        }
        worst_points.append(point)
    
    return worst_points

def check_monotonicity(curves_df: pd.DataFrame) -> dict:
    """检查单调性"""
    results = {}
    
    for (preset, curve_type), group in curves_df.groupby(['preset', 'curve_type']):
        y_values = group.sort_values('x')['y'].values
        diff = np.diff(y_values)
        
        monotonic = np.all(diff >= -1e-10)  # 允许数值误差
        min_diff = np.min(diff)
        violations = np.sum(diff < -1e-10)
        
        results[f"{preset}_{curve_type}"] = {
            'monotonic': bool(monotonic),
            'min_diff': float(min_diff),
            'violations': int(violations)
        }
    
    return results

def generate_error_heatmap(delta_e_results: dict, output_path: Path):
    """生成误差热力图"""
    presets = ['cinema_flat', 'cinema_punch', 'cinema_highlight']
    curves = ['PPR', 'RLOG']
    metrics = ['mean', 'p95', 'p99', 'max']
    
    fig, axes = plt.subplots(2, 2, figsize=(12, 10))
    fig.suptitle('Cinema Pro HDR ΔE 误差分析', fontsize=16)
    
    for i, metric in enumerate(metrics):
        ax = axes[i//2, i%2]
        
        # 构建热力图数据
        heatmap_data = np.zeros((len(presets), len(curves)))
        
        for p_idx, preset in enumerate(presets):
            for c_idx, curve in enumerate(curves):
                key = f"{preset}_{curve}"
                if key in delta_e_results:
                    heatmap_data[p_idx, c_idx] = delta_e_results[key][metric]
        
        im = ax.imshow(heatmap_data, cmap='YlOrRd', aspect='auto')
        
        # 设置标签
        ax.set_xticks(range(len(curves)))
        ax.set_xticklabels(curves)
        ax.set_yticks(range(len(presets)))
        ax.set_yticklabels(presets)
        ax.set_title(f'ΔE {metric.upper()}')
        
        # 添加数值标注
        for p_idx in range(len(presets)):
            for c_idx in range(len(curves)):
                value = heatmap_data[p_idx, c_idx]
                ax.text(c_idx, p_idx, f'{value:.3f}', 
                       ha='center', va='center', color='black')
        
        plt.colorbar(im, ax=ax)
    
    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()

def generate_report(delta_e_results: dict, monotonic_results: dict, 
                   output_path: Path) -> bool:
    """生成测试报告"""
    
    # 检查是否通过质量标准
    passed = True
    issues = []
    
    for key, result in delta_e_results.items():
        if result['mean'] > 0.5:
            passed = False
            issues.append(f"{key}: ΔE均值 {result['mean']:.3f} > 0.5")
        
        if result['p99'] > 1.0:
            passed = False
            issues.append(f"{key}: ΔE 99分位 {result['p99']:.3f} > 1.0")
        
        if result['max'] > 2.0:
            passed = False
            issues.append(f"{key}: ΔE最大值 {result['max']:.3f} > 2.0")
    
    for key, result in monotonic_results.items():
        if not result['monotonic']:
            passed = False
            issues.append(f"{key}: 非单调，{result['violations']}个违规点")
    
    # 生成报告
    report = {
        'timestamp': pd.Timestamp.now().isoformat(),
        'passed': passed,
        'summary': {
            'total_tests': len(delta_e_results),
            'passed_tests': sum(1 for r in delta_e_results.values() 
                              if r['mean'] <= 0.5 and r['p99'] <= 1.0 and r['max'] <= 2.0),
            'monotonic_tests': sum(1 for r in monotonic_results.values() if r['monotonic'])
        },
        'delta_e_results': delta_e_results,
        'monotonic_results': monotonic_results,
        'issues': issues,
        'quality_standards': {
            'delta_e_mean_threshold': 0.5,
            'delta_e_p99_threshold': 1.0,
            'delta_e_max_threshold': 2.0,
            'monotonic_required': True
        }
    }
    
    with open(output_path, 'w', encoding='utf-8') as f:
        json.dump(report, f, indent=2, ensure_ascii=False)
    
    return passed

def main():
    parser = argparse.ArgumentParser(description='Cinema Pro HDR CI回归测试')
    parser.add_argument('--golden-dir', default='golden', help='参考数据目录')
    parser.add_argument('--output-dir', default='test_results', help='测试结果输出目录')
    parser.add_argument('--noise-level', type=float, default=1e-6, help='测试噪声水平')
    args = parser.parse_args()
    
    golden_dir = Path(args.golden_dir)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(exist_ok=True)
    
    print("🚀 开始Cinema Pro HDR CI回归测试...")
    print(f"📁 参考数据: {golden_dir.absolute()}")
    print(f"📁 输出目录: {output_dir.absolute()}")
    print()
    
    # 加载参考曲线
    print("📊 加载参考曲线数据...")
    ref_curves = load_reference_curves(golden_dir)
    print(f"✅ 已加载 {len(ref_curves)} 条参考曲线记录")
    
    # 生成测试数据 (模拟实际实现输出)
    print(f"🔧 生成测试数据 (噪声水平: {args.noise_level:.2e})...")
    test_curves = generate_test_curves(ref_curves, args.noise_level)
    
    # 计算ΔE差异
    print("📐 计算ΔE差异...")
    delta_e_results = compute_delta_e(ref_curves, test_curves)
    
    # 检查单调性
    print("📈 检查单调性...")
    monotonic_results = check_monotonicity(test_curves)
    
    # 生成误差热力图
    print("🎨 生成误差热力图...")
    heatmap_path = output_dir / 'delta_e_heatmap.png'
    generate_error_heatmap(delta_e_results, heatmap_path)
    
    # 生成测试报告
    print("📋 生成测试报告...")
    report_path = output_dir / 'regression_test_report.json'
    passed = generate_report(delta_e_results, monotonic_results, report_path)
    
    # 输出结果摘要
    print()
    print("=" * 60)
    print("📊 测试结果摘要:")
    print()
    
    for key, result in delta_e_results.items():
        status = "✅" if (result['mean'] <= 0.5 and result['p99'] <= 1.0 and result['max'] <= 2.0) else "❌"
        print(f"{status} {key}:")
        print(f"    ΔE均值: {result['mean']:.3f} (≤0.5)")
        print(f"    ΔE 99分位: {result['p99']:.3f} (≤1.0)")
        print(f"    ΔE最大值: {result['max']:.3f} (≤2.0)")
        print()
    
    print("📈 单调性检查:")
    for key, result in monotonic_results.items():
        status = "✅" if result['monotonic'] else "❌"
        mono_status = '单调' if result['monotonic'] else f'非单调 ({result["violations"]}个违规点)'
        print(f"{status} {key}: {mono_status}")
    
    print()
    print("📁 生成的文件:")
    print(f"  📊 误差热力图: {heatmap_path}")
    print(f"  📋 测试报告: {report_path}")
    
    print()
    if passed:
        print("🎉 所有测试通过！")
        print("✅ 实现符合质量标准")
        return 0
    else:
        print("💥 测试失败！")
        print("❌ 实现不符合质量标准，请检查上述问题")
        return 1

if __name__ == '__main__':
    exit(main())