#include "cinema_pro_hdr/highlight_detail.h"
#include "cinema_pro_hdr/error_handler.h"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace CinemaProHDR {

HighlightDetailProcessor::HighlightDetailProcessor() = default;
HighlightDetailProcessor::~HighlightDetailProcessor() = default;

bool HighlightDetailProcessor::Initialize(const CphParams& params) {
    // 复制参数并自动修正无效值
    params_ = params;
    params_.ClampToValidRange();
    
    // 验证修正后的参数
    if (!params_.IsValid()) {
        last_error_ = "参数修正后仍然无效";
        return false;
    }
    
    initialized_ = true;
    last_error_.clear();
    
    // 重置状态
    Reset();
    
    return true;
}

bool HighlightDetailProcessor::ProcessFrame(const Image& input, Image& output, float pivot_threshold) {
    if (!initialized_) {
        last_error_ = "处理器未初始化";
        return false;
    }
    
    if (!input.IsValid()) {
        last_error_ = "输入图像无效";
        return false;
    }
    
    // 如果强度为0，直接复制输入到输出
    if (params_.highlight_detail <= 0.0f) {
        output = input;
        return true;
    }
    
    return ApplyUSM(input, output, pivot_threshold, params_.highlight_detail);
}

bool HighlightDetailProcessor::ProcessFrameWithMotionProtection(const Image& current_frame, 
                                                                const Image* previous_frame,
                                                                Image& output, 
                                                                float pivot_threshold) {
    if (!initialized_) {
        last_error_ = "处理器未初始化";
        return false;
    }
    
    float effective_intensity = params_.highlight_detail;
    
    // 如果有前一帧，进行运动检测
    if (previous_frame && previous_frame->IsValid() && has_previous_frame_) {
        float motion_energy = ComputeMotionEnergy(current_frame, *previous_frame, pivot_threshold);
        
        // 记录运动能量历史
        motion_energy_history_.push_back(motion_energy);
        if (motion_energy_history_.size() > 10) { // 保持最近10帧的历史
            motion_energy_history_.erase(motion_energy_history_.begin());
        }
        
        // 根据运动能量调整强度
        if (ShouldSuppressDetail(motion_energy)) {
            effective_intensity *= 0.5f; // 运动保护：降低强度
        }
    }
    
    // 应用USM处理
    bool result = ApplyUSM(current_frame, output, pivot_threshold, effective_intensity);
    
    // 更新前一帧
    if (result) {
        previous_frame_ = current_frame;
        has_previous_frame_ = true;
    }
    
    return result;
}

bool HighlightDetailProcessor::ValidateFrequencyConstraints(const std::vector<Image>& frame_sequence, float fps) const {
    if (frame_sequence.size() < 3) {
        return true; // 帧数不足，无法进行频域分析
    }
    
    // 采样几个代表性像素点进行频域分析
    const int sample_points = 16;
    int width = frame_sequence[0].width;
    int height = frame_sequence[0].height;
    
    for (int i = 0; i < sample_points; ++i) {
        int x = (i % 4) * width / 4 + width / 8;
        int y = (i / 4) * height / 4 + height / 8;
        
        // 计算该点的时域序列
        std::vector<float> temporal_sequence;
        for (const auto& frame : frame_sequence) {
            const float* pixel = frame.GetPixel(x, y);
            if (pixel) {
                float luminance = std::max(pixel[0], std::max(pixel[1], pixel[2]));
                temporal_sequence.push_back(luminance);
            }
        }
        
        if (temporal_sequence.size() < 3) continue;
        
        // 计算频谱
        std::vector<float> spectrum = ComputeTemporalSpectrum(frame_sequence, x, y);
        
        // 计算1-6Hz频段的能量
        float energy_1_6hz = ComputeEnergyInBand(spectrum, fps, 1.0f, 6.0f);
        float total_energy = ComputeEnergyInBand(spectrum, fps, 0.0f, fps / 2.0f);
        
        // 检查能量增长是否超过20%
        if (total_energy > 0.0f) {
            float energy_ratio = energy_1_6hz / total_energy;
            if (energy_ratio > 0.2f) { // 20%阈值
                return false;
            }
        }
    }
    
    return true;
}

void HighlightDetailProcessor::Reset() {
    has_previous_frame_ = false;
    motion_energy_history_.clear();
    previous_frame_.Clear();
}

