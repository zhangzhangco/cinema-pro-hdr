#!/usr/bin/env python3
"""
Compute correct color transformation matrices for Cinema Pro HDR
"""

import numpy as np

def compute_bt2020_to_p3d65_matrix():
    """Compute BT.2020 to P3-D65 transformation matrix"""
    
    # BT.2020 primaries (ITU-R BT.2020)
    bt2020_primaries = np.array([
        [0.7080, 0.2920, 0.0000],  # Red
        [0.1700, 0.7970, 0.0330],  # Green  
        [0.1310, 0.0460, 0.8230]   # Blue
    ]).T
    
    # P3-D65 primaries (DCI-P3 with D65 white point)
    p3d65_primaries = np.array([
        [0.6800, 0.3200, 0.0000],  # Red
        [0.2650, 0.6900, 0.0450],  # Green
        [0.1500, 0.0600, 0.7950]   # Blue
    ]).T
    
    # D65 white point
    d65_white = np.array([0.3127, 0.3290, 0.3583])
    
    # Compute transformation matrix
    # This is a simplified approach - in practice you'd use proper chromatic adaptation
    bt2020_to_xyz = np.array([
        [0.6369580, 0.1446169, 0.1688809],
        [0.2627045, 0.6779981, 0.0593017],
        [0.0000000, 0.0280727, 1.0609851]
    ])
    
    xyz_to_p3d65 = np.array([
        [ 2.4934969, -0.9313836, -0.4027108],
        [-0.8294890,  1.7626641,  0.0236247],
        [ 0.0358458, -0.0761724,  0.9568845]
    ])
    
    # BT.2020 to P3-D65 = XYZ_to_P3D65 * BT2020_to_XYZ
    bt2020_to_p3d65 = xyz_to_p3d65 @ bt2020_to_xyz
    
    # Compute inverse
    p3d65_to_bt2020 = np.linalg.inv(bt2020_to_p3d65)
    
    print("BT2020_TO_P3D65_MATRIX:")
    for i in range(3):
        print(f"    {bt2020_to_p3d65[i,0]:9.6f}f, {bt2020_to_p3d65[i,1]:9.6f}f, {bt2020_to_p3d65[i,2]:9.6f}f,")
    
    print("\nP3D65_TO_BT2020_MATRIX:")
    for i in range(3):
        print(f"    {p3d65_to_bt2020[i,0]:9.6f}f, {p3d65_to_bt2020[i,1]:9.6f}f, {p3d65_to_bt2020[i,2]:9.6f}f,")
    
    # Test round-trip
    identity = bt2020_to_p3d65 @ p3d65_to_bt2020
    print(f"\nRound-trip error (should be close to identity):")
    print(identity)
    print(f"Max error: {np.max(np.abs(identity - np.eye(3)))}")

def compute_acesg_matrices():
    """Compute ACEScg transformation matrices"""
    
    # Use simpler identity-like matrices for ACEScg for now
    # In practice, these would be computed from AP1 primaries
    bt2020_to_acesg = np.eye(3)  # Placeholder
    acesg_to_bt2020 = np.eye(3)  # Placeholder
    
    print("\nBT2020_TO_ACESG_MATRIX (placeholder):")
    for i in range(3):
        print(f"    {bt2020_to_acesg[i,0]:9.6f}f, {bt2020_to_acesg[i,1]:9.6f}f, {bt2020_to_acesg[i,2]:9.6f}f,")
    
    print("\nACESG_TO_BT2020_MATRIX (placeholder):")
    for i in range(3):
        print(f"    {acesg_to_bt2020[i,0]:9.6f}f, {acesg_to_bt2020[i,1]:9.6f}f, {acesg_to_bt2020[i,2]:9.6f}f,")

if __name__ == "__main__":
    compute_bt2020_to_p3d65_matrix()
    compute_acesg_matrices()