import os, sys
import numpy as np

if len(sys.argv) < 3:
    print("Pls execute with width and height")
    exit()

if __name__ == "__main__":
    width = int(sys.argv[1])
    height = int(sys.argv[2])

    result = np.frombuffer(open('final.bytes', 'rb').read(), np.float32).reshape((height, width, 4))
    luma = np.max(np.dot(result, np.array([0.299, 0.587, 0.114, 0])))
    result = result / luma / 0.587
    bmp = np.array((np.clip(result, 0.0, 1.0) * 255)[::-1, :, :3], dtype=np.uint8)
    bmp.tofile('final.data')
