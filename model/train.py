import os
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import DataLoader
from torchvision import transforms, datasets, models

# ------------------------------------
# CONFIG
# ------------------------------------
TRAIN_DIR = "./training-dataset"
VAL_DIR = "./validate-dataset"

BATCH_SIZE = 32
EPOCHS = 30
LR = 1e-3
IMG_SIZE = 224

PTH_PATH = "model_cardboard.pth"
ONNX_PATH = "model_cardboard.onnx"

device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
print("Using device:", device)

# ------------------------------------
# TRANSFORMS (RGB)
# ------------------------------------
print("Building RGB transforms...")

train_t = transforms.Compose([
    transforms.Resize((IMG_SIZE, IMG_SIZE)),
    transforms.RandomHorizontalFlip(),
    transforms.RandomRotation(5),
    transforms.ToTensor(),
    transforms.Normalize(mean=[0.485, 0.456, 0.406],
                         std=[0.229, 0.224, 0.225])
])

val_t = transforms.Compose([
    transforms.Resize((IMG_SIZE, IMG_SIZE)),
    transforms.ToTensor(),
    transforms.Normalize(mean=[0.485, 0.456, 0.406],
                         std=[0.229, 0.224, 0.225])
])

print("Transforms built.")

# ------------------------------------
# DATASETS
# ------------------------------------
train_ds = datasets.ImageFolder(TRAIN_DIR, train_t)
val_ds = datasets.ImageFolder(VAL_DIR, val_t)

print("Classes:", train_ds.classes)
print("Train samples:", len(train_ds), "  Val samples:", len(val_ds))

# ------------------------------------
# SAFE DATALOADER (NO SHM CRASH)
# ------------------------------------
train_loader = DataLoader(
    train_ds, batch_size=BATCH_SIZE, shuffle=True,
    num_workers=0, pin_memory=False, persistent_workers=False
)

val_loader = DataLoader(
    val_ds, batch_size=BATCH_SIZE, shuffle=False,
    num_workers=0, pin_memory=False, persistent_workers=False
)

# ------------------------------------
# MODEL (RGB)
# ------------------------------------
model = models.resnet18(weights=None)
model.conv1 = nn.Conv2d(3, 64, kernel_size=7, stride=2, padding=3, bias=False)
model.fc = nn.Linear(512, 2)

print("Model params:", sum(p.numel() for p in model.parameters()))
model = model.to(device)

criterion = nn.CrossEntropyLoss()
optimizer = optim.Adam(model.parameters(), lr=LR)

# ------------------------------------
# TRAIN LOOP
# ------------------------------------
for epoch in range(1, EPOCHS + 1):
    print(f"\nEpoch {epoch}/{EPOCHS}")
    model.train()
    running_loss = 0

    for imgs, labels in train_loader:
        imgs, labels = imgs.to(device), labels.to(device)

        optimizer.zero_grad()
        outputs = model(imgs)
        loss = criterion(outputs, labels)
        loss.backward()
        optimizer.step()

        running_loss += loss.item()

    print(f"Train Loss: {running_loss / len(train_loader):.4f}")

print("\nTraining complete.")

# ------------------------------------
# SAVE PTH
# ------------------------------------
torch.save(model.state_dict(), PTH_PATH)
print(f"Saved PyTorch model → {PTH_PATH}")

# ------------------------------------
# EXPORT ONNX (1 × 3 × 224 × 224)
# ------------------------------------
dummy = torch.randn(1, 3, IMG_SIZE, IMG_SIZE).to(device)

torch.onnx.export(
    model,
    dummy,
    ONNX_PATH,
    input_names=["input"],
    output_names=["output"],
    opset_version=11,
    dynamic_axes={"input": {0: "batch"}, "output": {0: "batch"}}
)

print(f"Saved ONNX model → {ONNX_PATH}")
