#!/usr/bin/env python3
"""
Cinema Pro HDR CI回归测试脚本

用于对比GPU/OFX/DCTL输出与RefMath参考数据的差异。
生成误差栅格图和top-10 worst points，一键定位数值分歧。

功能：
1. 加载RefMath参考数据
2. 对比实际实现输出
3. 生成误差分析报告
4. 输出可视化误差图表
"""

import numpy as np
import json
import csv
import argparse
import matplotlib.pyplot as plt
import matplotlib.patches as patches
from pathlib import Path
from typing import Dict, List, Tuple, Optional
from dataclasses import dataclass
import seaborn as sns

@dataclass
class ErrorAnalysis:
    """误差分析结果"""
    max_error: float
    mean_error: float
    rms_error: float
    percentile_95: float
    percentile_99: float
    worst_points: List[Tuple[int, float, float, float]]  # (index, input, expected, actual)

@dataclass
class ComparisonResult:
    """对比结果"""
    test_name: str
    passed: bool
    error_analysis: ErrorAnalysis
    tolerance: float
    message: str

class RefMathLoader:
    """RefMath参考数据加载器"""
    
    def __init__(self, golden_dir: str = "golden"):
        self.golden_dir = Path(golden_dir)
    
    def load_curves_csv(self) -> Dict:
        """加载曲线CSV数据"""
        csv_path = self.golden_dir / "curves.csv"
        if not csv_path.exists():
            raise FileNotFoundError(f"找不到参考曲线文件: {csv_path}")
        
        curves_data = {}
        
        with open(csv_path, 'r', encoding='utf-8') as f:
            reader = csv.DictReader(f)
            
            # 初始化数据结构
            for preset in ["Cinema-Flat", "Cinema-Punch", "Cinema-Highlight"]:
                curves_data[preset] = {
                    "x": [],
                    "ppr_y": [],
                    "rlog_y": []
                }
            
            # 读取数据
            for row in reader:
                x = float(row["x"])
                
                for preset in ["Cinema-Flat", "Cinema-Punch", "Cinema-Highlight"]:
                    curves_data[preset]["x"].append(x)
                    curves_data[preset]["ppr_y"].append(float(row[f"{preset}_PPR"]))
                    curves_data[preset]["rlog_y"].append(float(row[f"{preset}_RLOG"]))
        
        return curves_data
    
    def load_reference_json(self) -> Dict:
        """加载完整参考数据"""
        json_path = self.golden_dir / "reference_data.json"
        if not json_path.exists():
            raise FileNotFoundError(f"找不到参考数据文件: {json_path}")
        
        with open(json_path, 'r', encoding='utf-8') as f:
            return json.load(f)

class ErrorAnalyzer:
    """误差分析器"""
    
    @staticmethod
    def analyze_errors(expected: np.ndarray, actual: np.ndarray, 
                      input_values: Optional[np.ndarray] = None) -> ErrorAnalysis:
        """分析误差统计"""
        
        expected = np.asarray(expected, dtype=np.float64)
        actual = np.asarray(actual, dtype=np.float64)
        
        if expected.shape != actual.shape:
            raise ValueError(f"数组形状不匹配: {expected.shape} vs {actual.shape}")
        
        # 计算绝对误差
        abs_errors = np.abs(expected - actual)
        
        # 基本统计
        max_error = float(np.max(abs_errors))
        mean_error = float(np.mean(abs_errors))
        rms_error = float(np.sqrt(np.mean(abs_errors**2)))
        percentile_95 = float(np.percentile(abs_errors, 95))
        percentile_99 = float(np.percentile(abs_errors, 99))
        
        # 找出最差的10个点
        worst_indices = np.argsort(abs_errors)[-10:][::-1]  # 降序
        worst_points = []
        
        for idx in worst_indices:
            input_val = input_values[idx] if input_values is not None else idx
            worst_points.append((
                int(idx),
                float(input_val),
                float(expected[idx]),
                float(actual[idx])
            ))
        
        return ErrorAnalysis(
            max_error=max_error,
            mean_error=mean_error,
            rms_error=rms_error,
            percentile_95=percentile_95,
            percentile_99=percentile_99,
            worst_points=worst_points
        )
    
    @staticmethod
    def check_tolerance(error_analysis: ErrorAnalysis, tolerance: float, 
                       strict_mode: bool = False) -> bool:
        """检查是否在容差范围内"""
        
        if strict_mode:
            # 严格模式：最大误差必须在容差内
            return error_analysis.max_error <= tolerance
        else:
            # 宽松模式：99分位数在容差内即可
            return error_analysis.percentile_99 <= tolerance