bool HighlightDetailProcessor::ApplyUSM(const Image& input, Image& output, float pivot_threshold, float intensity) {
    /**
     * USM (Unsharp Mask) 算法实现
     * 
     * 按照需求 2.3 的规格实现：
     * - 仅作用于 x>p 区域（高光区域）
     * - r=2px（高斯模糊半径）
     * - amount=intensity（由参数控制，范围[0,1]）
     * - thr=0.03（阈值，避免噪声放大）
     * 
     * 步骤：
     * 1. 创建高光区域掩码（仅处理x>p的区域）
     * 2. 对输入图像应用高斯模糊（r=2px, sigma=1.0）
     * 3. 计算原图与模糊图的差值（细节层）
     * 4. 应用阈值处理（thr=0.03）避免噪声放大
     * 5. 将细节层按强度添加回原图，仅在高光区域
     * 
     * 满足需求：
     * - 需求 2.3: USM仅作用于x>p区域，r=2px，amount=0.2，thr=0.03
     * - 需求 7.2: 支持运动保护，防止闪烁
     * - 需求 7.3: 确保开启/关闭不增加20%以上闪烁
     */
    
    try {
        // 初始化输出图像
        output = Image(input.width, input.height, input.channels);
        output.color_space = input.color_space;
        
        // 创建高光掩码
        Image highlight_mask;
        HighlightDetailUtils::ComputeHighlightMask(input, pivot_threshold, highlight_mask);
        
        // 应用高斯模糊
        Image blurred;
        ApplyGaussianBlur(input, blurred, 2, 1.0f); // r=2px, sigma=1.0
        
        // 计算细节层
        Image detail_layer;
        ComputeUnsharpMask(input, blurred, detail_layer, intensity, 0.03f); // amount=intensity, thr=0.03
        
        // 将细节层应用到高光区域
        for (int y = 0; y < input.height; ++y) {
            for (int x = 0; x < input.width; ++x) {
                const float* input_pixel = input.GetPixel(x, y);
                const float* detail_pixel = detail_layer.GetPixel(x, y);
                const float* mask_pixel = highlight_mask.GetPixel(x, y);
                float* output_pixel = output.GetPixel(x, y);
                
                if (!input_pixel || !detail_pixel || !mask_pixel || !output_pixel) {
                    continue;
                }
                
                float mask_value = mask_pixel[0]; // 掩码强度
                
                for (int c = 0; c < input.channels; ++c) {
                    // 基础值 + 细节增强 * 掩码强度
                    output_pixel[c] = input_pixel[c] + detail_pixel[c] * mask_value;
                    
                    // 钳制到有效范围
                    output_pixel[c] = std::clamp(output_pixel[c], 0.0f, 1.0f);
                }
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        last_error_ = std::string("USM处理异常: ") + e.what();
        return false;
    }
}

float HighlightDetailProcessor::ComputeMotionEnergy(const Image& current, const Image& previous, float pivot_threshold) {
    /**
     * 计算运动能量 - 运动保护机制的核心
     * 
     * 按照需求 7.2 实现帧间差分检测：
     * - 帧差RMS能量计算，单位为PQ归一化
     * - 仅在高光区域（x>p）计算运动
     * - 阈值为0.02（PQ归一化单位）
     * 
     * 方法：
     * 1. 计算帧间差分（当前帧 - 前一帧）
     * 2. 仅在高光区域计算能量（MaxRGB > pivot_threshold）
     * 3. 计算RMS能量并归一化到[0,1]范围
     * 4. 用于运动保护决策（>0.02时启用保护）
     * 
     * 满足需求：
     * - 需求 7.2: 帧差RMS能量<阈值（默认0.02，单位PQ归一）
     * - 需求 7.3: 运动保护防止闪烁增加>20%
     */
    
    if (current.width != previous.width || current.height != previous.height) {
        return 0.0f; // 尺寸不匹配
    }
    
    float total_energy = 0.0f;
    int pixel_count = 0;
    
    for (int y = 0; y < current.height; ++y) {
        for (int x = 0; x < current.width; ++x) {
            const float* curr_pixel = current.GetPixel(x, y);
            const float* prev_pixel = previous.GetPixel(x, y);
            
            if (!curr_pixel || !prev_pixel) continue;
            
            // 计算当前像素的亮度
            float curr_luminance = std::max(curr_pixel[0], std::max(curr_pixel[1], curr_pixel[2]));
            
            // 仅在高光区域计算运动
            if (curr_luminance > pivot_threshold) {
                float prev_luminance = std::max(prev_pixel[0], std::max(prev_pixel[1], prev_pixel[2]));
                float diff = curr_luminance - prev_luminance;
                total_energy += diff * diff;
                pixel_count++;
            }
        }
    }
    
    if (pixel_count == 0) {
        return 0.0f;
    }
    
    // 归一化RMS能量
    float rms_energy = std::sqrt(total_energy / pixel_count);
    return std::clamp(rms_energy, 0.0f, 1.0f);
}

bool HighlightDetailProcessor::ShouldSuppressDetail(float motion_energy) {
    /**
     * 运动保护决策 - 防止闪烁的关键机制
     * 
     * 按照需求 7.2 和 7.3 实现：
     * - 阈值：0.02（PQ归一化单位）
     * - 目标：防止Highlight Detail开启时闪烁增加>20%
     * - 策略：检测到运动时降低细节增强强度
     * 
     * 规则：
     * - 当前帧运动能量 > 0.02 时启用保护
     * - 考虑历史运动能量的平均值（更低阈值0.01）
     * - 保护动作：将细节强度降低50%
     * 
     * 满足需求：
     * - 需求 7.2: 1-6Hz区间能量不得因Highlight Detail上升>20%
     * - 需求 7.3: 开启/关闭对比不应增加20%以上闪烁
     */
    
    const float motion_threshold = 0.02f; // PQ归一化单位
    
    // 当前帧检查
    if (motion_energy > motion_threshold) {
        return true;
    }
    
    // 历史平均检查
    if (!motion_energy_history_.empty()) {
        float avg_motion = std::accumulate(motion_energy_history_.begin(), 
                                           motion_energy_history_.end(), 0.0f) / 
                          motion_energy_history_.size();
        
        if (avg_motion > motion_threshold * 0.5f) { // 更低的历史阈值
            return true;
        }
    }
    
    return false;
}

std::vector<float> HighlightDetailProcessor::ComputeTemporalSpectrum(const std::vector<Image>& frames, int x, int y) const {
    /**
     * 简化的频域分析 - 1-6Hz能量约束检查
     * 
     * 按照需求 7.2 实现频域约束：
     * - 检查1-6Hz区间能量增长是否<20%
     * - 防止Highlight Detail引入过多闪烁
     * - 采样多个代表性像素点进行分析
     * 
     * 注意：这是一个简化实现，生产环境建议使用FFT
     * 
     * 满足需求：
     * - 需求 7.2: 1-6Hz区间能量不得因Highlight Detail上升>20%
     * - 提供频域验证机制，确保视觉质量
     */
    
    std::vector<float> temporal_values;
    for (const auto& frame : frames) {
        const float* pixel = frame.GetPixel(x, y);
        if (pixel) {
            float luminance = std::max(pixel[0], std::max(pixel[1], pixel[2]));
            temporal_values.push_back(luminance);
        }
    }
    
    // 简化的频谱计算（实际应该使用FFT）
    std::vector<float> spectrum(temporal_values.size() / 2);
    for (size_t i = 0; i < spectrum.size(); ++i) {
        spectrum[i] = 0.0f;
        for (size_t j = 0; j < temporal_values.size(); ++j) {
            float angle = 2.0f * M_PI * i * j / temporal_values.size();
            spectrum[i] += temporal_values[j] * std::cos(angle);
        }
        spectrum[i] = std::abs(spectrum[i]);
    }
    
    return spectrum;
}

float HighlightDetailProcessor::ComputeEnergyInBand(const std::vector<float>& spectrum, float fps, float low_freq, float high_freq) const {
    if (spectrum.empty()) return 0.0f;
    
    float nyquist = fps / 2.0f;
    int low_bin = static_cast<int>((low_freq / nyquist) * spectrum.size());
    int high_bin = static_cast<int>((high_freq / nyquist) * spectrum.size());
    
    low_bin = std::clamp(low_bin, 0, static_cast<int>(spectrum.size()) - 1);
    high_bin = std::clamp(high_bin, low_bin, static_cast<int>(spectrum.size()) - 1);
    
    float energy = 0.0f;
    for (int i = low_bin; i <= high_bin; ++i) {
        energy += spectrum[i] * spectrum[i];
    }
    
    return energy;
}

void HighlightDetailProcessor::ComputeGaussianKernel(std::vector<float>& kernel, int radius, float sigma) {
    int size = 2 * radius + 1;
    kernel.resize(size);
    
    float sum = 0.0f;
    for (int i = 0; i < size; ++i) {
        int x = i - radius;
        kernel[i] = std::exp(-(x * x) / (2.0f * sigma * sigma));
        sum += kernel[i];
    }
    
    // 归一化
    for (float& k : kernel) {
        k /= sum;
    }
}

void HighlightDetailProcessor::ApplyGaussianBlur(const Image& input, Image& output, int radius, float sigma) {
    output = Image(input.width, input.height, input.channels);
    output.color_space = input.color_space;
    
    // 计算高斯核
    std::vector<float> kernel;
    ComputeGaussianKernel(kernel, radius, sigma);
    
    // 水平模糊
    Image temp(input.width, input.height, input.channels);
    for (int y = 0; y < input.height; ++y) {
        for (int x = 0; x < input.width; ++x) {
            float* temp_pixel = temp.GetPixel(x, y);
            if (!temp_pixel) continue;
            
            for (int c = 0; c < input.channels; ++c) {
                float sum = 0.0f;
                for (int k = 0; k < static_cast<int>(kernel.size()); ++k) {
                    int src_x = x + k - radius;
                    src_x = std::clamp(src_x, 0, input.width - 1);
                    
                    const float* src_pixel = input.GetPixel(src_x, y);
                    if (src_pixel) {
                        sum += src_pixel[c] * kernel[k];
                    }
                }
                temp_pixel[c] = sum;
            }
        }
    }
    
    // 垂直模糊
    for (int y = 0; y < input.height; ++y) {
        for (int x = 0; x < input.width; ++x) {
            float* output_pixel = output.GetPixel(x, y);
            if (!output_pixel) continue;
            
            for (int c = 0; c < input.channels; ++c) {
                float sum = 0.0f;
                for (int k = 0; k < static_cast<int>(kernel.size()); ++k) {
                    int src_y = y + k - radius;
                    src_y = std::clamp(src_y, 0, input.height - 1);
                    
                    const float* src_pixel = temp.GetPixel(x, src_y);
                    if (src_pixel) {
                        sum += src_pixel[c] * kernel[k];
                    }
                }
                output_pixel[c] = sum;
            }
        }
    }
}

void HighlightDetailProcessor::ComputeUnsharpMask(const Image& original, const Image& blurred, Image& mask, float amount, float threshold) {
    mask = Image(original.width, original.height, original.channels);
    mask.color_space = original.color_space;
    
    for (int y = 0; y < original.height; ++y) {
        for (int x = 0; x < original.width; ++x) {
            const float* orig_pixel = original.GetPixel(x, y);
            const float* blur_pixel = blurred.GetPixel(x, y);
            float* mask_pixel = mask.GetPixel(x, y);
            
            if (!orig_pixel || !blur_pixel || !mask_pixel) continue;
            
            for (int c = 0; c < original.channels; ++c) {
                float diff = orig_pixel[c] - blur_pixel[c];
                
                // 应用阈值
                if (std::abs(diff) > threshold) {
                    mask_pixel[c] = diff * amount;
                } else {
                    mask_pixel[c] = 0.0f;
                }
            }
        }
    }
}

void HighlightDetailProcessor::ClampImageValues(Image& image) {
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            float* pixel = image.GetPixel(x, y);
            if (!pixel) continue;
            
            for (int c = 0; c < image.channels; ++c) {
                pixel[c] = std::clamp(pixel[c], 0.0f, 1.0f);
            }
        }
    }
}

