#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <pthread.h>

#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>

#include "camera_usb.h"
#include "utils.h"

using namespace cv;

StereoUSBCameraSet::StereoUSBCameraSet(pthread_mutex_t *mutex, int cam_num_l, int cam_num_r, int image_width, int image_height, char cam_fourcc[5])
{
    this->mutex = mutex;
    this->cam_num_l = cam_num_l;
    this->cam_num_r = cam_num_r;
    this->image_width = image_width;
    this->image_height = image_height;
    memcpy(this->cam_fourcc, cam_fourcc, 5);
    this->buf_img_l = cv::Mat(image_height, image_width, CV_8UC1);
    this->buf_img_r = cv::Mat(image_height, image_width, CV_8UC1);
}

/*
    Returns 0 on success, nonzero error on fail.
*/
int StereoUSBCameraSet::initialize_cameras()
{
    char* cc = this->cam_fourcc;
    int fourcc = VideoWriter::fourcc(cc[0], cc[1], cc[2], cc[3]);

    cap_l.open(cam_num_l);
    if(!cap_l.isOpened())
    {
        printf("cound not open left camera.\n");
        return 1;
    }

    cap_l.set(CAP_PROP_FRAME_WIDTH, image_width);
    cap_l.set(CAP_PROP_FRAME_HEIGHT, image_height);
    cap_l.set(CAP_PROP_FOURCC, fourcc);
    cap_l.set(CAP_PROP_FPS, 30);
    cap_l.set(CAP_PROP_BUFFERSIZE, CAMERA_BUFFER_SIZE);

    if(cap_l.get(CAP_PROP_FRAME_WIDTH) != image_width || cap_l.get(CAP_PROP_FRAME_HEIGHT) != image_height)
    {
        printf("left camera wrong resolution.\n");
        return 1;
    }
    if(cap_l.get(CAP_PROP_FOURCC) != fourcc)
    {
        printf("left camera wrong format.\n");
        return 1;
    }
    // TODO: validate fps
    if(cap_l.get(CAP_PROP_BUFFERSIZE) != CAMERA_BUFFER_SIZE)
    {
        printf("left camera unable to set buffersize.\n");
        return 1;
    }
    
    printf("left camera: width=%d, height=%d, fourcc=%s\n", (int) cap_l.get(CAP_PROP_FRAME_WIDTH), (int) cap_l.get(CAP_PROP_FRAME_HEIGHT), fourcc_to_str(cap_l.get(CAP_PROP_FOURCC)).c_str());


    cap_r.open(cam_num_r);
    if(!cap_r.isOpened())
    {
        printf("cound not open right camera.\n");
        return 1;
    }

    cap_r.set(CAP_PROP_FRAME_WIDTH, image_width);
    cap_r.set(CAP_PROP_FRAME_HEIGHT, image_height);
    cap_r.set(CAP_PROP_FOURCC, fourcc);
    cap_r.set(CAP_PROP_FPS, 30);
    cap_r.set(CAP_PROP_BUFFERSIZE, CAMERA_BUFFER_SIZE);

    if(cap_r.get(CAP_PROP_FRAME_WIDTH) != image_width || cap_r.get(CAP_PROP_FRAME_HEIGHT) != image_height)
    {
        printf("right camera wrong resolution.\n");
        return 1;
    }
    if(cap_r.get(CAP_PROP_FOURCC) != fourcc)
    {
        printf("right camera wrong format.\n");
        return 1;
    }
    // TODO: validate fps

    if(cap_r.get(CAP_PROP_BUFFERSIZE) != CAMERA_BUFFER_SIZE)
    {
        printf("right camera unable to set buffersize.\n");
        return 1;
    }

    printf("right camera: width=%d, height=%d, fourcc=%s\n", (int) cap_r.get(CAP_PROP_FRAME_WIDTH), (int) cap_r.get(CAP_PROP_FRAME_HEIGHT), fourcc_to_str(cap_r.get(CAP_PROP_FOURCC)).c_str());

    return 0;
}

int StereoUSBCameraSet::start_capture()
{
    pthread_t t;
    printf("this: %p\n", this);
    pthread_create(&t, NULL, &StereoUSBCameraSet::start_capture_helper, this);
    return 0;
}

void * StereoUSBCameraSet::start_capture_helper(void *context)
{
    return ((StereoUSBCameraSet *)context)->start_capture_thread();
}

void * StereoUSBCameraSet::start_capture_thread(void)
{
    printf("camera thread started\n");
    for(;;)
    {
        // does not wait for new frame, if called too fast it will grab the same frame again
        cap_l.grab();
        cap_r.grab();
        printf("grabbed frames\n");

        pthread_mutex_lock(this->mutex);

        cap_l.retrieve(buf_img_l);
        cap_r.retrieve(buf_img_r);
        printf("decoded frames\n");

        pthread_mutex_unlock(this->mutex);

        // wait some time to not use 100% cpu
        usleep(50 * 1000);
    }

    // TODO: graceful exit condition
    return NULL;
}

int StereoUSBCameraSet::get_images(cv::Mat &img_l, cv::Mat &img_r)
{
    pthread_mutex_lock(mutex);
    memcpy(img_l.data, buf_img_l.data, this->image_width*this->image_height*3);
    memcpy(img_r.data, buf_img_r.data, this->image_width*this->image_height*3);
    pthread_mutex_unlock(mutex);
    return 0;
}

int StereoUSBCameraSet::stop_capture()
{
    return 0;
}