class CurveComparator:
    """曲线对比器"""
    
    def __init__(self, golden_dir: str = "golden"):
        self.loader = RefMathLoader(golden_dir)
        self.reference_curves = self.loader.load_curves_csv()
    
    def compare_curve_implementation(self, test_data: Dict, 
                                   tolerance: float = 1e-6,
                                   strict_mode: bool = False) -> List[ComparisonResult]:
        """
        对比曲线实现
        
        Args:
            test_data: 测试数据，格式与reference_curves相同
            tolerance: 允许的最大误差
            strict_mode: 是否使用严格模式
        """
        
        results = []
        
        for preset_name in ["Cinema-Flat", "Cinema-Punch", "Cinema-Highlight"]:
            for curve_type in ["ppr_y", "rlog_y"]:
                test_name = f"{preset_name}_{curve_type.upper()}"
                
                # 获取参考数据和测试数据
                ref_data = np.array(self.reference_curves[preset_name][curve_type])
                test_curve_data = test_data.get(preset_name, {})
                actual_data = np.array(test_curve_data.get(curve_type, []))
                
                if len(actual_data) == 0:
                    results.append(ComparisonResult(
                        test_name=test_name,
                        passed=False,
                        error_analysis=None,
                        tolerance=tolerance,
                        message=f"缺少测试数据: {test_name}"
                    ))
                    continue
                
                if len(ref_data) != len(actual_data):
                    results.append(ComparisonResult(
                        test_name=test_name,
                        passed=False,
                        error_analysis=None,
                        tolerance=tolerance,
                        message=f"数据长度不匹配: 期望{len(ref_data)}, 实际{len(actual_data)}"
                    ))
                    continue
                
                # 分析误差
                input_values = np.array(self.reference_curves[preset_name]["x"])
                error_analysis = ErrorAnalyzer.analyze_errors(
                    ref_data, actual_data, input_values
                )
                
                # 检查容差
                passed = ErrorAnalyzer.check_tolerance(error_analysis, tolerance, strict_mode)
                
                message = f"最大误差: {error_analysis.max_error:.2e}, 容差: {tolerance:.2e}"
                if not passed:
                    message += f" [FAIL]"
                
                results.append(ComparisonResult(
                    test_name=test_name,
                    passed=passed,
                    error_analysis=error_analysis,
                    tolerance=tolerance,
                    message=message
                ))
        
        return results
    
    def generate_error_heatmap(self, test_data: Dict, output_path: str):
        """生成误差热力图"""
        
        fig, axes = plt.subplots(2, 3, figsize=(15, 10))
        fig.suptitle('Cinema Pro HDR 曲线误差热力图', fontsize=16)
        
        presets = ["Cinema-Flat", "Cinema-Punch", "Cinema-Highlight"]
        curve_types = ["ppr_y", "rlog_y"]
        
        for i, curve_type in enumerate(curve_types):
            for j, preset in enumerate(presets):
                ax = axes[i, j]
                
                # 获取数据
                ref_data = np.array(self.reference_curves[preset][curve_type])
                test_curve_data = test_data.get(preset, {})
                actual_data = np.array(test_curve_data.get(curve_type, []))
                
                if len(actual_data) == 0 or len(ref_data) != len(actual_data):
                    ax.text(0.5, 0.5, '数据缺失', ha='center', va='center', 
                           transform=ax.transAxes, fontsize=12)
                    ax.set_title(f"{preset} {curve_type.upper()}")
                    continue
                
                # 计算误差
                errors = np.abs(ref_data - actual_data)
                
                # 创建2D误差图 (将1D误差映射到2D网格)
                grid_size = int(np.sqrt(len(errors)))
                if grid_size * grid_size < len(errors):
                    grid_size += 1
                
                error_grid = np.zeros((grid_size, grid_size))
                for k, error in enumerate(errors):
                    row = k // grid_size
                    col = k % grid_size
                    if row < grid_size and col < grid_size:
                        error_grid[row, col] = error
                
                # 绘制热力图
                im = ax.imshow(error_grid, cmap='hot', aspect='auto')
                ax.set_title(f"{preset} {curve_type.upper()}\n最大误差: {np.max(errors):.2e}")
                
                # 添加颜色条
                plt.colorbar(im, ax=ax, fraction=0.046, pad=0.04)
        
        plt.tight_layout()
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        plt.close()
        
        print(f"误差热力图已保存到: {output_path}")

