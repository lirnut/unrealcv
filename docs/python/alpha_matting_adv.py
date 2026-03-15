"""
Advanced Alpha Matting with Brightness-Aware Remapping

This script processes alpha masks for hair/fur semi-transparent regions,
reducing background color contamination in bright areas.

Problem: When background is bright (white/light), hair edges appear to
"disappear" due to color blending - causing visual alpha mask inaccuracy.

Solution: Use pixel brightness to detect background contamination and
selectively reduce alpha in affected regions.

Input Format (IMPORTANT):
    The input alpha mask is INVERTED:
    - alpha=255 means background (to be removed)
    - alpha=0 means foreground/person (to be kept)

    This script automatically inverts to standard convention during processing:
    - alpha=1 = foreground (opaque, keep)
    - alpha=0 = background (transparent, remove)

Output Format:
    Standard PNG alpha format:
    - alpha=255 = fully opaque (foreground, keep)
    - alpha=0 = fully transparent (background, remove)

Usage:
    python alpha_matting_adv.py <working_directory>

Parameters (adjust at top of script):
    - BRIGHTNESS_THRESHOLD: Pixels brighter than this are considered contaminated
    - BRIGHTNESS_TARGET: Alpha reduction completes at this brightness
    - ALPHA_MIN: Minimum alpha after reduction (0 = fully transparent)
    - ALPHA_FALLOFF: Steepness of alpha reduction curve
"""

import cv2
import sys
import os
import numpy as np
from pathlib import Path

# =====================
# Configurable Parameters
# =====================
BRIGHTNESS_THRESHOLD = 0.25   # Brightness threshold (0-1), pixels above this are treated as contaminated
BRIGHTNESS_TARGET = 0.45      # Target brightness where alpha reduction is maximal
ALPHA_MIN = 0.05               # Minimum alpha after reduction (0=transparent, 0.1=slight edge)
ALPHA_FALLOFF = 2.0           # Alpha falloff curve steepness (higher = more abrupt)

# Output modes
OUTPUT_MODE = 'premultiplied'  # 'premultiplied': RGB * alpha, 'original': keep original RGB


def remap_alpha_by_brightness(alpha_norm: np.ndarray, brightness: np.ndarray) -> np.ndarray:
    """
    Remap alpha values based on pixel brightness.

    Logic:
    - alpha ~= 1 (solid): Keep unchanged
    - alpha ~= 0 (fully transparent): Keep unchanged
    - 0 < alpha < 1 (semi-transparent hair edges):
        - If brightness is HIGH -> background contamination -> reduce alpha
        - If brightness is LOW -> actual hair color -> preserve alpha

    Args:
        alpha_norm: Normalized alpha channel (0-1 float)
        brightness: Per-pixel brightness (0-1 float)

    Returns:
        Remapped alpha channel (0-1 float)
    """
    alpha_out = alpha_norm.copy()

    # Create mask for semi-transparent regions (hair edges)
    semi_transparent_mask = (alpha_norm > 0.01) & (alpha_norm < 0.99)

    if not np.any(semi_transparent_mask):
        return alpha_out

    # # Calculate how much each pixel exceeds the brightness threshold
    # # 0 = at threshold, 1 = at target brightness (full reduction)
    # brightness_excess = np.clip(
    #     (brightness - BRIGHTNESS_THRESHOLD) / (BRIGHTNESS_TARGET - BRIGHTNESS_THRESHOLD),
    #     0.0, 1.0
    # )
    max_brightness = np.max(brightness[semi_transparent_mask])
    print(f"Max brightness: {max_brightness:.3f}")
    min_brightness = np.min(brightness[semi_transparent_mask])
    print(f"Min brightness: {min_brightness:.3f}")
    # brightness_low = max(min_brightness, 0.2)
    # brightness_high = min(max_brightness, 0.4)
    brightness_low = min(max_brightness, 0.2) - 0.01
    brightness_high = (max_brightness + min_brightness) / 2
    brightness_excess = np.clip(
        (brightness - brightness_low) / (brightness_high - brightness_low),
        0.0, 1.0
    )

    # Apply reduction only to semi-transparent regions
    alpha_out[semi_transparent_mask] = np.minimum(
        alpha_out[semi_transparent_mask],
        np.maximum(
            alpha_out[semi_transparent_mask] * (1 - brightness_excess[semi_transparent_mask]),
            ALPHA_MIN
        )
    )

    return alpha_out


