import os
import shutil

SOURCE_DIR = r"G:\HUAWEI_Project_UE56\Plugins\unrealcv\docs\python"
TARGET_DIR = r"I:\HUAWEI_Project_UE56_PKG"

SPECIAL_PATHS = {
    "genvid.py": "Windows/HUAWEI_Project/Saved",
}

EXCLUDE_DIRS = {"__pycache__", ".git", ".pytest_cache"}
EXCLUDE_FILES = {".pyc", ".pyo", ".pyi"}

def install_files(src: str, dst: str):
    os.makedirs(dst, exist_ok=True)

    for item in os.listdir(src):
        if item in EXCLUDE_DIRS:
            continue

        src_path = os.path.join(src, item)

        if item in SPECIAL_PATHS:
            rel_path = SPECIAL_PATHS[item]
            if rel_path.startswith("./"):
                rel_path = rel_path[2:]
            dst_path = os.path.join(dst, rel_path, item)
        else:
            dst_path = os.path.join(dst, item)

        if os.path.isdir(src_path):
            install_files(src_path, dst_path)
        else:
            ext = os.path.splitext(item)[1].lower()
            if ext in EXCLUDE_FILES:
                continue

            if os.path.exists(dst_path):
                print(f"Removing {dst_path}")
                os.remove(dst_path)

            dst_dir = os.path.dirname(dst_path)
            if dst_dir and not os.path.exists(dst_dir):
                os.makedirs(dst_dir, exist_ok=True)

            print(f"Copying {src_path} -> {dst_path}")
            shutil.copy2(src_path, dst_path)

if __name__ == "__main__":
    print(f"Installing from {SOURCE_DIR} to {TARGET_DIR}")
    install_files(SOURCE_DIR, TARGET_DIR)
    print("Done!")
