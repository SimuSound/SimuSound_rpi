#ifndef CAMERA_BASE_H
#define CAMERA_BASE_H

#include <opencv2/opencv.hpp>

// my shitty attempt at polymorphism
// doesn't work because c++ reasons
// do not use

class StereoCameraSet
{
// TODO: make some of this private
// TODO: raspicam apparently has a mode to capture 2 images side by side (only for rpi cams)
// TODO: handle case that images are read more than once per frame update (use bool to indicate read status)
public:
    virtual int initialize_cameras() = 0;
    virtual int start_capture() = 0;
    virtual int get_images(cv::Mat &img_l, cv::Mat &img_r) = 0;
    virtual int stop_capture() = 0;
};

#endif /* CAMERA_BASE_H */