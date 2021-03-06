#!/usr/bin/python3.8

# USAGE
# python detect_aruco_video.py -c [cameraIndex] -t [dictType]

import argparse

import cv2
import numpy as np
from imutils import resize
from imutils.video import VideoStream


###################################################################################################
################################## FUNCTION DECLARATION ###########################################


def log(func):
    def wrapper(*args, **kwargs):
        ret_values = [val for val in func(*args, **kwargs)]
        with open("detection_output.txt", "a") as f:
            f.write(f"{str(func)} called with {args} {kwargs} -- output: {ret_values}\n\n")
        return ret_values
        
    return wrapper


def cleanup():
    cv2.destroyAllWindows()
    VIDEO_SOURCE.stop()


def dict_input_parser() -> int:
    user_input = 0
    while user_input not in ARUCO_DICT.keys():
        user_input = input("Input a aruco dictionary type do detect (suported types: -h / --help): ")
        if user_input.lower() in ["-h", "--help"]:
            print(str(ARUCO_DICT.keys())[str(ARUCO_DICT.keys()).index("(") + 1 : -1])
    return ARUCO_DICT[user_input]


def clr_input_parser() -> str:
    allowed_colors = ["r", "g", "b", "w"]
    user_input = None
    while user_input not in allowed_colors:
        try:
            user_input = input("Input a color channel to mask (r/g/b/w): ")
        except EOFError:
            cleanup()
            quit()

    return user_input


def mask_frame(frame: np.ndarray, color: str, delta: int = 12):
    # Extract primary color channels from incoming frame into a dictionary.
    channels = {"r": frame[:, :, 2], "g": frame[:, :, 1], "b": frame[:, :, 0]}

    mask_chnl = channels[color]
    ch2 = None

    for key in channels.keys():
        if key == color:
            continue
        if ch2 is None:
            ch2 = channels[key]
        else:
            ch3 = channels[key]

    color_mask = (mask_chnl > (ch2 + delta)) & (mask_chnl > (ch3 + delta))

    #  filter false positives that come up if _ + delta > 255
    false_positives = (ch2 < 255 - delta) & (ch3 < 255 - delta)
    masked_image = np.zeros(mask_chnl.shape, dtype="uint8")
    masked_image[color_mask & false_positives] = 255

    return masked_image


def process_frame(original_image, target_color_channel, morphology_kernel_size: tuple = (10, 10)):
    # run a bilateralFilter to blur the original image - helps reducing noise for future masking
    blurred_image = cv2.bilateralFilter(original_image, 15, 75, 90)

    # if the target color is "white", image is only converted to grayscale -- no further masking is needed
    if target_color_channel == "w":
        return original_image, cv2.cvtColor(blurred_image, cv2.COLOR_BGR2GRAY)

    # threshold image relative to the selected color channel\
    masked_image = mask_frame(blurred_image, target_color_channel)

    # create an eliptical kernel for image dilation and a rectangular one for erosion
    dilation_kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, morphology_kernel_size)
    erosion_kernel = cv2.getStructuringElement(cv2.MORPH_RECT, morphology_kernel_size)

    # image dilation with eliptical kernel will help close possible black spots inside the code squares
    # created due to excessive glow in the center of the pixel (where the led is located)
    masked_image = cv2.dilate(masked_image, dilation_kernel, iterations=1)

    # image erosion with rectangular kernel to try and correct the proportions of black and white squares
    # in the masked/thresholded image -- white squares tend have bigger area than the black squares
    masked_image = cv2.erode(masked_image, erosion_kernel, iterations=1)

    return original_image, masked_image


@log
def calculate_pose(corners):
    return cv2.aruco.estimatePoseSingleMarkers(corners, 0.02, CAMERA_MATRIX, DIST_COEFFS)


def detect_markers(original_image, masked_image, aruco_dict, verbose: bool):
    # detect ArUco markers in the input frame
    corners, ids, rejected = cv2.aruco.detectMarkers(
        masked_image, aruco_dict, parameters=ARUCO_PARAMS, cameraMatrix=CAMERA_MATRIX, distCoeff=DIST_COEFFS
    )

    if len(rejected) > 0 and verbose:
        cv2.aruco.drawDetectedMarkers(original_image, rejected)

    # if at least 1 marker was detected, corners matrix will not be empty
    if len(corners) > 0:
        cv2.aruco.drawDetectedMarkers(original_image, corners, ids)
        for i in range(len(ids)):
            rvec, tvec, marker_points = calculate_pose(corners[i])

            (rvec - tvec).any()
            cv2.aruco.drawAxis(original_image, CAMERA_MATRIX, DIST_COEFFS, rvec, tvec, 0.01)

        if verbose:
            print(f"Aruco Marker id: {ids[i]}")
            print(f"\trotation vector: [{rvec[ 0, 0, 0]} {rvec[ 0, 0, 1]} {rvec[ 0, 0, 2]}]")
            print(f"\ttranslation vector: [{tvec[ 0, 0, 0]} {tvec[ 0, 0, 1]} {tvec[ 0, 0, 2]}]")


###################################################################################################
######################################### MAIN CODE ###############################################


