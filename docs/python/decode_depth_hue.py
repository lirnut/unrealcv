import cv2
import numpy as np
from pathlib import Path
import argparse
import re


def decode_hue_from_rgb(rgb_image: np.ndarray) -> np.ndarray:
    """
    Decode hue value from RGB image.
    Based on DecodeDepthFromHue in ImageUtil.cpp
    """
    r = rgb_image[:, :, 2].astype(np.int32)  # OpenCV uses BGR, so R is at index 2
    g = rgb_image[:, :, 1].astype(np.int32)
    b = rgb_image[:, :, 0].astype(np.int32)

    max_val = np.maximum(np.maximum(r, g), b)
    min_val = np.minimum(np.minimum(r, g), b)
    delta = max_val - min_val

    hue = np.zeros_like(r, dtype=np.float32)

    # MaxVal == MinVal -> Hue = 0 (already initialized)
    mask = delta > 0

    # MaxVal == R && G >= B
    mask_r = mask & (max_val == r) & (g >= b)
    hue[mask_r] = (g[mask_r] * 255.0) / delta[mask_r]

    # MaxVal == R && G < B
    mask_r2 = mask & (max_val == r) & (g < b)
    hue[mask_r2] = 1529.0 - (b[mask_r2] * 255.0) / delta[mask_r2]

    # MaxVal == G
    mask_g = mask & (max_val == g)
    hue[mask_g] = 510.0 + ((b[mask_g] - r[mask_g]) * 255.0) / delta[mask_g]

    # MaxVal == B
    mask_b = mask & (max_val == b)
    hue[mask_b] = 1020.0 + ((r[mask_b] - g[mask_b]) * 255.0) / delta[mask_b]

    hue = np.clip(hue, 0, 1529)
    return hue


def decode_depth_from_hue(
    hue: np.ndarray,
    min_depth: float,
    max_depth: float,
    inverse_mapping: bool = True,
    exponent: float = 0.2
) -> np.ndarray:
    """
    Decode depth from hue values.
    Based on DecodeDepthFromHue in ImageUtil.cpp
    """
    normalized = hue / 1529.0

    if inverse_mapping and exponent > 0:
        pow_max = max_depth ** (-exponent)
        pow_min = min_depth ** (-exponent)
        pow_depth = pow_max + normalized * (pow_min - pow_max)
        depth = pow_depth ** (-1.0 / exponent)
    else:
        depth = min_depth + normalized * (max_depth - min_depth)

    return depth


def decode_depth_image(
    image_path: Path,
    min_depth: float = 0.1,
    max_depth: float = 300.0,
    inverse_mapping: bool = True,
    exponent: float = 0.2
) -> np.ndarray:
    """
    Decode a hue-encoded depth image to metric depth (in meters).

    Args:
        image_path: Path to the encoded depth image (JPG/PNG)
        min_depth: Minimum depth value in meters (default: 0.1m = 10cm)
        max_depth: Maximum depth value in meters (default: 300m)
        inverse_mapping: Whether to use power-law inverse mapping
        exponent: Power-law exponent for inverse mapping (default: 0.2)

    Returns:
        Depth map in meters as numpy array (H, W)
    """
    rgb_image = cv2.imread(str(image_path))
    if rgb_image is None:
        raise ValueError(f"Failed to load image: {image_path}")

    hue = decode_hue_from_rgb(rgb_image)
    depth = decode_depth_from_hue(hue, min_depth, max_depth, inverse_mapping, exponent)

    return depth


def parse_filename_params(filename: str) -> dict:
    """
    Parse depth encoding parameters from filename.
    Example: 0_depthHueExp0p2Min10cmMax300m.jpg
    """
    params = {
        'exponent': 0.2,
        'min_depth': 0.1,
        'max_depth': 300.0,
        'inverse_mapping': True
    }

    # Try to parse exponent
    exp_match = re.search(r'Exp([0-9p]+)', filename)
    if exp_match:
        exp_str = exp_match.group(1).replace('p', '.')
        try:
            params['exponent'] = float(exp_str)
        except ValueError:
            pass

    # Try to parse min depth
    min_match = re.search(r'Min([0-9]+)(cm|m)', filename)
    if min_match:
        value = float(min_match.group(1))
        unit = min_match.group(2)
        params['min_depth'] = value / 100.0 if unit == 'cm' else value

    # Try to parse max depth
    max_match = re.search(r'Max([0-9]+)(cm|m)', filename)
    if max_match:
        value = float(max_match.group(1))
        unit = max_match.group(2)
        params['max_depth'] = value / 100.0 if unit == 'cm' else value

    return params


