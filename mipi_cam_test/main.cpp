#include <iostream>
#include <chrono>

#include <cstdio>

#include <opencv2/opencv.hpp>

#include "../stereo_matching.h"
#include "../utils.h"

using namespace cv;

int setup_cameras(VideoCapture &cap_l, VideoCapture &cap_r)
{
    print_available_camera_backends();

    cap_l.open(0, CAP_V4L2);
    printf("left camera opened\n");
    if(!cap_l.isOpened())
    {
        printf("cound not open left camera.\n");
        return 1;
    }
    cap_l.set(CAP_PROP_FRAME_WIDTH, 640);
    cap_l.set(CAP_PROP_FRAME_HEIGHT, 480);
    cap_l.set(CAP_PROP_FOURCC, VideoWriter::fourcc('M', 'J', 'P', 'G'));
    cap_l.set(CAP_PROP_FPS, 30);
    cap_l.set(CAP_PROP_BUFFERSIZE, 1);
    printf("left camera ready\n");

    cap_r.open(1, CAP_V4L2);
    printf("right camera opened\n");
    if(!cap_r.isOpened())
    {
        printf("cound not open right camera.\n");
        return 1;
    }
    cap_r.set(CAP_PROP_FRAME_WIDTH, 640);
    cap_r.set(CAP_PROP_FRAME_HEIGHT, 480);
    cap_r.set(CAP_PROP_FOURCC, VideoWriter::fourcc('M', 'J', 'P', 'G'));
    cap_r.set(CAP_PROP_FPS, 30);
    cap_r.set(CAP_PROP_BUFFERSIZE, 1);
    printf("right camera ready\n");

    return 0;
}

int main(int argc, char* argv[])
{
    Mat img_l;
    Mat img_r;
    Mat img_disp;
    Mat img_disp_filtered;
    VideoCapture cap_l;
    VideoCapture cap_r;
    int key;

    if(argc == 1)
    {
        if(setup_cameras(cap_l, cap_r))
        {
            return 1;
        }
    }

    // init stereo matching
    StereoMatcherBM stereo_matcher;
    StereoParameters stereo_params;
    stereo_params.blockSize = 9;
    stereo_matcher.setup_stereo_matching(stereo_params);

    WLSFilter wls_filter;
    WLSFilterParameters wls_filter_params;
    wls_filter.setup_wls_filter(wls_filter_params, stereo_matcher.stereo_matcher);

    printf("press esc to exit, any other key to get next frame\n");

    for(;;)
    {
        if(argc == 1)
        {
            cap_l.grab();
            cap_r.grab();
            cap_l.retrieve(img_l);
            cap_r.retrieve(img_r);
        }
        else
        {
            img_l = imread(argv[1]);
            img_r = imread(argv[2]);
        }
        imshow("left", img_l);
        imshow("right", img_r);

        cvtColor(img_l, img_l, cv::COLOR_RGB2GRAY);
        cvtColor(img_r, img_r, cv::COLOR_RGB2GRAY);

        auto start = std::chrono::steady_clock::now();
        stereo_matcher.stereo_match(img_l, img_r, img_disp);
        auto end = std::chrono::steady_clock::now();
        std::cout << "block matching time: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
            << " ms" << std::endl;

        start = std::chrono::steady_clock::now();
        wls_filter.filter(img_disp, img_l, img_disp_filtered);
        end = std::chrono::steady_clock::now();
        std::cout << "wls filtering time: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
            << " ms" << std::endl;

        cv_stereo_16sc1_to_8uc1(img_disp, img_disp);
        cv_stereo_16sc1_to_8uc1(img_disp_filtered, img_disp_filtered);

        cv::normalize(img_disp, img_disp, 0, 255, cv::NORM_MINMAX);
        cv::normalize(img_disp_filtered, img_disp_filtered, 0, 255, cv::NORM_MINMAX);
        imshow("disparity", img_disp);
        imshow("disparity filtered", img_disp_filtered);

        key = waitKey(0);
        if(key == 27)
        {
            break;
        }
    }
    
    return 0;
}