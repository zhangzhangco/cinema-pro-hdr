/**
 * Cinema Pro HDR DCTL 统计收集器
 * 
 * 用途：收集和分析DCTL处理过程中的实时统计信息
 * 功能：min/avg/max PqEncodedMaxRGB计算，性能监控
 * 
 * 注意：由于DCTL的限制，实际的统计收集需要通过
 * 外部机制实现（如Resolve回调或独立的统计线程）
 */

#include "parameter_mapping.h"
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <mutex>
#include <atomic>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <sstream>

using namespace CinemaProHDR::DCTLMapping;

namespace CinemaProHDR {
namespace DCTLStats {

/**
 * 线程安全的统计收集器类
 */
class StatisticsCollector {
private:
    mutable std::mutex stats_mutex_;
    std::vector<float> pq_max_rgb_samples_;
    std::atomic<size_t> total_pixels_{0};
    std::atomic<float> processing_time_sum_{0.0f};
    std::atomic<size_t> frame_count_{0};
    
    // 配置参数
    static constexpr size_t MAX_SAMPLES = 10000;  // 最大样本数
    static constexpr float OUTLIER_PERCENTILE = 0.01f;  // 1%顶帽去极值
    
public:
    /**
     * 添加单个像素的PQ MaxRGB值
     */
    void AddPqMaxRgbSample(float pq_max_rgb) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        
        // 验证输入值
        if (!std::isfinite(pq_max_rgb) || pq_max_rgb < 0.0f || pq_max_rgb > 1.0f) {
            return; // 忽略无效样本
        }
        
        pq_max_rgb_samples_.push_back(pq_max_rgb);
        
        // 限制样本数量，使用滑动窗口
        if (pq_max_rgb_samples_.size() > MAX_SAMPLES) {
            // 移除最旧的样本（FIFO）
            pq_max_rgb_samples_.erase(pq_max_rgb_samples_.begin());
        }
        
        total_pixels_.fetch_add(1);
    }
    
    /**
     * 批量添加PQ MaxRGB样本
     */
    void AddPqMaxRgbSamples(const std::vector<float>& samples) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        
        for (float sample : samples) {
            if (std::isfinite(sample) && sample >= 0.0f && sample <= 1.0f) {
                pq_max_rgb_samples_.push_back(sample);
            }
        }
        
        // 限制样本数量
        if (pq_max_rgb_samples_.size() > MAX_SAMPLES) {
            size_t excess = pq_max_rgb_samples_.size() - MAX_SAMPLES;
            pq_max_rgb_samples_.erase(pq_max_rgb_samples_.begin(), 
                                     pq_max_rgb_samples_.begin() + excess);
        }
        
        total_pixels_.fetch_add(samples.size());
    }
    
    /**
     * 记录帧处理时间
     */
    void RecordFrameProcessingTime(double time_ms) {
        if (std::isfinite(time_ms) && time_ms >= 0.0) {
            float current_sum = processing_time_sum_.load();
            while (!processing_time_sum_.compare_exchange_weak(current_sum, current_sum + static_cast<float>(time_ms))) {
                // 重试直到成功
            }
            frame_count_.fetch_add(1);
        }
    }
    
