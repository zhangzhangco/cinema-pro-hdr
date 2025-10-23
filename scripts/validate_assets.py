#!/usr/bin/env python3
"""
Cinema Pro HDR 资产验证脚本
验证生成的测试资产的完整性和正确性
"""

import json
import pandas as pd
import numpy as np
from pathlib import Path
import argparse
import sys

def validate_cliplist(cliplist_path: Path) -> bool:
    """验证样片清单"""
    print(f"🔍 验证样片清单: {cliplist_path}")
    
    try:
        with open(cliplist_path, 'r', encoding='utf-8') as f:
            cliplist = json.load(f)
    except Exception as e:
        print(f"❌ 无法读取样片清单: {e}")
        return False
    
    # 检查必需字段
    required_fields = ['version', 'clips']
    for field in required_fields:
        if field not in cliplist:
            print(f"❌ 缺少必需字段: {field}")
            return False
    
    clips = cliplist['clips']
    if len(clips) != 12:
        print(f"❌ 样片数量错误: 期望12个，实际{len(clips)}个")
        return False
    
    # 检查场景类型分布
    scene_types = {}
    for clip in clips:
        scene_type = clip.get('type', 'unknown')
        scene_types[scene_type] = scene_types.get(scene_type, 0) + 1
    
    expected_types = {
        'night_scene': 2,
        'backlit_portrait': 2, 
        'neon': 2,
        'mist': 2,
        'highlight_peak': 2,
        'skin_tone': 2
    }
    
    for scene_type, expected_count in expected_types.items():
        actual_count = scene_types.get(scene_type, 0)
        if actual_count != expected_count:
            print(f"❌ 场景类型 {scene_type} 数量错误: 期望{expected_count}个，实际{actual_count}个")
            return False
    
    # 检查每个样片的必需字段
    required_clip_fields = ['id', 'name', 'type', 'duration_seconds', 'resolution', 'framerate']
    for i, clip in enumerate(clips):
        for field in required_clip_fields:
            if field not in clip:
                print(f"❌ 样片 {i+1} 缺少字段: {field}")
                return False
        
        # 检查规格
        if clip['duration_seconds'] != 12:
            print(f"❌ 样片 {clip['id']} 时长错误: 期望12秒，实际{clip['duration_seconds']}秒")
            return False
        
        if clip['resolution'] != '4096x2160':
            print(f"❌ 样片 {clip['id']} 分辨率错误: 期望4096x2160，实际{clip['resolution']}")
            return False
        
        if clip['framerate'] != 24:
            print(f"❌ 样片 {clip['id']} 帧率错误: 期望24fps，实际{clip['framerate']}fps")
            return False
    
    print(f"✅ 样片清单验证通过: {len(clips)}个样片，6种场景类型")
    return True

