import os, re
import shutil
from pathlib import Path

SOURCE_DIR = r"."
TARGET_DIR = r"I:\HUAWEI_Project_UE56_PKG_0318"
# TARGET_DIR = r"D:\codes\CitySample_PKG"
# TARGET_DIR = r"C:\Users\Administrator\Desktop\HillsideSampleProject_PKG"

platform = "Windows"

platform_files = os.listdir(os.path.join(TARGET_DIR , platform))
pattern_no_suffix = re.compile(r'^.+$')

PROJ = "not found"
for f in platform_files:
    if  pattern_no_suffix.match(f):
        fp = os.path.join(TARGET_DIR , platform , f)
        if os.path.isdir(fp) and (not f.startswith("Engine")):
            PROJ = f
            print("found proj", PROJ)
            break



SPECIAL_PATHS = {
    "genvid.py": f"{platform}/{f}/Saved",
    "MetaHumanCache.json": f"{platform}/{f}/Saved",
}

UnrealCV_Client_Path = "../../client/python/unrealcv"

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
    
    unrealcv_target = os.path.join(dst, "unrealcv")
    if os.path.exists(unrealcv_target):
        print("remove ", unrealcv_target)
        shutil.rmtree(unrealcv_target)
    
    shutil.copytree(UnrealCV_Client_Path, unrealcv_target)
    print(f"Copying {UnrealCV_Client_Path} -> {unrealcv_target}")

if __name__ == "__main__":
    print(f"Installing from {SOURCE_DIR} to {TARGET_DIR}")
    install_files(SOURCE_DIR, TARGET_DIR)
    print("Done!")