    /**
     * 计算当前统计信息
     */
    DCTLStatistics ComputeCurrentStatistics() const {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        
        DCTLStatistics stats = InitializeStatistics();
        
        if (pq_max_rgb_samples_.empty()) {
            return stats;
        }
        
        // 复制样本并排序（用于百分位数计算）
        std::vector<float> sorted_samples = pq_max_rgb_samples_;
        std::sort(sorted_samples.begin(), sorted_samples.end());
        
        // 计算1%顶帽去极值的范围
        size_t sample_count = sorted_samples.size();
        size_t outlier_count = static_cast<size_t>(sample_count * OUTLIER_PERCENTILE);
        size_t start_idx = outlier_count;
        size_t end_idx = sample_count - outlier_count;
        
        if (start_idx >= end_idx) {
            // 样本太少，使用全部样本
            start_idx = 0;
            end_idx = sample_count;
        }
        
        // 提取有效样本范围
        std::vector<float> trimmed_samples(sorted_samples.begin() + start_idx,
                                          sorted_samples.begin() + end_idx);
        
        if (!trimmed_samples.empty()) {
            // 计算最小值、最大值
            stats.min_pq_encoded_max_rgb = trimmed_samples.front();
            stats.max_pq_encoded_max_rgb = trimmed_samples.back();
            
            // 计算截尾均值
            double sum = std::accumulate(trimmed_samples.begin(), trimmed_samples.end(), 0.0);
            stats.avg_pq_encoded_max_rgb = static_cast<float>(sum / trimmed_samples.size());
            
            // 计算方差
            double variance_sum = 0.0;
            for (float sample : trimmed_samples) {
                double diff = sample - stats.avg_pq_encoded_max_rgb;
                variance_sum += diff * diff;
            }
            stats.variance_pq_encoded_max_rgb = static_cast<float>(variance_sum / trimmed_samples.size());
        }
        
        // 计算平均处理时间
        size_t frame_count = frame_count_.load();
        if (frame_count > 0) {
            stats.processing_time_ms = static_cast<float>(processing_time_sum_.load() / frame_count);
        }
        
        stats.processed_pixels = static_cast<int>(total_pixels_.load());
        
        return stats;
    }
    
    /**
     * 获取详细的百分位数统计
     */
    struct PercentileStats {
        float p1, p5, p10, p25, p50, p75, p90, p95, p99;
        float mean, std_dev;
        size_t sample_count;
    };
    
    PercentileStats ComputePercentileStatistics() const {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        
        PercentileStats percentiles = {};
        
        if (pq_max_rgb_samples_.empty()) {
            return percentiles;
        }
        
        std::vector<float> sorted_samples = pq_max_rgb_samples_;
        std::sort(sorted_samples.begin(), sorted_samples.end());
        
        size_t n = sorted_samples.size();
        percentiles.sample_count = n;
        
        // 计算百分位数
        auto getPercentile = [&](float p) -> float {
            if (n == 0) return 0.0f;
            float index = p * (n - 1);
            size_t lower = static_cast<size_t>(std::floor(index));
            size_t upper = static_cast<size_t>(std::ceil(index));
            
            if (lower == upper) {
                return sorted_samples[lower];
            } else {
                float weight = index - lower;
                return sorted_samples[lower] * (1.0f - weight) + sorted_samples[upper] * weight;
            }
        };
        
        percentiles.p1 = getPercentile(0.01f);
        percentiles.p5 = getPercentile(0.05f);
        percentiles.p10 = getPercentile(0.10f);
        percentiles.p25 = getPercentile(0.25f);
        percentiles.p50 = getPercentile(0.50f);  // 中位数
        percentiles.p75 = getPercentile(0.75f);
        percentiles.p90 = getPercentile(0.90f);
        percentiles.p95 = getPercentile(0.95f);
        percentiles.p99 = getPercentile(0.99f);
        
        // 计算均值和标准差
        double sum = std::accumulate(sorted_samples.begin(), sorted_samples.end(), 0.0);
        percentiles.mean = static_cast<float>(sum / n);
        
        double variance_sum = 0.0;
        for (float sample : sorted_samples) {
            double diff = sample - percentiles.mean;
            variance_sum += diff * diff;
        }
        percentiles.std_dev = static_cast<float>(std::sqrt(variance_sum / n));
        
        return percentiles;
    }
    
    /**
     * 重置统计信息
     */
    void Reset() {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        pq_max_rgb_samples_.clear();
        total_pixels_.store(0);
        processing_time_sum_.store(0.0);
        frame_count_.store(0);
    }
    