def main(dict_type):
    print("[INFO] detecting '{}' tags...".format(dict_type))
    aruco_dict = cv2.aruco.Dictionary_get(ARUCO_DICT[dict_type])

    target_color_channel = clr_input_parser()
    verbose = False  # flag to toggle marker info printing

    # main code loop --- loop over the frames from the video stream
    while True:
        # grab the frame from the threaded video stream and resize it
        # to have a maximum width of 1000 pixels
        original_image, masked_image = process_frame(resize(VIDEO_SOURCE.read(), width=1000), target_color_channel)

        detect_markers(original_image, masked_image, aruco_dict, verbose)

        # show the input and output frames
        cv2.imshow("Camera", original_image)
        cv2.imshow("CV2 input", masked_image)

        ################################ input waitKeys ################################
        key = cv2.waitKey(1) & 0xFF

        # if the 'i' key was pressed, pause the loop and parse the color mask input
        if key == ord("d"):
            dict_type = dict_input_parser()
            aruco_dict = cv2.aruco.Dictionary_get(dict_type)

        # if the 'i' key was pressed, pause the loop and parse the color mask input
        if key == ord("c"):
            target_color_channel = clr_input_parser()

        # toggle verbose mode using either 'v' or 'p' keys
        if key == ord("v") or key == ord("p"):
            verbose = verbose is not True

        # 'q' terminates execution
        if key == ord("q"):
            break

    # final cleanup
    cleanup()


###################################################################################################
######################################## DRIVER CODE ##############################################


if __name__ == "__main__":
    # construct the argument parser and parse the arguments
    ap = argparse.ArgumentParser()
    ap.add_argument("-c", "--camera", type=int, required=False, default=0, help="webcam index")
    ap.add_argument("-t", "--type", type=str, required=False, default="dict6_100", help="type of ArUCo tag to detect")
    args = vars(ap.parse_args())

    # define names of each possible ArUco tag OpenCV supports
    ARUCO_DICT = {
        "dict4_50": cv2.aruco.DICT_4X4_50,
        "dict4_100": cv2.aruco.DICT_4X4_100,
        "dict4_250": cv2.aruco.DICT_4X4_250,
        "dict4_1000": cv2.aruco.DICT_4X4_1000,
        "dict5_50": cv2.aruco.DICT_5X5_50,
        "dict5_100": cv2.aruco.DICT_5X5_100,
        "dict5_250": cv2.aruco.DICT_5X5_250,
        "dict5_1000": cv2.aruco.DICT_5X5_1000,
        "dict6_50": cv2.aruco.DICT_6X6_50,
        "dict6_100": cv2.aruco.DICT_6X6_100,
        "dict6_250": cv2.aruco.DICT_6X6_250,
        "dict6_1000": cv2.aruco.DICT_6X6_1000,
        # "dict7_50": cv2.aruco.DICT_7X7_50,  Not in use due to hardware limitations
        # "dict7_100": cv2.aruco.DICT_7X7_100,
        # "dict7_250": cv2.aruco.DICT_7X7_250,
        # "dict7_1000": cv2.aruco.DICT_7X7_1000,
        "original": cv2.aruco.DICT_ARUCO_ORIGINAL,
    }

    if args["type"] not in ARUCO_DICT:
        print("[FATAL] ArUCo tag of '{}' is not supported".format(args["type"]))
        quit()

    # load the aruco dictionary and create detection parameters
    ARUCO_PARAMS = cv2.aruco.DetectorParameters_create()

    for cam_src in range(-args["camera"], 10):
        print("[INFO] starting video stream...")
        VIDEO_SOURCE = VideoStream(src=cam_src, resolution=(1920, 1080)).start()
        testFrame = VIDEO_SOURCE.read()

        if testFrame is not None:
            break

        VIDEO_SOURCE.stop()

    if testFrame is None:
        print("[FATAL] camera feed not found")
        VIDEO_SOURCE.stop()
        quit()

    # with np.load("../cameraCalibration/calib_results.npz") as npzfile:
    #     cameraMatrix, distCoeffs, rvecs, tvecs = [
    #         npzfile[i] for i in ["mtx", "dist", "rvecs", "tvecs"]]

    # camera matrix and distance coefficients outputed by ros camera_calibration module -- needs propper integration
    CAMERA_MATRIX = np.ndarray(
        shape=(3, 3),
        buffer=np.array([752.461885, 0, 363.097359, 0, 513.308335, 242.851570, 0, 0, 1]),
    )

    # CAMERA_MATRIX = np.ndarray(
    #     shape=(3, 3),
    #     buffer=np.array([660.15908, 0.0, 312.77898, 0.0, 718.20931, 197.64162, 0.0, 0.0, 1.0]),
    # )

    DIST_COEFFS = np.ndarray(
        shape=(1, 5),
        buffer=np.array([0.050106, 0.045766, -0.019956, 0.022466, 0.000000]),
    )

    # DIST_COEFFS = np.ndarray(
    #     shape=(1, 5),
    #     buffer=np.array([0.071220, -0.145375, -0.016046, -0.003888, 0.000000]),
    # )

    main(args["type"])
