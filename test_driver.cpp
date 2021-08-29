#include <algorithm>
#include <cstring>
#include <vector>
#include <iostream>
#include <cstdio>
#include <chrono>

#include <unistd.h>
#include <pthread.h>

#include <opencv2/opencv.hpp>

#include "camera_mipi.h"
#include "camera_usb.h"
#include "mobiledet.h"
#include "stereo_matching.h"
#include "utils.h"
#include "extra_args.h"

using namespace cv;

void resize_ratio(Mat img, int max_dim)
{
    double scale_factor = (double) max_dim / std::max(img.rows, img.cols);
    resize(img, img, Size(img.cols * scale_factor, img.rows * scale_factor));
}

void resize_ratio(Mat img_src, Mat img_dest, int max_dim)
{
    double scale_factor = (double) max_dim / std::max(img_src.rows, img_src.cols);
    resize(img_src, img_dest, Size(img_src.cols * scale_factor, img_src.rows * scale_factor));
}


int main(int argc, char** argv)
{
    // argv[1] is model file name
	if(argc < 3) {
		std::cout << "WARNING: No parameter file given." << std::endl;
	}
	else {
		std::string parameterFile = std::string(argv[2]);
		try {
			parse_logic_camera_args(parameterFile);
		} catch(std::exception& e) {
			std::cout << e.what() << std::endl;
		}
	}

    pthread_mutex_t mutex;
    Mat img_local_l(camera_height, camera_width, CV_8UC3, Scalar(0));
    Mat img_local_r(camera_height, camera_width, CV_8UC3, Scalar(0));
    #ifdef USE_USB_CAMS
        StereoUSBCameraSet *stereo_cams;
    #else
        StereoMipiCameraSet *stereo_cams;
    #endif

    if (pthread_mutex_init(&mutex, NULL) != 0)
    {
        printf("\nmutex init failed\n");
        return 1;
    }

    // init cameras
    #ifdef USE_USB_CAMS
        stereo_cams = new StereoUSBCameraSet(&mutex, left_cam_num, right_cam_num, camera_width, camera_height, usb_cam_fourcc.c_str());
    #else
        stereo_cams = new StereoMipiCameraSet(&mutex, left_cam_num, right_cam_num, camera_width, camera_height);
    #endif
    if(stereo_cams->initialize_cameras())
    {
        printf("failed to initialize cameras\n");
        return 1;
    }
    if(stereo_cams->start_capture())
    {
        printf("failed to start capture\n");
        return 1;
    }

    // init AI model
    const std::string model_file = argv[1];
    MobileDetCoral mobiledet;
    mobiledet.setup(model_file);

    // init stereo undistort
    StereoUndistort stereo_undistort;
    stereo_undistort.setup_stereo_undistort(
        camera_width,
        camera_height,
        calibration_path + "/mapL1_LE.wav",
        calibration_path + "/mapL2_LE.wav",
        calibration_path + "/mapR1_LE.wav",
        calibration_path + "/mapR2_LE.wav"
    );

    // init stereo matching
    StereoMatcherBM stereo_matcher;
    StereoParameters stereo_params;
    stereo_params.blockSize = 25;
    stereo_params.numDisparities = 256;
    stereo_matcher.setup_stereo_matching(stereo_params);

    // wait for camera thread to start getting images
    sleep(5);
    printf("start displaying frames\n");
    
    Mat img_ai_model_l;
    Mat img_ai_model_l_display;
    Mat img_stereo_disparity_l;
    Mat img_stereo_disparity_disp_l;
    Mat img_local_l_temp;
    Mat img_local_r_temp;
    Mat img_local_l_undistort;
    Mat img_local_r_undistort;

    
    auto start = std::chrono::steady_clock::now();
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

    for(int i = 0; ; ++i)
    {
        start = std::chrono::steady_clock::now();
        stereo_cams->get_images(img_local_l, img_local_r);
        end = std::chrono::steady_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "get_images time: " << duration << " ms" << std::endl;

        // imshow("left", img_local_l);
        // imshow("right", img_local_r);
        // waitKey(1);
        img_ai_model_l = img_local_l.clone();

        // blocking, so not efficient, should make nonblocking so coral and rpi can compute simultaneously
        start = std::chrono::steady_clock::now();
        mobiledet.evaluate(img_ai_model_l);
        end = std::chrono::steady_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "ai inference time: " << duration << " ms" << std::endl;

        img_ai_model_l = resize_keep_aspect_ratio(img_ai_model_l, cv::Size(832, 832), cv::Scalar(0, 0, 0));
        img_ai_model_l_display = draw_detected_objects(mobiledet.detected_objects, img_ai_model_l, 832/(float)320, 0.4);

        imshow("mobiledet", img_ai_model_l_display);
        // waitKey(1);

        // downsample_vertical(img_local_l, img_local_l_temp);
        // downsample_vertical(img_local_r, img_local_r_temp);

        cvtColor(img_local_l, img_local_l_temp, COLOR_BGR2GRAY);
        cvtColor(img_local_r, img_local_r_temp, COLOR_BGR2GRAY);

        stereo_undistort.undistort(img_local_l_temp, img_local_r_temp, img_local_l_undistort, img_local_r_undistort);
        imshow("undistort left", img_local_l_undistort);
        imshow("undistort right", img_local_r_undistort);
        // waitKey(1);


        start = std::chrono::steady_clock::now();
        stereo_matcher.stereo_match(img_local_l_undistort, img_local_r_undistort, img_stereo_disparity_l);
        end = std::chrono::steady_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "stereo matching time: " << duration << " ms" << std::endl;

        cv_stereo_16sc1_to_8uc1(img_stereo_disparity_l, img_stereo_disparity_disp_l);
        normalize(img_stereo_disparity_disp_l, img_stereo_disparity_disp_l, 0, 255, NORM_MINMAX);
        imshow("disparity", img_stereo_disparity_disp_l);

        if( waitKey(10) == 27 ) break; // stop capturing by pressing ESC

        printf("new frames displayed %d\n", i);

    }

    return 0;
}
