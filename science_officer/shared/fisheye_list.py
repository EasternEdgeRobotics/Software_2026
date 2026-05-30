import numpy as np

def get_correction(source, width, height):
    K_PINHOLE = None
    D_PINHOLE = None
    D_FISHEYE = None
    K_FISHEYE = None
    if source == "rtsp://192.168.137.200:8554/cam" and width == 1280 and height == 720: # BlueStar USB Cam 1 720p  
        K_PINHOLE = np.array(
            [[699.109225014212,   0.            , 622.213045643403],
            [  0.            , 697.848580237548, 361.076159115635],
            [  0.            ,   0.            ,   1.            ]]
            , dtype=np.float64)
        
        D_PINHOLE = np.array(
            [ 0.106249329844,  1.116833115869, -0.001670857685,  0.004376066072,
            -1.299643180583]
            , dtype=np.float64)
        
        K_FISHEYE = np.array(
            [[782.074211565195,   0.            , 633.867896973044],
            [  0.            , 783.865393384029, 366.818732620464],
            [  0.            ,   0.            ,   1.            ]]
            , dtype=np.float64)
        
        D_FISHEYE = np.array(
            [0.60572293225 , 0.406824578527, 4.248976304665, 0.075641646229]
            , dtype=np.float64)

    elif source == "rtsp://192.168.137.200:8554/cam2" and width == 1280 and height == 720: # BlueStar USB Cam 1 @ 720p
        K_PINHOLE = np.array(
            [
                [717.455974216264, 0.0, 619.957448045625],
                [0.0, 717.005657811401, 353.389742786115],
                [0.0, 0.0, 1.0],
            ],
            dtype=np.float64,
        )

        D_PINHOLE = np.array([[ 0.068425017715,  1.540616528956, -0.011678777902, -0.003492755592,-1.854065441272]])
        D_FISHEYE = np.array([0.068425017715,  1.540616528956, -0.011678777902, -0.003492755592], dtype=np.float64)

    elif source == "usb0" and width == 1280 and height == 720: # USB Testing cam
        K_PINHOLE = np.array(
            [
                [700.0, 0.0, width / 2.0],
                [0.0, 700.0, height / 2.0],
                [0.0, 0.0, 1.0],
            ],
            dtype=np.float64,
        )
        K_FISHEYE = np.array(
            [
                [700.0, 0.0, width / 2.0],
                [0.0, 700.0, height / 2.0],
                [0.0, 0.0, 1.0],
            ],
            dtype=np.float64,
        )

        D_FISHEYE = np.array([[0.107390768561], [-0.020355523575], [-0.050326565276], [-0.010284033343]], dtype=np.float64,)
        D_PINHOLE = np.array([[0.107390768561, -0.020355523575, -0.050326565276, -0.010284033343, 0.081400030792]], dtype=np.float64,)

    else: # Other Cameras / other sources
        K_FISHEYE = np.array(
            [
                [700.0, 0.0, width / 2.0],
                [0.0, 700.0, height / 2.0],
                [0.0, 0.0, 1.0],
            ],
            dtype=np.float64,
        )
        K_PINHOLE = np.array(
            [
                [700.0, 0.0, width / 2.0],
                [0.0, 700.0, height / 2.0],
                [0.0, 0.0, 1.0],
            ],
            dtype=np.float64,
        )
        D_FISHEYE = np.array([[-0.05], [0.01], [0.0], [0.0]])
        D_PINHOLE = np.array([[-0.05], [0.01], [0.0], [0.0], [0.0]])


    return K_FISHEYE, D_FISHEYE, K_PINHOLE, D_PINHOLE