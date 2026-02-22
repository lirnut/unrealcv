# HUAWEI Matting Dataset Generator

Automated matting dataset generation using Unreal Engine 5.6 and UnrealCV.

## Installation

```bash
pip install -r requirements.txt
```

## Usage

```bash
# Generate 1 scene (test)
python run_matting_generation.py --scenes 1

# Generate 10 scenes
python run_matting_generation.py --scenes 10

# Connect to existing UE instance
python run_matting_generation.py --scenes 1 --skip-start
```

## Output

Generated datasets are saved to:
```
Windows/HUAWEI_Project/Saved/DatasetAutomationOutputDirectory/
└── scene_XXXX_XXXXXXXX/
    └── render_only/
        ├── rgb.mp4                    # RGB video
        ├── overview.json              # Scene metadata
        ├── metadata/*.json            # Per-frame camera data
        ├── mask/*.png                 # Segmentation masks
        └── oneobjlit/*.png            # Object-isolated lighting
```

## Dataset Specifications

- **Task**: Matting (hair/human matting with motion)
- **Frames**: 90 frames @ 30fps (~3 seconds)
- **Resolution**: Random 1080x1920 (portrait) or 1920x1080 (landscape)
- **FOV**: Random 40-55 degrees
- **Camera Distance**: 75-100cm (close-up)
- **Foreground Movement**: 70 cm/s @ 90° offset
- **Hair Physics**: Groom simulation with low damping/stiffness

## Documentation

See [CLAUDE.md](CLAUDE.md) for detailed architecture and development guide.
