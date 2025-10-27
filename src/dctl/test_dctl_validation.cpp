/**
 * Cinema Pro HDR DCTL 验证测试工具
 * 
 * 用途：验证DCTL实现的数值精度和性能
 * 功能：单调性测试、C¹连续性测试、性能基准测试
 */

#include "parameter_mapping.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <random>
#include <cassert>

using namespace CinemaProHDR::DCTLMapping;

namespace CinemaProHDR {
namespace DCTLValidation {

/**
 * DCTL算法的C++实现（用于验证）
 * 这里复制DCTL中的核心算法逻辑
 */
class DCTLValidator {
private:
    DCTLPresetParams params_;
    
    // 复制DCTL中的数学函数
    float SafePow(float base, float exponent) const {
        if (base <= 0.0f) return 0.0f;
        if (!std::isfinite(base) || !std::isfinite(exponent)) return 0.0f;
        
        float result = std::pow(base, exponent);
        return std::isfinite(result) ? result : 0.0f;
    }
    
    float SmoothStep(float edge0, float edge1, float x) const {
        if (edge1 <= edge0) return x >= edge1 ? 1.0f : 0.0f;
        
        float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
    
    float Mix(float a, float b, float t) const {
        t = std::clamp(t, 0.0f, 1.0f);
        return a * (1.0f - t) + b * t;
    }
    
    // PPR算法实现
    float ApplyPPR(float x) const {
        x = std::clamp(x, 0.0f, 1.0f);
        
        // 阴影段
        float normalized_shadow = x / params_.pivot_pq;
        float shadow_power = SafePow(normalized_shadow, params_.gamma_s);
        float shadow_value = shadow_power * params_.pivot_pq;
        
        // 高光段
        float normalized_highlight = (x - params_.pivot_pq) / (1.0f - params_.pivot_pq);
        normalized_highlight = std::clamp(normalized_highlight, 0.0f, 1.0f);
        
        float rational_denom = 1.0f + params_.shoulder_h * normalized_highlight;
        if (rational_denom <= 0.0f) rational_denom = 1e-8f;
        
        float rational_part = normalized_highlight / rational_denom;
        float highlight_power = SafePow(rational_part, params_.gamma_h);
        float highlight_value = params_.pivot_pq + highlight_power * (1.0f - params_.pivot_pq);
        
        // 混合
        float blend_range = params_.pivot_pq * 0.1f;
        float blend_start = params_.pivot_pq - blend_range;
        float blend_end = params_.pivot_pq + blend_range;
        float blend_weight = SmoothStep(blend_start, blend_end, x);
        
        return Mix(shadow_value, highlight_value, blend_weight);
    }
    
    // RLOG算法实现
    float ApplyRLOG(float x) const {
        x = std::clamp(x, 0.0f, 1.0f);
        
        const float blend_range = 0.05f;
        
        // 暗部段
        float log_numerator = std::log(1.0f + params_.rlog_a * x);
        float log_denominator = std::log(1.0f + params_.rlog_a);
        float dark_value = (log_denominator > 0.0f) ? (log_numerator / log_denominator) : x;
        
        // 高光段
        float highlight_denom = 1.0f + params_.rlog_c * x;
        if (highlight_denom <= 0.0f) highlight_denom = 1e-8f;
        float highlight_value = (params_.rlog_b * x) / highlight_denom;
        
        // 连续性调整
        float dark_at_t = std::log(1.0f + params_.rlog_a * params_.rlog_t) / log_denominator;
        float highlight_at_t = (params_.rlog_b * params_.rlog_t) / (1.0f + params_.rlog_c * params_.rlog_t);
        float scale_factor = (highlight_at_t > 0.0f) ? (dark_at_t / highlight_at_t) : 1.0f;
        highlight_value *= scale_factor;
        
        // 拼接
        float blend_start = params_.rlog_t - blend_range;
        float blend_end = params_.rlog_t + blend_range;
        float blend_weight = SmoothStep(blend_start, blend_end, x);
        
        return Mix(dark_value, highlight_value, blend_weight);
    }
    
    // 软膝处理
    float ApplySoftKnee(float y) const {
        if (y <= params_.yknee) return y;
        
        float excess = y - params_.yknee;
        float max_excess = 1.0f - params_.yknee;
        if (max_excess <= 0.0f) return params_.yknee;
        
        float normalized_excess = excess / max_excess;
        float compressed_excess = normalized_excess / (1.0f + params_.alpha * normalized_excess);
        
        return params_.yknee + compressed_excess * max_excess;
    }
    
    // Toe夹持
    float ApplyToeClamp(float y) const {
        if (params_.toe <= 0.0f || y <= 0.0f) return y;
        return std::max(y, params_.toe);
    }
    
public:
    DCTLValidator(const DCTLPresetParams& params) : params_(params) {}
    
    /**
     * 主变换函数（模拟DCTL的transform函数）
     */
    float Transform(float x) const {
        x = std::clamp(x, 0.0f, 1.0f);
        
        // 色调映射
        float mapped_luminance;
        if (params_.curve_type == 0) {
            mapped_luminance = ApplyPPR(x);
        } else {
            mapped_luminance = ApplyRLOG(x);
        }
        
        // 软膝处理
        mapped_luminance = ApplySoftKnee(mapped_luminance);
        
        // Toe夹持
        mapped_luminance = ApplyToeClamp(mapped_luminance);
        
        return std::clamp(mapped_luminance, 0.0f, 1.0f);
    }
    
    /**
     * 验证单调性
     */
    bool ValidateMonotonicity(int sample_count = 4096) const {
        float prev_value = -1.0f;
        
        for (int i = 0; i < sample_count; ++i) {
            float x = static_cast<float>(i) / static_cast<float>(sample_count - 1);
            float current_value = Transform(x);
            
            if (current_value < prev_value) {
                std::cout << "单调性失败: x=" << x << ", prev=" << prev_value 
                         << ", current=" << current_value << std::endl;
                return false;
            }
            
            prev_value = current_value;
        }
        
        return true;
    }
    
    /**
     * 验证C¹连续性
     */
    bool ValidateC1Continuity(float epsilon = 1e-3f, float threshold = 1e-3f) const {
        const int sample_count = 100;
        float max_derivative_gap = 0.0f;
        
        for (int i = 1; i < sample_count - 1; ++i) {
            float x = static_cast<float>(i) / static_cast<float>(sample_count - 1);
            
            if (x <= epsilon || x >= 1.0f - epsilon) {
                continue;
            }
            
            // 计算左导数和右导数
            float x_left = x - epsilon;
            float x_right = x + epsilon;
            
            float y_left = Transform(x_left);
            float y_center = Transform(x);
            float y_right = Transform(x_right);
            
            float left_derivative = (y_center - y_left) / epsilon;
            float right_derivative = (y_right - y_center) / epsilon;
            
            float derivative_gap = std::abs(right_derivative - left_derivative);
            max_derivative_gap = std::max(max_derivative_gap, derivative_gap);
        }
        
        std::cout << "最大导数间隙: " << max_derivative_gap << " (阈值: " << threshold << ")" << std::endl;
        return max_derivative_gap <= threshold;
    }
    
    /**
     * 性能基准测试
     */
    double BenchmarkPerformance(int iterations = 1000000) const {
        std::vector<float> test_values(iterations);
        
        // 生成测试数据
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(0.0f, 1.0f);
        
        for (int i = 0; i < iterations; ++i) {
            test_values[i] = dis(gen);
        }
        
        // 性能测试
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            volatile float result = Transform(test_values[i]);
            (void)result; // 防止编译器优化
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        double total_time_ms = duration.count() / 1000.0;
        double time_per_sample_us = duration.count() / static_cast<double>(iterations);
        
        std::cout << "性能测试结果:" << std::endl;
        std::cout << "  总时间: " << total_time_ms << " ms" << std::endl;
        std::cout << "  每样本时间: " << time_per_sample_us << " μs" << std::endl;
        std::cout << "  吞吐量: " << (iterations / total_time_ms * 1000.0) << " 样本/秒" << std::endl;
        
        return time_per_sample_us;
    }
    
    /**
     * 生成曲线数据用于可视化
     */
    std::vector<std::pair<float, float>> GenerateCurveData(int sample_count = 1000) const {
        std::vector<std::pair<float, float>> curve_data;
        curve_data.reserve(sample_count);
        
        for (int i = 0; i < sample_count; ++i) {
            float x = static_cast<float>(i) / static_cast<float>(sample_count - 1);
            float y = Transform(x);
            curve_data.emplace_back(x, y);
        }
        
        return curve_data;
    }
};

/**
 * 测试套件类
 */
class DCTLTestSuite {
public:
    /**
     * 运行所有测试
     */
    static bool RunAllTests() {
        std::cout << "=== Cinema Pro HDR DCTL 验证测试 ===" << std::endl;
        
        bool all_passed = true;
        
        // 测试所有预设
        std::vector<std::pair<std::string, DCTLPresetParams>> presets = {
            {"Cinema-Flat", GetCinemaFlatPreset()},
            {"Cinema-Punch", GetCinemaPunchPreset()},
            {"Cinema-Highlight", GetCinemaHighlightPreset()}
        };
        
        for (const auto& preset : presets) {
            std::cout << "\n--- 测试预设: " << preset.first << " ---" << std::endl;
            
            DCTLValidator validator(preset.second);
            
            // 参数验证
            if (!TestParameterValidation(preset.second)) {
                std::cout << "❌ 参数验证失败" << std::endl;
                all_passed = false;
            } else {
                std::cout << "✅ 参数验证通过" << std::endl;
            }
            
            // 单调性测试
            if (!validator.ValidateMonotonicity()) {
                std::cout << "❌ 单调性测试失败" << std::endl;
                all_passed = false;
            } else {
                std::cout << "✅ 单调性测试通过" << std::endl;
            }
            
            // C¹连续性测试
            if (!validator.ValidateC1Continuity()) {
                std::cout << "❌ C¹连续性测试失败" << std::endl;
                all_passed = false;
            } else {
                std::cout << "✅ C¹连续性测试通过" << std::endl;
            }
            
            // 性能测试
            double perf_us = validator.BenchmarkPerformance(100000);
            if (perf_us > 10.0) { // 10μs阈值（保守估计）
                std::cout << "⚠️  性能可能需要优化 (>" << perf_us << "μs/样本)" << std::endl;
            } else {
                std::cout << "✅ 性能测试通过 (" << perf_us << "μs/样本)" << std::endl;
            }
        }
        
        // 参数映射测试
        std::cout << "\n--- 参数映射测试 ---" << std::endl;
        if (!TestParameterMapping()) {
            std::cout << "❌ 参数映射测试失败" << std::endl;
            all_passed = false;
        } else {
            std::cout << "✅ 参数映射测试通过" << std::endl;
        }
        
        // 统计收集测试
        std::cout << "\n--- 统计收集测试 ---" << std::endl;
        if (!TestStatisticsCollection()) {
            std::cout << "❌ 统计收集测试失败" << std::endl;
            all_passed = false;
        } else {
            std::cout << "✅ 统计收集测试通过" << std::endl;
        }
        
        std::cout << "\n=== 测试总结 ===" << std::endl;
        if (all_passed) {
            std::cout << "🎉 所有测试通过！" << std::endl;
        } else {
            std::cout << "❌ 部分测试失败，请检查实现" << std::endl;
        }
        
        return all_passed;
    }
    
private:
    /**
     * 测试参数验证
     */
    static bool TestParameterValidation(const DCTLPresetParams& params) {
        // 测试参数有效性
        if (!AreParamsValid(params)) {
            return false;
        }
        
        // 测试参数钳制
        DCTLPresetParams invalid_params = params;
        invalid_params.pivot_pq = -1.0f; // 无效值
        invalid_params.gamma_s = 10.0f;  // 超出范围
        
        DCTLPresetParams clamped = ValidateAndClampParams(invalid_params);
        
        if (clamped.pivot_pq < 0.05f || clamped.pivot_pq > 0.30f) {
            return false;
        }
        
        if (clamped.gamma_s < 1.0f || clamped.gamma_s > 1.6f) {
            return false;
        }
        
        return true;
    }
    
