import numpy as np

def get_correction(source, width, height):
    if source == "rtsp://192.168.137.200:8554/cam" and width == 1280 and height == 720: # BlueStar USB Cam 1 720p
        K = np.array(
            [
                [700.0, 0.0, width / 2.0],
                [0.0, 700.0, height / 2.0],
                [0.0, 0.0, 1.0],
            ],
            dtype=np.float64,
        )

        D = np.array([[-0.05], [0.01], [0.0], [0.0]])
    elif source == "rtsp://192.168.137.200:8554/cam2" and width == 1280 and height == 720: # BlueStar USB Cam 1 @ 720p
        K = np.array(
            [
                [700.0, 0.0, width / 2.0],
                [0.0, 700.0, height / 2.0],
                [0.0, 0.0, 1.0],
            ],
            dtype=np.float64,
        )

        D = np.array([[-0.05], [0.01], [0.0], [0.0]])
    else: # Other Cameras / other sources
        K = np.array(
            [
                [700.0, 0.0, width / 2.0],
                [0.0, 700.0, height / 2.0],
                [0.0, 0.0, 1.0],
            ],
            dtype=np.float64,
        )

        D = np.array([[-0.05], [0.01], [0.0], [0.0]])

    return K, D