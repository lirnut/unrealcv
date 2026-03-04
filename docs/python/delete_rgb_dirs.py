import os
import shutil
from pathlib import Path

dataset_dir = Path(".")

for item in dataset_dir.glob("*/*/rgb"):
    if item.is_dir():
        print(f"Deleting directory: {item}")
        shutil.rmtree(item)
    else:
        print(f"Skipping (not a directory): {item}")

print("Done!")