def validate_curves(curves_path: Path) -> bool:
    """验证RefMath曲线数据"""
    print(f"🔍 验证RefMath曲线: {curves_path}")
    
    try:
        curves_df = pd.read_csv(curves_path)
    except Exception as e:
        print(f"❌ 无法读取曲线数据: {e}")
        return False
    
    # 检查必需列
    required_columns = ['preset', 'curve_type', 'sample_index', 'x', 'y', 'monotonic', 'c1_continuous']
    for col in required_columns:
        if col not in curves_df.columns:
            print(f"❌ 缺少必需列: {col}")
            return False
    
    # 检查预设和曲线类型
    expected_presets = {'cinema_flat', 'cinema_punch', 'cinema_highlight'}
    expected_curves = {'PPR', 'RLOG'}
    
    actual_presets = set(curves_df['preset'].unique())
    actual_curves = set(curves_df['curve_type'].unique())
    
    if actual_presets != expected_presets:
        print(f"❌ 预设类型错误: 期望{expected_presets}，实际{actual_presets}")
        return False
    
    if actual_curves != expected_curves:
        print(f"❌ 曲线类型错误: 期望{expected_curves}，实际{actual_curves}")
        return False
    
    # 检查每个预设-曲线组合的数据完整性
    total_combinations = len(expected_presets) * len(expected_curves)
    actual_combinations = len(curves_df.groupby(['preset', 'curve_type']))
    
    if actual_combinations != total_combinations:
        print(f"❌ 预设-曲线组合数量错误: 期望{total_combinations}个，实际{actual_combinations}个")
        return False
    
    # 检查采样点数
    for (preset, curve_type), group in curves_df.groupby(['preset', 'curve_type']):
        sample_count = len(group)
        if sample_count != 16384:
            print(f"❌ {preset}-{curve_type} 采样点数错误: 期望16384个，实际{sample_count}个")
            return False
        
        # 检查x值范围和单调性
        x_values = group['x'].values
        if not (np.allclose(x_values[0], 0.0) and np.allclose(x_values[-1], 1.0)):
            print(f"❌ {preset}-{curve_type} x值范围错误: 期望[0,1]，实际[{x_values[0]:.6f},{x_values[-1]:.6f}]")
            return False
        
        if not np.all(np.diff(x_values) > 0):
            print(f"❌ {preset}-{curve_type} x值不是严格递增")
            return False
        
        # 检查y值范围
        y_values = group['y'].values
        if np.any(y_values < 0) or np.any(y_values > 1.1):  # 允许轻微超出1.0
            print(f"❌ {preset}-{curve_type} y值超出合理范围: [{y_values.min():.6f},{y_values.max():.6f}]")
            return False
        
        # 检查单调性标记
        monotonic_flag = group['monotonic'].iloc[0]
        actual_monotonic = np.all(np.diff(y_values) >= -1e-10)  # 允许数值误差
        
        if monotonic_flag != actual_monotonic:
            print(f"❌ {preset}-{curve_type} 单调性标记错误: 标记为{monotonic_flag}，实际为{actual_monotonic}")
            return False
    
    print(f"✅ RefMath曲线验证通过: {len(curves_df)}条记录，{total_combinations}个预设-曲线组合")
    return True

def validate_reference_data(reference_path: Path) -> bool:
    """验证参考数据JSON"""
    print(f"🔍 验证参考数据: {reference_path}")
    
    try:
        with open(reference_path, 'r', encoding='utf-8') as f:
            ref_data = json.load(f)
    except Exception as e:
        print(f"❌ 无法读取参考数据: {e}")
        return False
    
    # 检查顶层字段
    required_fields = ['version', 'generator', 'precision', 'sample_count', 'presets', 'curves', 'validation']
    for field in required_fields:
        if field not in ref_data:
            print(f"❌ 缺少必需字段: {field}")
            return False
    
    # 检查精度和采样数
    if ref_data['precision'] != 'float64':
        print(f"❌ 精度错误: 期望float64，实际{ref_data['precision']}")
        return False
    
    if ref_data['sample_count'] != 16384:
        print(f"❌ 采样数错误: 期望16384，实际{ref_data['sample_count']}")
        return False
    
    # 检查预设数量
    presets = ref_data['presets']
    if len(presets) != 3:
        print(f"❌ 预设数量错误: 期望3个，实际{len(presets)}个")
        return False
    
    expected_preset_names = {'cinema_flat', 'cinema_punch', 'cinema_highlight'}
    actual_preset_names = set(presets.keys())
    
    if actual_preset_names != expected_preset_names:
        print(f"❌ 预设名称错误: 期望{expected_preset_names}，实际{actual_preset_names}")
        return False
    
    # 检查每个预设的参数
    for preset_name, preset_data in presets.items():
        required_preset_fields = ['name', 'ppr', 'rlog', 'saturation']
        for field in required_preset_fields:
            if field not in preset_data:
                print(f"❌ 预设 {preset_name} 缺少字段: {field}")
                return False
        
        # 检查PPR参数
        ppr_params = preset_data['ppr']
        required_ppr_fields = ['pivot', 'gamma_s', 'gamma_h', 'shoulder']
        for field in required_ppr_fields:
            if field not in ppr_params:
                print(f"❌ 预设 {preset_name} PPR缺少参数: {field}")
                return False
        
        # 检查参数范围
        if not (0.05 <= ppr_params['pivot'] <= 0.30):
            print(f"❌ 预设 {preset_name} pivot超出范围: {ppr_params['pivot']}")
            return False
    
    # 检查曲线数据
    curves = ref_data['curves']
    if len(curves) != 6:  # 3预设 × 2曲线
        print(f"❌ 曲线数据数量错误: 期望6个，实际{len(curves)}个")
        return False
    
    # 检查验证结果
    validation = ref_data['validation']
    if not validation.get('all_monotonic', False):
        print(f"❌ 存在非单调曲线")
        return False
    
    if not validation.get('all_c1_continuous', False):
        print(f"❌ 存在非C¹连续曲线")
        return False
    
    print(f"✅ 参考数据验证通过: 3个预设，6条曲线，验证通过")
    return True