    /**
     * 获取当前样本数量
     */
    size_t GetSampleCount() const {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return pq_max_rgb_samples_.size();
    }
    
    /**
     * 检查是否有足够的样本进行统计
     */
    bool HasSufficientSamples(size_t min_samples = 100) const {
        return GetSampleCount() >= min_samples;
    }
};

/**
 * 全局统计收集器实例
 */
static StatisticsCollector g_statistics_collector;

/**
 * C接口函数：供DCTL或其他C代码调用
 */
extern "C" {
    
    /**
     * 添加PQ MaxRGB样本
     */
    void cph_dctl_add_pq_sample(float pq_max_rgb) {
        g_statistics_collector.AddPqMaxRgbSample(pq_max_rgb);
    }
    
    /**
     * 记录处理时间
     */
    void cph_dctl_record_time(double time_ms) {
        g_statistics_collector.RecordFrameProcessingTime(time_ms);
    }
    
    /**
     * 获取当前统计信息
     */
    DCTLStatistics cph_dctl_get_statistics() {
        return g_statistics_collector.ComputeCurrentStatistics();
    }
    
    /**
     * 重置统计信息
     */
    void cph_dctl_reset_statistics() {
        g_statistics_collector.Reset();
    }
    
    /**
     * 检查样本数量
     */
    int cph_dctl_get_sample_count() {
        return static_cast<int>(g_statistics_collector.GetSampleCount());
    }
}

/**
 * 性能监控辅助类
 */
class PerformanceMonitor {
private:
    std::chrono::high_resolution_clock::time_point start_time_;
    bool is_running_;
    
public:
    PerformanceMonitor() : is_running_(false) {}
    
    /**
     * 开始计时
     */
    void Start() {
        start_time_ = std::chrono::high_resolution_clock::now();
        is_running_ = true;
    }
    
    /**
     * 停止计时并记录结果
     */
    double Stop() {
        if (!is_running_) return 0.0;
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time_);
        double time_ms = duration.count() / 1000.0;
        
        g_statistics_collector.RecordFrameProcessingTime(time_ms);
        is_running_ = false;
        
        return time_ms;
    }
    
    /**
     * 获取当前经过的时间（不停止计时）
     */
    double GetElapsedTime() const {
        if (!is_running_) return 0.0;
        
        auto current_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(current_time - start_time_);
        return duration.count() / 1000.0;
    }
};

/**
 * RAII性能监控器：自动计时
 */
class ScopedPerformanceMonitor {
private:
    PerformanceMonitor monitor_;
    
public:
    ScopedPerformanceMonitor() {
        monitor_.Start();
    }
    
    ~ScopedPerformanceMonitor() {
        monitor_.Stop();
    }
    
    double GetElapsedTime() const {
        return monitor_.GetElapsedTime();
    }
};

/**
 * 统计报告生成器
 */