class ColorSpaceComparator:
    """色彩空间转换对比器"""
    
    def __init__(self, golden_dir: str = "golden"):
        self.loader = RefMathLoader(golden_dir)
        self.reference_data = self.loader.load_reference_json()
    
    def compare_color_transforms(self, test_transforms: Dict,
                               tolerance: float = 1e-6) -> List[ComparisonResult]:
        """对比色彩空间转换"""
        
        results = []
        ref_color_data = self.reference_data['color_spaces']
        
        # 对比各种转换
        comparisons = [
            ("bt2020_to_p3d65", "bt2020_samples", "p3d65_converted"),
            ("p3d65_to_bt2020", "p3d65_converted", "bt2020_roundtrip"),
            ("pq_eotf", "pq_samples", "linear_converted"),
            ("pq_oetf", "linear_converted", "pq_roundtrip"),
            ("rgb_to_oklab", "oklab_samples", "oklab_converted"),
            ("oklab_to_rgb", "oklab_converted", "rgb_from_oklab")
        ]
        
        for transform_name, input_key, output_key in comparisons:
            if transform_name not in test_transforms:
                results.append(ComparisonResult(
                    test_name=transform_name,
                    passed=False,
                    error_analysis=None,
                    tolerance=tolerance,
                    message=f"缺少测试数据: {transform_name}"
                ))
                continue
            
            # 获取参考数据和测试数据
            ref_output = np.array(ref_color_data[output_key])
            test_output = np.array(test_transforms[transform_name])
            
            if ref_output.shape != test_output.shape:
                results.append(ComparisonResult(
                    test_name=transform_name,
                    passed=False,
                    error_analysis=None,
                    tolerance=tolerance,
                    message=f"形状不匹配: 期望{ref_output.shape}, 实际{test_output.shape}"
                ))
                continue
            
            # 分析误差 (展平为1D数组)
            error_analysis = ErrorAnalyzer.analyze_errors(
                ref_output.flatten(), test_output.flatten()
            )
            
            # 检查容差
            passed = ErrorAnalyzer.check_tolerance(error_analysis, tolerance)
            
            message = f"最大误差: {error_analysis.max_error:.2e}, RMS: {error_analysis.rms_error:.2e}"
            if not passed:
                message += f" [FAIL]"
            
            results.append(ComparisonResult(
                test_name=transform_name,
                passed=passed,
                error_analysis=error_analysis,
                tolerance=tolerance,
                message=message
            ))
        
        return results

class ReportGenerator:
    """报告生成器"""
    
    @staticmethod
    def generate_text_report(results: List[ComparisonResult], output_path: str):
        """生成文本报告"""
        
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write("# Cinema Pro HDR CI回归测试报告\n\n")
            f.write(f"测试时间: {np.datetime64('now')}\n\n")
            
            # 统计
            total_tests = len(results)
            passed_tests = sum(1 for r in results if r.passed)
            failed_tests = total_tests - passed_tests
            
            f.write(f"## 测试概览\n\n")
            f.write(f"- 总测试数: {total_tests}\n")
            f.write(f"- 通过: {passed_tests}\n")
            f.write(f"- 失败: {failed_tests}\n")
            f.write(f"- 通过率: {passed_tests/total_tests*100:.1f}%\n\n")
            
            # 详细结果
            f.write(f"## 详细结果\n\n")
            
            for result in results:
                status = "✓ PASS" if result.passed else "✗ FAIL"
                f.write(f"### {result.test_name} - {status}\n\n")
                f.write(f"- 容差: {result.tolerance:.2e}\n")
                f.write(f"- 消息: {result.message}\n")
                
                if result.error_analysis:
                    ea = result.error_analysis
                    f.write(f"- 最大误差: {ea.max_error:.2e}\n")
                    f.write(f"- 平均误差: {ea.mean_error:.2e}\n")
                    f.write(f"- RMS误差: {ea.rms_error:.2e}\n")
                    f.write(f"- 95分位数: {ea.percentile_95:.2e}\n")
                    f.write(f"- 99分位数: {ea.percentile_99:.2e}\n")
                    
                    if ea.worst_points:
                        f.write(f"\n**Top-5 最差点:**\n")
                        for i, (idx, input_val, expected, actual) in enumerate(ea.worst_points[:5]):
                            error = abs(expected - actual)
                            f.write(f"{i+1}. 索引{idx}: 输入={input_val:.6f}, 期望={expected:.6f}, 实际={actual:.6f}, 误差={error:.2e}\n")
                
                f.write("\n")
        
        print(f"测试报告已保存到: {output_path}")
    
    @staticmethod
    def generate_json_report(results: List[ComparisonResult], output_path: str):
        """生成JSON格式报告"""
        
        report_data = {
            "timestamp": str(np.datetime64('now')),
            "summary": {
                "total_tests": len(results),
                "passed_tests": sum(1 for r in results if r.passed),
                "failed_tests": sum(1 for r in results if not r.passed),
                "pass_rate": sum(1 for r in results if r.passed) / len(results) * 100
            },
            "results": []
        }
        
        for result in results:
            result_data = {
                "test_name": result.test_name,
                "passed": result.passed,
                "tolerance": result.tolerance,
                "message": result.message
            }
            
            if result.error_analysis:
                ea = result.error_analysis
                result_data["error_analysis"] = {
                    "max_error": ea.max_error,
                    "mean_error": ea.mean_error,
                    "rms_error": ea.rms_error,
                    "percentile_95": ea.percentile_95,
                    "percentile_99": ea.percentile_99,
                    "worst_points": [
                        {
                            "index": idx,
                            "input": input_val,
                            "expected": expected,
                            "actual": actual,
                            "error": abs(expected - actual)
                        }
                        for idx, input_val, expected, actual in ea.worst_points
                    ]
                }
            
            report_data["results"].append(result_data)
        
        with open(output_path, 'w', encoding='utf-8') as f:
            json.dump(report_data, f, indent=2, ensure_ascii=False)
        
        print(f"JSON报告已保存到: {output_path}")