def apply_alpha_to_rgb():
    """
    Main processing function.
    Reads rgb.mp4 and oneobjlit PNGs, applies brightness-aware alpha remapping,
    outputs processed frames with corrected alpha masks.
    """
    workdir = os.path.realpath(sys.argv[1])

    # Find first subdirectory in workdir
    entries = [e for e in os.listdir(workdir) if os.path.isdir(os.path.join(workdir, e))]
    if not entries:
        print(f"Error: No subdirectory found in {workdir}")
        sys.exit(1)

    workdir = os.path.join(workdir, entries[0]) + "/"
    print(f"Working directory: {workdir}")

    rgb_video = cv2.VideoCapture(workdir + 'rgb.mp4')
    output_dir = Path(workdir + 'rgb-alpha-adv')
    output_dir.mkdir(exist_ok=True)
    oneobjlit_dir = Path(workdir + 'oneobjlit')

    if not rgb_video.isOpened():
        print(f"Error: Cannot open {workdir}rgb.mp4")
        sys.exit(1)

    if not oneobjlit_dir.exists():
        print(f"Error: {oneobjlit_dir} does not exist")
        sys.exit(1)

    total_frames = int(rgb_video.get(cv2.CAP_PROP_FRAME_COUNT))
    frame_idx = 0
    processed_frames = 0
    alpha_stats = {'min': 255, 'max': 0, 'sum': 0}

    print(f"Processing {total_frames} frames...")
    print(f"Parameters: brightness_threshold={BRIGHTNESS_THRESHOLD}, "
          f"brightness_target={BRIGHTNESS_TARGET}, alpha_min={ALPHA_MIN}, "
          f"alpha_falloff={ALPHA_FALLOFF}")

    while True:
        ret, rgb_frame = rgb_video.read()
        if not ret:
            break

        alpha_png_path = oneobjlit_dir / f'{frame_idx}_oneobjlit.png'

        if alpha_png_path.exists():
            alpha_img = cv2.imread(str(alpha_png_path), cv2.IMREAD_UNCHANGED)

            # Extract original alpha channel
            if alpha_img.ndim == 3 and alpha_img.shape[2] == 4:
                alpha_channel = alpha_img[:, :, 3]
            else:
                # Fallback: compute alpha from RGB mean
                alpha_channel = np.mean(alpha_img[:, :, :3], axis=2).astype(np.uint8)

            # Normalize alpha to 0-1 float
            alpha_norm = alpha_channel.astype(np.float32) / 255.0

            # IMPORTANT: In this dataset, alpha is INVERTED:
            #   alpha=1 means background (transparent/to be removed)
            #   alpha=0 means foreground (opaque/person to keep)
            # Invert to standard convention: alpha=1 = foreground, alpha=0 = background
            alpha_norm = 1.0 - alpha_norm

            # Calculate brightness from RGB frame
            rgb_gray = cv2.cvtColor(rgb_frame, cv2.COLOR_BGR2GRAY)
            brightness = rgb_gray.astype(np.float32) / 255.0

            # Apply brightness-aware alpha remapping
            alpha_remapped = remap_alpha_by_brightness(alpha_norm, brightness)

            # Optional: Apply mild gamma correction for smooth edges
            # This makes the transition more natural
            alpha_remapped = np.power(alpha_remapped, 0.85)

            # Convert back to 0-255
            alpha_remapped = np.clip(alpha_remapped, 0.0, 1.0)
            alpha_out = (alpha_remapped * 255).astype(np.uint8)

            # Update statistics
            alpha_stats['min'] = min(alpha_stats['min'], alpha_out.min())
            alpha_stats['max'] = max(alpha_stats['max'], alpha_out.max())
            alpha_stats['sum'] += alpha_out.sum()

            # Apply alpha to RGB
            alpha_3ch = np.stack([alpha_remapped] * 3, axis=2)

            if OUTPUT_MODE == 'premultiplied':
                result = (rgb_frame.astype(np.float32) * alpha_3ch)
                result = np.clip(result, 0, 255).astype(np.uint8)
            else:
                result = rgb_frame

            # Composite with alpha channel
            result_with_alpha = np.dstack([result, alpha_out])

            output_path = output_dir / f'{frame_idx}.png'
            cv2.imwrite(str(output_path), result_with_alpha)

            processed_frames += 1

            if frame_idx % 30 == 0:
                avg_alpha = alpha_out.mean()
                print(f"Frame {frame_idx}/{total_frames} | "
                      f"Alpha range: [{alpha_out.min():3d}, {alpha_out.max():3d}] | "
                      f"Mean: {avg_alpha:.1f}")

        frame_idx += 1

    rgb_video.release()

    # Final statistics
    avg_alpha_overall = alpha_stats['sum'] / (processed_frames * 256 * 512) if processed_frames > 0 else 0
    print(f"\n{'='*50}")
    print(f"Done! Processed {processed_frames}/{total_frames} frames")
    print(f"Output directory: {output_dir}")
    print(f"Alpha statistics:")
    print(f"  Min: {alpha_stats['min']}")
    print(f"  Max: {alpha_stats['max']}")
    print(f"  Avg: {avg_alpha_overall:.1f}")
    print(f"{'='*50}")


