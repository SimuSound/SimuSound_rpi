#ifndef CAMERA_MIPI_H
#define CAMERA_MIPI_H

#include <pthread.h>
#include "interface/mmal/mmal.h"

#include <opencv2/opencv.hpp>

// #include "camera_base.h"

typedef struct {
    uint32_t video_width;
    uint32_t video_height;
    uint32_t frame_width;
    uint32_t frame_height;
    float video_fps;
    MMAL_POOL_T *splitter_port_pool;
    void *image0_data;
    void *image1_data;
    pthread_mutex_t *mutex;
} PORT_USERDATA;


// for now only create one instance of this class
class StereoMipiCameraSet
{
// TODO: make some of this private
// TODO: handle case that images are read more than once per frame update (use bool to indicate already read)
public:
    pthread_mutex_t *mutex;
    int cam_num_l;
    int cam_num_r;  // not technically needed
    int image_width;  // of each frame, must be multiple of 32
    int image_height;  // so *2 for full top-bottom image height, must be multiple of 16

    // cv::Mat image0;  // temporary storage of images
    // cv::Mat image1;
    MMAL_COMPONENT_T *camera;
    PORT_USERDATA userdata;     // user-defined data

    StereoMipiCameraSet(pthread_mutex_t *mutex, int cam_num_l, int cam_num_r, int image_width, int image_height);
    ~StereoMipiCameraSet();

    int initialize_cameras();
    int start_capture();
    int get_images(cv::Mat &img_l, cv::Mat &img_r);
    int stop_capture();

};

#endif /* CAMERA_MIPI_H */