def load_test_data_example() -> Dict:
    """
    加载测试数据示例
    
    在实际使用中，这里应该调用GPU/OFX/DCTL实现来生成测试数据
    """
    
    # 这里是示例数据，实际应该从GPU/OFX/DCTL获取
    loader = RefMathLoader()
    reference_curves = loader.load_curves_csv()
    
    # 模拟一些小的数值误差
    test_data = {}
    for preset_name in ["Cinema-Flat", "Cinema-Punch", "Cinema-Highlight"]:
        test_data[preset_name] = {}
        for curve_type in ["ppr_y", "rlog_y"]:
            ref_values = np.array(reference_curves[preset_name][curve_type])
            # 添加小的随机误差来模拟实现差异
            noise = np.random.normal(0, 1e-7, len(ref_values))
            test_data[preset_name][curve_type] = (ref_values + noise).tolist()
    
    return test_data

def main():
    """主函数"""
    parser = argparse.ArgumentParser(description="Cinema Pro HDR CI回归测试")
    parser.add_argument("--golden-dir", default="golden", help="RefMath数据目录")
    parser.add_argument("--test-data", help="测试数据文件 (JSON格式)")
    parser.add_argument("--tolerance", type=float, default=1e-6, help="允许的最大误差")
    parser.add_argument("--strict", action="store_true", help="使用严格模式")
    parser.add_argument("--output-dir", default="test_results", help="输出目录")
    parser.add_argument("--generate-plots", action="store_true", help="生成可视化图表")
    
    args = parser.parse_args()
    
    # 创建输出目录
    output_dir = Path(args.output_dir)
    output_dir.mkdir(exist_ok=True)
    
    print("开始CI回归测试...")
    
    # 加载测试数据
    if args.test_data:
        with open(args.test_data, 'r', encoding='utf-8') as f:
            test_data = json.load(f)
    else:
        print("使用示例测试数据 (实际使用时应提供真实测试数据)")
        test_data = load_test_data_example()
    
    # 对比曲线实现
    print("对比曲线实现...")
    curve_comparator = CurveComparator(args.golden_dir)
    curve_results = curve_comparator.compare_curve_implementation(
        test_data, args.tolerance, args.strict
    )
    
    # 生成误差热力图
    if args.generate_plots:
        heatmap_path = output_dir / "error_heatmap.png"
        curve_comparator.generate_error_heatmap(test_data, str(heatmap_path))
    
    # 生成报告
    print("生成测试报告...")
    text_report_path = output_dir / "regression_test_report.md"
    json_report_path = output_dir / "regression_test_report.json"
    
    ReportGenerator.generate_text_report(curve_results, str(text_report_path))
    ReportGenerator.generate_json_report(curve_results, str(json_report_path))
    
    # 输出总结
    total_tests = len(curve_results)
    passed_tests = sum(1 for r in curve_results if r.passed)
    
    print(f"\n测试完成!")
    print(f"总测试数: {total_tests}")
    print(f"通过: {passed_tests}")
    print(f"失败: {total_tests - passed_tests}")
    print(f"通过率: {passed_tests/total_tests*100:.1f}%")
    
    # 如果有失败的测试，返回非零退出码
    if passed_tests < total_tests:
        print("\n有测试失败，请检查报告!")
        exit(1)
    else:
        print("\n所有测试通过!")
        exit(0)

if __name__ == "__main__":
    main()