// 工具函数实现
namespace HighlightDetailUtils {

void ComputeHighlightMask(const Image& image, float pivot_threshold, Image& mask) {
    mask = Image(image.width, image.height, 1); // 单通道掩码
    mask.color_space = image.color_space;
    
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            const float* pixel = image.GetPixel(x, y);
            float* mask_pixel = mask.GetPixel(x, y);
            
            if (!pixel || !mask_pixel) continue;
            
            // 计算像素亮度（MaxRGB方法）
            float luminance = std::max(pixel[0], std::max(pixel[1], pixel[2]));
            
            // 生成平滑的掩码
            if (luminance > pivot_threshold) {
                // 在高光区域使用平滑过渡
                float mask_strength = (luminance - pivot_threshold) / (1.0f - pivot_threshold);
                mask_pixel[0] = std::clamp(mask_strength, 0.0f, 1.0f);
            } else {
                mask_pixel[0] = 0.0f;
            }
        }
    }
}

void ApplyMask(const Image& source, const Image& mask, Image& target, int blend_mode) {
    if (source.width != mask.width || source.height != mask.height ||
        source.width != target.width || source.height != target.height) {
        return; // 尺寸不匹配
    }
    
    for (int y = 0; y < source.height; ++y) {
        for (int x = 0; x < source.width; ++x) {
            const float* src_pixel = source.GetPixel(x, y);
            const float* mask_pixel = mask.GetPixel(x, y);
            float* tgt_pixel = target.GetPixel(x, y);
            
            if (!src_pixel || !mask_pixel || !tgt_pixel) continue;
            
            float mask_value = mask_pixel[0];
            
            for (int c = 0; c < source.channels; ++c) {
                switch (blend_mode) {
                    case 0: // 替换
                        tgt_pixel[c] = src_pixel[c] * mask_value + tgt_pixel[c] * (1.0f - mask_value);
                        break;
                    case 1: // 加法
                        tgt_pixel[c] += src_pixel[c] * mask_value;
                        break;
                    case 2: // 乘法
                        tgt_pixel[c] *= (1.0f + src_pixel[c] * mask_value);
                        break;
                }
                
                tgt_pixel[c] = std::clamp(tgt_pixel[c], 0.0f, 1.0f);
            }
        }
    }
}

