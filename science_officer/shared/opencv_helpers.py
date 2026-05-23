import cv2
from math import sqrt
import numpy as np
import time

def text_with_background(img, text, position, font=cv2.FONT_HERSHEY_SIMPLEX, font_scale=1, text_color=(255, 255, 255), bg_color=(0, 0, 0), thickness=2, padding=5,):
    x, y = position

    text_size, baseline = cv2.getTextSize(text, font, font_scale, thickness)
    text_width, text_height = text_size

    top_left = (x - padding, y - text_height - padding)
    bottom_right = (x + text_width + padding, y + baseline + padding)

    cv2.rectangle(img, top_left, bottom_right, bg_color, cv2.FILLED)

    cv2.putText(img, text, position, font, font_scale, text_color, thickness, cv2.LINE_AA,)

def draw_zoom_cursor(display_img, source_img, center, zoom=2.5, lens_radius=90, border_color=(0, 255, 255), crosshair_color=(0, 0, 255)):
    x, y = center
    h, w = display_img.shape[:2]

    if x < 0 or y < 0 or x >= w or y >= h:
        return

    diameter = lens_radius * 2
    crop_size = max(2, int(diameter / zoom))

    pad = crop_size + 4

    padded = cv2.copyMakeBorder(source_img, pad, pad, pad, pad, cv2.BORDER_REPLICATE)

    patch = cv2.getRectSubPix(padded, (crop_size, crop_size), (x + pad, y + pad))

    zoomed = cv2.resize(patch, (diameter, diameter), interpolation=cv2.INTER_LINEAR)

    mask = np.zeros((diameter, diameter), dtype=np.uint8)
    cv2.circle(mask, (lens_radius, lens_radius), lens_radius, 255, -1)

    dst_x1 = x - lens_radius
    dst_y1 = y - lens_radius
    dst_x2 = x + lens_radius
    dst_y2 = y + lens_radius

    src_x1 = 0
    src_y1 = 0
    src_x2 = diameter
    src_y2 = diameter

    if dst_x1 < 0:
        src_x1 = -dst_x1
        dst_x1 = 0

    if dst_y1 < 0:
        src_y1 = -dst_y1
        dst_y1 = 0

    if dst_x2 > w:
        src_x2 -= dst_x2 - w
        dst_x2 = w

    if dst_y2 > h:
        src_y2 -= dst_y2 - h
        dst_y2 = h

    zoom_crop = zoomed[src_y1:src_y2, src_x1:src_x2]
    mask_crop = mask[src_y1:src_y2, src_x1:src_x2]

    roi = display_img[dst_y1:dst_y2, dst_x1:dst_x2]

    np.copyto(roi, zoom_crop, where=mask_crop[:, :, None].astype(bool))

    cv2.circle(display_img, (x, y), lens_radius, border_color, 3, cv2.LINE_AA)
    cv2.line(display_img, (x - 12, y), (x + 12, y), crosshair_color, 2, cv2.LINE_AA)
    cv2.line(display_img, (x, y - 12), (x, y + 12), crosshair_color, 2, cv2.LINE_AA)

def prepare_fisheye(K, D, resolution, balance):
    balance = 0.3

    new_K = cv2.fisheye.estimateNewCameraMatrixForUndistortRectify(
        K,
        D,
        resolution,
        np.eye(3),
        balance=balance,
    )

    map1, map2 = cv2.fisheye.initUndistortRectifyMap(
        K,
        D,
        np.eye(3),
        new_K,
        resolution,
        cv2.CV_16SC2,
    )

    return map1, map2