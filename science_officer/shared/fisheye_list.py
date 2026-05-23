import numpy as np

def get_correction(source, width, height):
    K = None
    D = None
    D_FISHEYE = None
    if source == "rtsp://192.168.137.200:8554/cam" and width == 1280 and height == 720: # BlueStar USB Cam 1 720p
        # Fisheye
        K = np.array(
            [
                [700.0, 0.0, width / 2.0],
                [0.0, 700.0, height / 2.0],
                [0.0, 0.0, 1.0],
            ],
            dtype=np.float64,
        )

        D_FISHEYE = np.array([[-0.05], [0.01], [0.0], [0.0]])

    elif source == "rtsp://192.168.137.200:8554/cam2" and width == 1280 and height == 720: # BlueStar USB Cam 1 @ 720p
        K = np.array(
            [
                [700.0, 0.0, width / 2.0],
                [0.0, 700.0, height / 2.0],
                [0.0, 0.0, 1.0],
            ],
            dtype=np.float64,
        )

        D_FISHEYE = np.array([[-0.05], [0.01], [0.0], [0.0]])

    elif source == "usb0" and width == 1280 and height == 720: # USB Testing cam
        K = np.array(
            [
                [700.0, 0.0, width / 2.0],
                [0.0, 700.0, height / 2.0],
                [0.0, 0.0, 1.0],
            ],
            dtype=np.float64,
        )

        D_FISHEYE = np.array([[0.107390768561], [-0.020355523575], [-0.050326565276], [-0.010284033343]])
        D = np.array([[0.107390768561, -0.020355523575, -0.050326565276, -0.010284033343, 0.081400030792]], dtype=np.float64,)

    else: # Other Cameras / other sources
        K = np.array(
            [
                [700.0, 0.0, width / 2.0],
                [0.0, 700.0, height / 2.0],
                [0.0, 0.0, 1.0],
            ],
            dtype=np.float64,
        )

        D_FISHEYE = np.array([[-0.05], [0.01], [0.0], [0.0]])

    return K, D_FISHEYE, D