
#include <pthread.h>

#include <opencv2/opencv.hpp>

#include "camera_mipi.h"
#include "unet.h"
#include "stereo_matching.h"
#include "utils.h"
#include "extra_args.h"

using namespace cv;

int main(int argc, char** argv)
{
    get_extra_args();

    cv::setNumThreads(0);

    pthread_mutex_t mutex;
    Mat img_local_l(camera_height, camera_width, CV_8UC3, Scalar(0));
    Mat img_local_r(camera_height, camera_width, CV_8UC3, Scalar(0));
    StereoMipiCameraSet *stereo_cams;

    if (pthread_mutex_init(&mutex, NULL) != 0)
    {
        printf("\nmutex init failed\n");
        return 1;
    }

    // init cameras
    stereo_cams = new StereoMipiCameraSet(&mutex, left_cam_num, right_cam_num, camera_width, camera_height);
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
    stereo_params.blockSize = 51;
    stereo_params.numDisparities = 256;
    stereo_matcher.setup_stereo_matching(stereo_params);

    // wait for camera thread to start getting images
    sleep(5);
    printf("start displaying frames\n");

    Mat img_stereo_disparity_l;
    Mat img_stereo_disparity_disp_l;
    Mat img_local_l_temp;
    Mat img_local_r_temp;
    Mat img_local_l_undistort;
    Mat img_local_r_undistort;

    std::string img_dir("./");

    for(int i = 0;;)
    {
        stereo_cams->get_images(img_local_l, img_local_r);

        imshow("left", img_local_l);
        imshow("right", img_local_r);
        waitKey(1);
 
        stereo_undistort.undistort(img_local_l, img_local_r, img_local_l_undistort, img_local_r_undistort);
        imshow("undistort left", img_local_l_undistort);
        imshow("undistort right", img_local_r_undistort);
        waitKey(1);
    
        cvtColor(img_local_l_undistort, img_local_l_temp, COLOR_BGR2GRAY);
        cvtColor(img_local_r_undistort, img_local_r_temp, COLOR_BGR2GRAY);

        stereo_matcher.stereo_match(img_local_l_temp, img_local_r_temp, img_stereo_disparity_l);
        cv_stereo_16sc1_to_8uc1(img_stereo_disparity_l, img_stereo_disparity_disp_l);
        normalize(img_stereo_disparity_disp_l, img_stereo_disparity_disp_l, 0, 255, NORM_MINMAX);
        imshow("disparity", img_stereo_disparity_disp_l);

        printf("new frames displayed\n");

        int key = waitKey(1);  // get single character

        // if( waitKey(10) == 27 ) break; // stop capturing by pressing ESC

        if(key == 113)  // stop capturing by pressing q
        {
            printf("exiting\n");
            break;
        }
        else if(key == 32)  // save image by pressing spacebar
        {
            printf("saving image set %d\n", i);
            imwrite(img_dir + std::to_string(i) + "_l_undistorted.png", img_local_l_undistort);
            imwrite(img_dir + std::to_string(i) + "_r_undistorted.png", img_local_r_undistort);
            printf("saved image set %d\n", i);
            ++i;
        }
        else  // any other key skips current image image
        {
            printf("skipping\n");
        }
    }

    return 0;
}