def validate_frames_metadata(frames_meta_path: Path) -> bool:
    """验证参考帧元数据"""
    print(f"🔍 验证参考帧元数据: {frames_meta_path}")
    
    try:
        with open(frames_meta_path, 'r', encoding='utf-8') as f:
            frames_meta = json.load(f)
    except Exception as e:
        print(f"❌ 无法读取参考帧元数据: {e}")
        return False
    
    # 检查必需字段
    required_fields = ['version', 'description', 'frames']
    for field in required_fields:
        if field not in frames_meta:
            print(f"❌ 缺少必需字段: {field}")
            return False
    
    frames = frames_meta['frames']
    expected_frame_count = 3 * 4  # 3预设 × 4样片
    
    if len(frames) != expected_frame_count:
        print(f"❌ 参考帧数量错误: 期望{expected_frame_count}个，实际{len(frames)}个")
        return False
    
    # 检查每个帧的必需字段
    required_frame_fields = ['preset', 'clip_id', 'filename', 'description', 'expected_stats']
    for i, frame in enumerate(frames):
        for field in required_frame_fields:
            if field not in frame:
                print(f"❌ 参考帧 {i+1} 缺少字段: {field}")
                return False
    
    print(f"✅ 参考帧元数据验证通过: {len(frames)}个参考帧")
    return True

def main():
    parser = argparse.ArgumentParser(description='验证Cinema Pro HDR测试资产')
    parser.add_argument('--assets-dir', default='.', help='资产根目录')
    args = parser.parse_args()
    
    assets_dir = Path(args.assets_dir)
    
    print("🚀 开始验证Cinema Pro HDR测试资产...")
    print(f"📁 资产目录: {assets_dir.absolute()}")
    print()
    
    all_passed = True
    
    # 验证样片清单
    cliplist_path = assets_dir / 'assets' / 'cliplist.json'
    if cliplist_path.exists():
        all_passed &= validate_cliplist(cliplist_path)
    else:
        print(f"❌ 样片清单不存在: {cliplist_path}")
        all_passed = False
    
    print()
    
    # 验证RefMath曲线
    curves_path = assets_dir / 'golden' / 'curves.csv'
    if curves_path.exists():
        all_passed &= validate_curves(curves_path)
    else:
        print(f"❌ RefMath曲线不存在: {curves_path}")
        all_passed = False
    
    print()
    
    # 验证参考数据
    reference_path = assets_dir / 'golden' / 'reference_data.json'
    if reference_path.exists():
        all_passed &= validate_reference_data(reference_path)
    else:
        print(f"❌ 参考数据不存在: {reference_path}")
        all_passed = False
    
    print()
    
    # 验证参考帧元数据
    frames_meta_path = assets_dir / 'golden' / 'frames' / 'metadata.json'
    if frames_meta_path.exists():
        all_passed &= validate_frames_metadata(frames_meta_path)
    else:
        print(f"❌ 参考帧元数据不存在: {frames_meta_path}")
        all_passed = False
    
    print()
    print("=" * 60)
    
    if all_passed:
        print("🎉 所有资产验证通过！")
        print("✅ 资产已准备就绪，可用于CI视觉回归测试")
        sys.exit(0)
    else:
        print("💥 资产验证失败！")
        print("❌ 请修复上述问题后重新验证")
        sys.exit(1)

if __name__ == '__main__':
    main()