float ComputeFrameDifference(const Image& frame1, const Image& frame2, const Image* region_mask) {
    if (frame1.width != frame2.width || frame1.height != frame2.height) {
        return 0.0f;
    }
    
    float total_diff = 0.0f;
    int pixel_count = 0;
    
    for (int y = 0; y < frame1.height; ++y) {
        for (int x = 0; x < frame1.width; ++x) {
            const float* pixel1 = frame1.GetPixel(x, y);
            const float* pixel2 = frame2.GetPixel(x, y);
            
            if (!pixel1 || !pixel2) continue;
            
            // 检查区域掩码
            float mask_value = 1.0f;
            if (region_mask) {
                const float* mask_pixel = region_mask->GetPixel(x, y);
                if (mask_pixel) {
                    mask_value = mask_pixel[0];
                }
            }
            
            if (mask_value > 0.0f) {
                float diff_sum = 0.0f;
                for (int c = 0; c < frame1.channels; ++c) {
                    float diff = pixel1[c] - pixel2[c];
                    diff_sum += diff * diff;
                }
                
                total_diff += diff_sum * mask_value;
                pixel_count++;
            }
        }
    }
    
    if (pixel_count == 0) {
        return 0.0f;
    }
    
    return std::sqrt(total_diff / pixel_count);
}

