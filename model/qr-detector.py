import cv2
import numpy as np
from PIL import Image
import zxingcpp
import matplotlib.pyplot as plt

def show(title, img):
    plt.figure(figsize=(5,5))
    if len(img.shape) == 2:
        plt.imshow(img, cmap="gray")
    else:
        plt.imshow(cv2.cvtColor(img, cv2.COLOR_BGR2RGB))
    plt.title(title)
    plt.axis("off")
    plt.show()

def detect_cardboard(img):
    hsv = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)

    lower = np.array([5, 40, 40])
    upper = np.array([25, 255, 255])

    mask = cv2.inRange(hsv, lower, upper)

    mask = cv2.medianBlur(mask, 11)
    mask = cv2.dilate(mask, None, iterations=3)

    cnts, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    if not cnts:
        print("[ERROR] No cardboard color region found.")
        return None

    cnt = max(cnts, key=cv2.contourArea)
    x,y,w,h = cv2.boundingRect(cnt)

    cardboard = img[y:y+h, x:x+w]
    print(f"[INFO] Cardboard Crop: x={x}, y={y}, w={w}, h={h}")

    return cardboard, (x,y,w,h)

def detect_qr_opencv(img):
    qr = cv2.QRCodeDetector()
    data, pts, _ = qr.detectAndDecode(img)
    if pts is None or len(pts)==0:
        return None, None
    pts = pts[0].astype(int)
    x1,y1 = pts.min(axis=0)
    x2,y2 = pts.max(axis=0)
    crop = img[y1:y2, x1:x2]
    return data, crop


def detect_qr_zxing(img):
    try:
        pil = Image.fromarray(cv2.cvtColor(img, cv2.COLOR_BGR2RGB))
        r = zxingcpp.read_barcodes(pil)
        if r:
            return r[0].text
        return None
    except:
        return None

def robust_qr_decode(qr_region):
    attempts = []

    attempts.append(("Raw", qr_region))

    gray = cv2.cvtColor(qr_region, cv2.COLOR_BGR2GRAY)
    eq = cv2.equalizeHist(gray)
    attempts.append(("Equalized", eq))

    thr = cv2.adaptiveThreshold(eq,255,cv2.ADAPTIVE_THRESH_GAUSSIAN_C,
                                cv2.THRESH_BINARY,25,10)
    attempts.append(("Adaptive Threshold", thr))

    thr2 = cv2.bitwise_not(thr)
    attempts.append(("Inverted Threshold", thr2))

    for name, img in attempts:
        print(f"[INFO] Trying QR decode: {name}")
        if len(img.shape)==2:
            test_img = cv2.cvtColor(img, cv2.COLOR_GRAY2BGR)
        else:
            test_img = img

        data, _ = detect_qr_opencv(test_img)
        if data:
            print(f"[SUCCESS] OpenCV decoded: {data}")
            return data

        zx = detect_qr_zxing(test_img)
        if zx:
            print(f"[SUCCESS] ZXing decoded: {zx}")
            return zx

    return None

def main():
    img = cv2.imread("IMG_9508.JPG")
    print("[INFO] Loaded:", img.shape)

    # Stage 1: Cardboard detection
    cardboard, _ = detect_cardboard(img)
    if cardboard is None:
        return

    show("Cardboard Crop", cardboard)

    H, W = cardboard.shape[:2]
    qr_region = cardboard[0:int(H*0.5), 0:int(W*0.5)]

    show("QR Search Region", qr_region)

    # Stage 2: QR decoding
    qr_text = robust_qr_decode(qr_region)

    if qr_text:
        print("FINAL QR DECODED TEXT:", qr_text)
    else:
        print("[FAIL] Could not decode QR from any method.")


if __name__ == "__main__":
    main()
