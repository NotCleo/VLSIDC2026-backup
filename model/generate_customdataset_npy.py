import os
import numpy as np
import cv2
import random
import sys

"""
Modified calibration generator for your cardboard model.

- Reads ALL images from:
    /home/joeld/modelwork/training-dataset/training-defective
    /home/joeld/modelwork/training-dataset/training-undefective

- Resizes to your model input size (224x224)
- Converts BGR → RGB
- Applies normalization used in training
- Saves calibration batch: calibration_224x224x3.npy
"""

random_seed = 342
IMG_SIZE = 224  # from your training script

# Normalization used during training
MEAN = np.array([0.485, 0.456, 0.406], dtype=np.float32)
STD = np.array([0.229, 0.224, 0.225], dtype=np.float32)

# -------------------------------------------------------------------------
# Read all images from both defective + undefective
# -------------------------------------------------------------------------
def load_all_training_images(root_dir, max_count=200):
    random.seed(random_seed)

    subfolders = ["training-defective", "training-undefective"]

    all_files = []
    for sub in subfolders:
        full = os.path.join(root_dir, sub)
        files = [os.path.join(full, f) for f in os.listdir(full)
                 if f.lower().endswith((".jpg", ".jpeg", ".png"))]
        all_files.extend(files)

    # shuffle for good mix
    random.shuffle(all_files)

    # pick max_count images
    selected = all_files[:max_count]
    print(f"Loaded {len(selected)} images for calibration.")

    images = []
    for img_path in selected:
        img = cv2.imread(img_path)
        if img is None:
            continue
        img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
        images.append(img)

    return images


# -------------------------------------------------------------------------
# Preprocessing = resize + normalize
# -------------------------------------------------------------------------
def preprocess_images(images):
    out = np.zeros((len(images), IMG_SIZE, IMG_SIZE, 3), dtype=np.float32)

    for i, img in enumerate(images):
        resized = cv2.resize(img, (IMG_SIZE, IMG_SIZE),
                             interpolation=cv2.INTER_LINEAR).astype(np.float32)

        # Normalize exactly like training
        resized /= 255.0
        resized = (resized - MEAN) / STD

        out[i] = resized

    return out


# -------------------------------------------------------------------------
# Main
# -------------------------------------------------------------------------
def main():
    DATASET_ROOT = "/home/joeld/modelwork/training-dataset"
    OUTPUT_PATH = "/home/joeld/modelwork/calibdata/calibration_224x224x3.npy"

    print("Loading images...")
    imgs = load_all_training_images(DATASET_ROOT, max_count=200)

    print("Preprocessing images...")
    arr = preprocess_images(imgs)

    print("Saving calibration file...")
    np.save(OUTPUT_PATH, arr)

    print(f"\nCalibration file saved at:\n  {OUTPUT_PATH}")
    print("Shape:", arr.shape)


if __name__ == "__main__":
    main()
