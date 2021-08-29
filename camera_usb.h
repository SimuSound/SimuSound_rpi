#ifndef CAMERA_USB_H
#define CAMERA_USB_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>

// #include "camera_base.h"

#define CAMERA_BUFFER_SIZE 1


class StereoUSBCameraSet
{
// TODO: make some of this private
// TODO: handle case that images are read more than once per frame update (use bool to indicate read status)
public:
    cv::VideoCapture cap_l;
    cv::VideoCapture cap_r;
    pthread_mutex_t *mutex;
    int cam_num_l;
    int cam_num_r;
    cv::Mat buf_img_l;
    cv::Mat buf_img_r;
    int image_width;
    int image_height;
    char cam_fourcc[5];
    volatile bool thread_exit;

    StereoUSBCameraSet(pthread_mutex_t *mutex, int cam_num_l, int cam_num_r, int image_width, int image_height, char cam_fourcc[5]);
    ~StereoUSBCameraSet();

    int initialize_cameras();
    int start_capture();
    int get_images(cv::Mat &img_l, cv::Mat &img_r);
    int stop_capture();

    static void *start_capture_helper(void *context);
    void * start_capture_thread();

};

#endif /* CAMERA_USB_H */
