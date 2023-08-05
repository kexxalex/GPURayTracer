import sys
import numpy as np

if len(sys.argv) < 3:
    print("Pls execute with width and height")
    exit()

def ACESFilm(data):
    a = 2.51;
    b = 0.03;
    c = 2.43;
    d = 0.59;
    e = 0.14;
    return np.power(np.clip((data*(a*data+b))/(data*(c*data+d)+e), 0.0, 1.0), 1.0/2.6)

if __name__ == "__main__":
    width = int(sys.argv[1])
    height = int(sys.argv[2])

    result = np.fromfile('final.bytes', np.float32).reshape((height, width, 4)).transpose((1,0,2))[:,:,:3]
    print(result.shape)
    mapped = ACESFilm(result)
    less = (mapped <= 0.0031308)
    mapped[less] *= 12.92
    mapped[np.logical_not(less)] = 1.055 * np.power(mapped[np.logical_not(less)], 1.0/2.4) - 0.055
    np.clip(mapped*0x100, 0, 0xFF).astype(np.uint8).tofile('final.data')
