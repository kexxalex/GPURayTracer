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
    return np.power(np.clip((data*(a*data+b))/(data*(c*data+d)+e), 0.0, 1.0), 1.0/2.2)

if __name__ == "__main__":
    width = int(sys.argv[1])
    height = int(sys.argv[2])

    result = np.fromfile('final.bytes', np.float32).reshape((height, width, 4)).transpose((1,0,2))[:,:,:3]
    mapped = ACESFilm(result)
    np.clip(mapped*0xFF, 0, 0xFF).astype(np.uint8).tofile('final.data')
