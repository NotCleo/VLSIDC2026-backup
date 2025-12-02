!pip install --quiet qrcode qrcode-terminal

import os
import qrcode_terminal

# Directory for product info
os.makedirs('product_info', exist_ok=True)

product_types = [
    "extremely fragile",
    "partially fragile",
    "partially solid",
    "extremely solid"
]

# Create product info files and QR codes
for pid in range(1, 11):
    ptype = product_types[(pid - 1) % len(product_types)]
    info_text = f"Product ID: {pid}\nProduct Type: {ptype}\nStatus: OK"

    # Save as text file
    file_path = os.path.join("product_info", f"product_{pid}.txt")
    with open(file_path, "w") as f:
        f.write(info_text)

    # Print summary
    print(f"\n{'='*60}")
    print(f"Product {pid} — {ptype}")
    print(f"Saved: {file_path}")
    print(f"QR code below (encodes file text):")
    print(f"{'='*60}\n")

    # Print QR in terminal
    qrcode_terminal.draw(info_text)
