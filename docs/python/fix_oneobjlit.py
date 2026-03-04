import os
import glob
import cv2
import numpy as np
from multiprocessing import Pool, cpu_count

def process_single_image(image_path):
    """
    高性能处理单张图片：使用cv2将BGR通道置0，保留Alpha通道（开启PNG压缩）
    :param image_path: 图片文件路径
    """
    try:
        # 以无损模式读取图片（IMREAD_UNCHANGED保留所有通道）
        img = cv2.imread(image_path, cv2.IMREAD_UNCHANGED)
        
        if img is None:
            print(f"❌ 无法读取图片: {image_path}")
            return
        
        # 检查是否是4通道图片（RGBA）
        if img.shape[2] == 4:
            # 核心操作：将BGR通道（0,1,2）全部置0，Alpha通道（3）保持不变
            img[:, :, :3] = 0
        else:
            # 如果是3通道图片，直接将所有通道置0（无Alpha通道）
            # img[:, :, :] = 0
            print(f"⚠️ 图片 {image_path} 无Alpha通道")
        
        # 保存图片（开启PNG压缩，平衡压缩率和速度）
        # PNG_COMPRESSION取值范围0-9：0=无压缩（最快），9=最高压缩（最慢）
        # 推荐值3-5：兼顾压缩率和处理速度，是生产环境常用配置
        cv2.imwrite(
            image_path, 
            img, 
            # [cv2.IMWRITE_PNG_COMPRESSION, 3]  # 开启压缩，级别3（可根据需求调整）
        )
        
        # 释放内存（性能敏感场景显式释放）
        del img
        print(f"✅ 处理完成: {image_path}")
        
    except Exception as e:
        print(f"❌ 处理失败 {image_path}: {str(e)}")

def main():
    # 1. 匹配所有目标图片
    image_pattern = "./scene_*/**/oneobjlit/*_oneobjlit.png"
    image_paths = glob.glob(image_pattern, recursive=True)
    
    if not image_paths:
        print("⚠️ 未找到符合条件的图片文件")
        return
    
    print(f"🔍 共找到 {len(image_paths)} 张待处理图片")
    
    # 2. 优化多进程配置（性能敏感场景）
    process_num = cpu_count()
    print(f"⚡ 使用 {process_num} 个进程进行处理（开启PNG压缩模式）")
    
    # 3. 批量处理图片（使用maxtasksperchild减少内存泄漏风险）
    with Pool(processes=process_num, maxtasksperchild=10) as pool:
        pool.map(process_single_image, image_paths)
    
    print("🎉 所有图片处理完成！")

if __name__ == "__main__":
    # 安装依赖（如果未安装）
    # pip install opencv-python numpy
    main()