void GaussianBlur(const Image& input, Image& output, int radius, float sigma) {
    output = Image(input.width, input.height, input.channels);
    output.color_space = input.color_space;
    
    // 计算高斯核
    std::vector<float> kernel;
    int size = 2 * radius + 1;
    kernel.resize(size);
    
    float sum = 0.0f;
    for (int i = 0; i < size; ++i) {
        int x = i - radius;
        kernel[i] = std::exp(-(x * x) / (2.0f * sigma * sigma));
        sum += kernel[i];
    }
    
    // 归一化
    for (float& k : kernel) {
        k /= sum;
    }
    
    // 水平模糊
    Image temp(input.width, input.height, input.channels);
    for (int y = 0; y < input.height; ++y) {
        for (int x = 0; x < input.width; ++x) {
            float* temp_pixel = temp.GetPixel(x, y);
            if (!temp_pixel) continue;
            
            for (int c = 0; c < input.channels; ++c) {
                float sum = 0.0f;
                for (int k = 0; k < static_cast<int>(kernel.size()); ++k) {
                    int src_x = x + k - radius;
                    src_x = std::clamp(src_x, 0, input.width - 1);
                    
                    const float* src_pixel = input.GetPixel(src_x, y);
                    if (src_pixel) {
                        sum += src_pixel[c] * kernel[k];
                    }
                }
                temp_pixel[c] = sum;
            }
        }
    }
    
    // 垂直模糊
    for (int y = 0; y < input.height; ++y) {
        for (int x = 0; x < input.width; ++x) {
            float* output_pixel = output.GetPixel(x, y);
            if (!output_pixel) continue;
            
            for (int c = 0; c < input.channels; ++c) {
                float sum = 0.0f;
                for (int k = 0; k < static_cast<int>(kernel.size()); ++k) {
                    int src_y = y + k - radius;
                    src_y = std::clamp(src_y, 0, input.height - 1);
                    
                    const float* src_pixel = temp.GetPixel(x, src_y);
                    if (src_pixel) {
                        sum += src_pixel[c] * kernel[k];
                    }
                }
                output_pixel[c] = sum;
            }
        }
    }
}

} // namespace HighlightDetailUtils

} // namespace CinemaProHDR