class StatisticsReporter {
public:
    /**
     * 生成文本格式的统计报告
     */
    static std::string GenerateTextReport() {
        DCTLStatistics stats = g_statistics_collector.ComputeCurrentStatistics();
        StatisticsCollector::PercentileStats percentiles = g_statistics_collector.ComputePercentileStatistics();
        
        std::ostringstream report;
        report << "=== Cinema Pro HDR DCTL 统计报告 ===\n";
        report << "处理像素数: " << stats.processed_pixels << "\n";
        report << "样本数量: " << percentiles.sample_count << "\n";
        report << "\n";
        
        report << "PQ编码MaxRGB统计:\n";
        report << "  最小值: " << std::fixed << std::setprecision(4) << stats.min_pq_encoded_max_rgb << "\n";
        report << "  平均值: " << stats.avg_pq_encoded_max_rgb << "\n";
        report << "  最大值: " << stats.max_pq_encoded_max_rgb << "\n";
        report << "  标准差: " << percentiles.std_dev << "\n";
        report << "\n";
        
        report << "百分位数分布:\n";
        report << "  P1:  " << percentiles.p1 << "\n";
        report << "  P5:  " << percentiles.p5 << "\n";
        report << "  P25: " << percentiles.p25 << "\n";
        report << "  P50: " << percentiles.p50 << " (中位数)\n";
        report << "  P75: " << percentiles.p75 << "\n";
        report << "  P95: " << percentiles.p95 << "\n";
        report << "  P99: " << percentiles.p99 << "\n";
        report << "\n";
        
        report << "性能指标:\n";
        report << "  平均处理时间: " << stats.processing_time_ms << " ms/帧\n";
        
        // 性能评估
        if (stats.processing_time_ms > 0.0f) {
            if (stats.processing_time_ms < 1.0f) {
                report << "  性能评级: 优秀 (4K目标: <1ms)\n";
            } else if (stats.processing_time_ms < 1.2f) {
                report << "  性能评级: 良好 (接近4K目标)\n";
            } else if (stats.processing_time_ms < 3.5f) {
                report << "  性能评级: 可接受 (8K目标: <3.5ms)\n";
            } else {
                report << "  性能评级: 需要优化 (超出8K目标)\n";
            }
        }
        
        report << "\n";
        report << "曲线验证状态:\n";
        report << "  单调性: " << (stats.is_monotonic ? "通过" : "失败") << "\n";
        report << "  C¹连续性: " << (stats.is_c1_continuous ? "通过" : "失败") << "\n";
        if (stats.max_derivative_gap > 0.0f) {
            report << "  最大导数间隙: " << stats.max_derivative_gap << "\n";
        }
        
        return report.str();
    }
    
    /**
     * 生成JSON格式的统计报告
     */
    static std::string GenerateJsonReport() {
        DCTLStatistics stats = g_statistics_collector.ComputeCurrentStatistics();
        StatisticsCollector::PercentileStats percentiles = g_statistics_collector.ComputePercentileStatistics();
        
        std::ostringstream json;
        json << "{\n";
        json << "  \"processed_pixels\": " << stats.processed_pixels << ",\n";
        json << "  \"sample_count\": " << percentiles.sample_count << ",\n";
        json << "  \"pq_max_rgb_stats\": {\n";
        json << "    \"min\": " << stats.min_pq_encoded_max_rgb << ",\n";
        json << "    \"avg\": " << stats.avg_pq_encoded_max_rgb << ",\n";
        json << "    \"max\": " << stats.max_pq_encoded_max_rgb << ",\n";
        json << "    \"std_dev\": " << percentiles.std_dev << ",\n";
        json << "    \"variance\": " << stats.variance_pq_encoded_max_rgb << "\n";
        json << "  },\n";
        json << "  \"percentiles\": {\n";
        json << "    \"p1\": " << percentiles.p1 << ",\n";
        json << "    \"p5\": " << percentiles.p5 << ",\n";
        json << "    \"p25\": " << percentiles.p25 << ",\n";
        json << "    \"p50\": " << percentiles.p50 << ",\n";
        json << "    \"p75\": " << percentiles.p75 << ",\n";
        json << "    \"p95\": " << percentiles.p95 << ",\n";
        json << "    \"p99\": " << percentiles.p99 << "\n";
        json << "  },\n";
        json << "  \"performance\": {\n";
        json << "    \"avg_processing_time_ms\": " << stats.processing_time_ms << "\n";
        json << "  },\n";
        json << "  \"validation\": {\n";
        json << "    \"is_monotonic\": " << (stats.is_monotonic ? "true" : "false") << ",\n";
        json << "    \"is_c1_continuous\": " << (stats.is_c1_continuous ? "true" : "false") << ",\n";
        json << "    \"max_derivative_gap\": " << stats.max_derivative_gap << "\n";
        json << "  }\n";
        json << "}\n";
        
        return json.str();
    }
};

} // namespace DCTLStats
} // namespace CinemaProHDR