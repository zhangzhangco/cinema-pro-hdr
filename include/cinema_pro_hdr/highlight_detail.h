#pragma once

#include "core.h"
#include <vector>

namespace CinemaProHDR {

/**
 * @brief 高光细节处理模块
 * 
 * 实现USM（Unsharp Mask）算法用于增强高光区域的细节：
 * - 仅作用于x>p区域（高光区域）
 * - 使用运动保护机制防止闪烁
 * - 频域约束检查确保1-6Hz区间能量增长<20%
 * - 支持λ_hd∈[0,1]强度控制
 * 
 * 用途：增强高光区域的细节和锐度
 * 不是：全局锐化或噪声增强
 * 
 * 适用场景：
 * - 需要保持高光细节的HDR内容
 * - 静态或低运动的场景
 * - 要求视觉质量提升的专业制作
 * 
 * 不适用场景：
 * - 高运动或快速变化的场景
 * - 已经有噪声的内容
 * - 需要严格控制闪烁的广播内容
 */
class HighlightDetailProcessor {
public:
    HighlightDetailProcessor();
    ~HighlightDetailProcessor();
    
    /**
     * @brief 初始化高光细节处理器
     * @param params 处理参数
     * @return 初始化是否成功
     */
    bool Initialize(const CphParams& params);
    
    /**
     * @brief 处理单帧图像的高光细节
     * @param input 输入图像（工作域）
     * @param output 输出图像
     * @param pivot_threshold 高光阈值（PQ归一化）
     * @return 处理是否成功
     */
    bool ProcessFrame(const Image& input, Image& output, float pivot_threshold);
    
    /**
     * @brief 处理带运动保护的帧序列
     * @param current_frame 当前帧
     * @param previous_frame 前一帧（可选，用于运动检测）
     * @param output 输出帧
     * @param pivot_threshold 高光阈值
     * @return 处理是否成功
     */
    bool ProcessFrameWithMotionProtection(const Image& current_frame, 
                                          const Image* previous_frame,
                                          Image& output, 
                                          float pivot_threshold);
    
    /**
     * @brief 验证频域约束
     * @param frame_sequence 帧序列（至少3帧用于频域分析）
     * @param fps 帧率
     * @return 是否满足频域约束（1-6Hz能量增长<20%）
     */
    bool ValidateFrequencyConstraints(const std::vector<Image>& frame_sequence, float fps) const;
    
    /**
     * @brief 获取当前参数
     * @return 当前使用的参数
     */
    const CphParams& GetParams() const { return params_; }
    
    /**
     * @brief 获取最后的错误信息
     * @return 错误信息字符串
     */
    std::string GetLastError() const { return last_error_; }
    
    /**
     * @brief 重置处理器状态（清除运动历史等）
     */
    void Reset();

private:
    CphParams params_;
    std::string last_error_;
    bool initialized_ = false;
    
    // 运动保护相关
    Image previous_frame_;
    bool has_previous_frame_ = false;
    std::vector<float> motion_energy_history_;
    
    // USM算法实现
    bool ApplyUSM(const Image& input, Image& output, float pivot_threshold, float intensity);
    
    // 运动检测
    float ComputeMotionEnergy(const Image& current, const Image& previous, float pivot_threshold);
    bool ShouldSuppressDetail(float motion_energy);
    
    // 频域分析
    std::vector<float> ComputeTemporalSpectrum(const std::vector<Image>& frames, int x, int y) const;
    float ComputeEnergyInBand(const std::vector<float>& spectrum, float fps, float low_freq, float high_freq) const;
    
    // 辅助函数
    void ComputeGaussianKernel(std::vector<float>& kernel, int radius, float sigma);
    void ApplyGaussianBlur(const Image& input, Image& output, int radius, float sigma);
    void ComputeUnsharpMask(const Image& original, const Image& blurred, Image& mask, float amount, float threshold);
    
    // 数值保护
    void ClampImageValues(Image& image);
};

/**
 * @brief 高光细节处理工具函数
 */
namespace HighlightDetailUtils {
    
    /**
     * @brief 计算图像的高光区域掩码
     * @param image 输入图像
     * @param pivot_threshold 高光阈值
     * @param mask 输出掩码（0-1值）
     */
    void ComputeHighlightMask(const Image& image, float pivot_threshold, Image& mask);
    
    /**
     * @brief 应用掩码到图像
     * @param source 源图像
     * @param mask 掩码图像
     * @param target 目标图像
     * @param blend_mode 混合模式（0=替换，1=加法，2=乘法）
     */
    void ApplyMask(const Image& source, const Image& mask, Image& target, int blend_mode = 0);
    
    /**
     * @brief 计算帧间差分能量
     * @param frame1 第一帧
     * @param frame2 第二帧
     * @param region_mask 区域掩码（可选）
     * @return 归一化的差分能量
     */
    float ComputeFrameDifference(const Image& frame1, const Image& frame2, const Image* region_mask = nullptr);
    
    /**
     * @brief 简单的高斯模糊实现
     * @param input 输入图像
     * @param output 输出图像
     * @param radius 模糊半径（像素）
     * @param sigma 高斯标准差
     */
    void GaussianBlur(const Image& input, Image& output, int radius = 2, float sigma = 1.0f);
}

} // namespace CinemaProHDR