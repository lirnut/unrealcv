import cv2
import numpy as np
from pathlib import Path



workdir = "./scene_0000_3C49CF9E/render_only/"
workdir = "G:/HUAWEI_Project_UE56/Saved/DatasetAutomationOutputDirectory/scene_0000_C365AB43/render_only/"


def apply_alpha_to_rgb():
    rgb_video = cv2.VideoCapture(workdir +'rgb.mp4')
    output_dir = Path(workdir +'rgb-alpha-rev')
    output_dir.mkdir(exist_ok=True)
    oneobjlit_dir = Path(workdir + 'oneobjlit')

    frame_idx = 0

    while True:
        ret, rgb_frame = rgb_video.read()
        if not ret:
            break

        alpha_png_path = oneobjlit_dir / f'{frame_idx}_oneobjlit.png'

        if alpha_png_path.exists():
            alpha_img = cv2.imread(str(alpha_png_path), cv2.IMREAD_UNCHANGED)

            if alpha_img.shape[2] == 4:
                alpha_channel = alpha_img[:, :, 3]
            else:
                alpha_channel = np.mean(alpha_img[:, :, :3], axis=2).astype(np.uint8)

            alpha_normalized = alpha_channel.astype(np.float32) / 255.0
            alpha_reversed = 1.0 - alpha_normalized
            alpha_enhanced = np.power(alpha_reversed, 0.5)

            rgb_float = rgb_frame.astype(np.float32)
            alpha_3ch = np.stack([alpha_enhanced] * 3, axis=2)

            result = rgb_float * alpha_3ch
            result = np.clip(result, 0, 255).astype(np.uint8)

            result_with_alpha = np.dstack([result, (alpha_enhanced * 255).astype(np.uint8)])

            output_path = output_dir / f'{frame_idx}.png'
            cv2.imwrite(str(output_path), result_with_alpha)

            if frame_idx % 10 == 0:
                print(f'Processed frame {frame_idx}')

        frame_idx += 1

    rgb_video.release()
    print(f'Done! Processed {frame_idx} frames to {output_dir}')

if __name__ == '__main__':
    apply_alpha_to_rgb()