def visualize_depth(depth: np.ndarray, colormap: int = cv2.COLORMAP_TURBO) -> np.ndarray:
    """
    Create a visualization of depth map using OpenCV colormap.
    """
    valid_mask = depth > 0
    min_val = depth[valid_mask].min() if valid_mask.any() else 0
    max_val = depth[valid_mask].max() if valid_mask.any() else 1

    normalized = np.zeros_like(depth, dtype=np.uint8)
    if max_val > min_val:
        normalized = ((depth - min_val) / (max_val - min_val) * 255).clip(0, 255).astype(np.uint8)

    colored = cv2.applyColorMap(normalized, colormap)
    return colored


def main():
    parser = argparse.ArgumentParser(description='Decode hue-encoded depth images')
    parser.add_argument('input', type=str, help='Input image path or directory')
    parser.add_argument('--output', '-o', type=str, help='Output directory (default: same as input)')
    parser.add_argument('--min-depth', type=float, default=None, help='Minimum depth in meters')
    parser.add_argument('--max-depth', type=float, default=None, help='Maximum depth in meters')
    parser.add_argument('--exponent', type=float, default=None, help='Power-law exponent')
    parser.add_argument('--no-inverse', action='store_true', help='Disable inverse mapping')
    parser.add_argument('--visualize', '-v', action='store_true', help='Create visualization images')
    parser.add_argument('--save-npy', action='store_true', help='Save as numpy array')
    parser.add_argument('--save-exr', action='store_true', help='Save as EXR (32-bit float)')

    args = parser.parse_args()

    input_path = Path(args.input)

    if input_path.is_file():
        image_files = [input_path]
        output_dir = Path(args.output) if args.output else input_path.parent
    else:
        image_files = sorted(input_path.glob('*.jpg')) + sorted(input_path.glob('*.png'))
        output_dir = Path(args.output) if args.output else input_path

    output_dir.mkdir(parents=True, exist_ok=True)

    for img_path in image_files:
        print(f"Processing: {img_path.name}")

        params = parse_filename_params(img_path.name)

        if args.min_depth is not None:
            params['min_depth'] = args.min_depth
        if args.max_depth is not None:
            params['max_depth'] = args.max_depth
        if args.exponent is not None:
            params['exponent'] = args.exponent
        if args.no_inverse:
            params['inverse_mapping'] = False

        print(f"  Parameters: min={params['min_depth']}m, max={params['max_depth']}m, "
              f"exp={params['exponent']}, inverse={params['inverse_mapping']}")

        depth = decode_depth_image(
            img_path,
            min_depth=params['min_depth'],
            max_depth=params['max_depth'],
            inverse_mapping=params['inverse_mapping'],
            exponent=params['exponent']
        )

        base_name = img_path.stem.replace('_depthHueExp0p2Min10cmMax300m', '').replace('_depth', '')

        if args.save_npy:
            npy_path = output_dir / f"{base_name}_depth.npy"
            np.save(npy_path, depth.astype(np.float32))
            print(f"  Saved: {npy_path}")

        if args.save_exr:
            exr_path = output_dir / f"{base_name}_depth.exr"
            cv2.imwrite(str(exr_path), depth.astype(np.float32))
            print(f"  Saved: {exr_path}")

        if args.visualize:
            vis = visualize_depth(depth)
            vis_path = output_dir / f"{base_name}_depth_vis.png"
            cv2.imwrite(str(vis_path), vis)
            print(f"  Saved: {vis_path}")

        print(f"  Depth range: [{depth.min():.2f}m, {depth.max():.2f}m]")


if __name__ == '__main__':
    main()