def generate_debug_visualization():
    """
    Generate debug visualization showing brightness vs alpha relationship.
    Creates scatter plot and histogram for quality analysis.
    """
    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib not available, skipping debug visualization")
        return

    workdir = os.path.realpath(sys.argv[1])
    entries = [e for e in os.listdir(workdir) if os.path.isdir(os.path.join(workdir, e))]
    if not entries:
        return

    workdir = os.path.join(workdir, entries[0]) + "/"

    # Load a sample frame
    rgb_video = cv2.VideoCapture(workdir + 'rgb.mp4')
    oneobjlit_dir = Path(workdir + 'oneobjlit')

    # Get frame 10 for analysis
    sample_frame = 10
    rgb_video.set(cv2.CAP_PROP_POS_FRAMES, sample_frame)
    ret, rgb_frame = rgb_video.read()

    if not ret:
        rgb_video.release()
        return

    alpha_png_path = oneobjlit_dir / f'{sample_frame}_oneobjlit.png'
    if not alpha_png_path.exists():
        rgb_video.release()
        return

    alpha_img = cv2.imread(str(alpha_png_path), cv2.IMREAD_UNCHANGED)
    alpha_channel = alpha_img[:, :, 3] if alpha_img.shape[2] == 4 else np.mean(alpha_img[:, :, :3], axis=2)
    alpha_norm = alpha_channel.astype(np.float32) / 255.0

    # Invert to standard convention: alpha=1 = foreground, alpha=0 = background
    alpha_norm = 1.0 - alpha_norm

    brightness = cv2.cvtColor(rgb_frame, cv2.COLOR_BGR2GRAY).astype(np.float32) / 255.0
    print(f"Brightness range: [{brightness.min():.3f}, {brightness.max():.3f}]")
    print(f"Brightness mean: {brightness.mean():.3f}")
    print(f"Brightness std: {brightness.std():.3f}, "
          f"Median: {np.median(brightness):.3f}")
    alpha_remapped = remap_alpha_by_brightness(alpha_norm, brightness)

    # Create figure with 4 subplots
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))

    # 1. Original RGB frame
    axes[0, 0].imshow(cv2.cvtColor(rgb_frame, cv2.COLOR_BGR2RGB))
    axes[0, 0].set_title(f'Frame {sample_frame}: RGB Input')
    axes[0, 0].axis('off')

    # 2. Original alpha mask
    im = axes[0, 1].imshow(alpha_norm, cmap='gray', vmin=0, vmax=1)
    axes[0, 1].set_title('Original Alpha Mask')
    axes[0, 1].axis('off')
    plt.colorbar(im, ax=axes[0, 1])

    # 3. Brightness vs Alpha scatter (sampled)
    sample_indices = np.random.choice(len(brightness.ravel()), min(10000, len(brightness.ravel())), replace=False)
    brightness_flat = brightness.ravel()[sample_indices]
    alpha_orig_flat = alpha_norm.ravel()[sample_indices]
    alpha_new_flat = alpha_remapped.ravel()[sample_indices]

    # Color by alpha region
    edge_mask = (alpha_orig_flat > 0.01) & (alpha_orig_flat < 0.99)

    axes[1, 0].scatter(brightness_flat[~edge_mask], alpha_orig_flat[~edge_mask],
                       c='blue', s=1, alpha=0.3, label='Solid Background')
    axes[1, 0].scatter(brightness_flat[edge_mask], alpha_orig_flat[edge_mask],
                       c='orange', s=1, alpha=0.5, label='Hair Edge (Original)')
    axes[1, 0].scatter(brightness_flat[edge_mask], alpha_new_flat[edge_mask],
                       c='red', s=1, alpha=0.5, label='Hair Edge (Remapped)', marker='x')
    axes[1, 0].axhline(y=BRIGHTNESS_THRESHOLD, color='green', linestyle='--',
                       label=f'Brightness Threshold ({BRIGHTNESS_THRESHOLD})')
    axes[1, 0].set_xlabel('Pixel Brightness')
    axes[1, 0].set_ylabel('Alpha (1=Foreground, 0=Background)')
    axes[1, 0].set_title('Brightness vs Alpha Relationship')
    axes[1, 0].legend(loc='upper right', fontsize=8)
    axes[1, 0].set_xlim(0, 1)
    axes[1, 0].set_ylim(0, 1)

    # 4. Alpha histogram comparison
    axes[1, 1].hist(alpha_orig_flat[edge_mask], bins=50, alpha=0.6,
                    label='Original Edge Alpha', color='orange')
    axes[1, 1].hist(alpha_new_flat[edge_mask], bins=50, alpha=0.6,
                    label='Remapped Edge Alpha', color='red')
    axes[1, 1].set_xlabel('Alpha Value')
    axes[1, 1].set_ylabel('Pixel Count')
    axes[1, 1].set_title('Alpha Distribution (Hair Edge Pixels Only)')
    axes[1, 1].legend()

    plt.tight_layout()
    debug_path = Path(workdir + 'alpha_matting_debug.png')
    plt.savefig(debug_path, dpi=150, bbox_inches='tight')
    plt.close()

    print(f"Debug visualization saved to: {debug_path}")

    rgb_video.release()


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python alpha_matting_adv.py <working_directory>")
        print("\nThis script processes alpha masks for hair/fur semi-transparent regions,")
        print("reducing background color contamination in bright areas.")
        print("\nSee script header for configurable parameters.")
        sys.exit(1)

    apply_alpha_to_rgb()
    generate_debug_visualization()