    /**
     * 测试参数映射
     */
    static bool TestParameterMapping() {
        // 测试PPR映射
        float gamma_s = MapShadowsContrast(0.5f);
        if (std::abs(gamma_s - 1.3f) > 1e-6f) {
            std::cout << "PPR Shadows映射错误: 期望1.3, 实际" << gamma_s << std::endl;
            return false;
        }
        
        float gamma_h = MapHighlightContrast(0.5f);
        if (std::abs(gamma_h - 1.1f) > 1e-6f) {
            std::cout << "PPR Highlights映射错误: 期望1.1, 实际" << gamma_h << std::endl;
            return false;
        }
        
        // 测试RLOG映射
        float rlog_a = MapRLOGShadowLift(0.5f);
        if (std::abs(rlog_a - 8.5f) > 1e-6f) {
            std::cout << "RLOG Shadow映射错误: 期望8.5, 实际" << rlog_a << std::endl;
            return false;
        }
        
        // 测试PQ映射
        float pq_value = MapPivotNitsToPQ(180.0f);
        if (pq_value < 0.05f || pq_value > 0.30f) {
            std::cout << "PQ映射超出范围: " << pq_value << std::endl;
            return false;
        }
        
        return true;
    }
    
    /**
     * 测试统计收集
     */
    static bool TestStatisticsCollection() {
        // 简化的统计测试
        DCTLStatistics stats = InitializeStatistics();
        
        // 验证初始化状态
        if (stats.min_pq_encoded_max_rgb != 1.0f) {
            std::cout << "统计初始化错误: min应为1.0" << std::endl;
            return false;
        }
        
        if (stats.max_pq_encoded_max_rgb != 0.0f) {
            std::cout << "统计初始化错误: max应为0.0" << std::endl;
            return false;
        }
        
        if (!stats.is_monotonic || !stats.is_c1_continuous) {
            std::cout << "统计初始化错误: 验证标志应为true" << std::endl;
            return false;
        }
        
        return true;
    }
};

} // namespace DCTLValidation
} // namespace CinemaProHDR

/**
 * 主函数：运行DCTL验证测试
 */
int main() {
    try {
        bool success = CinemaProHDR::DCTLValidation::DCTLTestSuite::RunAllTests();
        return success ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "测试异常: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "未知测试异常" << std::endl;
        return 1;
    }
}