#include <dirent.h>

#include <opencv2/opencv.hpp>
#include <opencv2/videoio/registry.hpp>

#include "utils.h"

bool DirectoryExists(const char* pzPath)
{
    if ( pzPath == NULL) return false;

    DIR *pDir;
    bool bExists = false;

    pDir = opendir (pzPath);

    if (pDir != NULL)
    {
        bExists = true;    
        (void) closedir (pDir);
    }

    return bExists;
}

using namespace cv;

std::string fourcc_to_str(int fourcc)
{
    std::string fourcc_str = format("%c%c%c%c", fourcc & 255, (fourcc >> 8) & 255, (fourcc >> 16) & 255, (fourcc >> 24) & 255);
    return fourcc_str;
}

void downsample_vertical(Mat &img_in, Mat &img_out, int factor)
{
    img_in.copyTo(img_out);
    resize(img_out, img_out, Size(), 1, 1.0/factor, INTER_NEAREST);
}

void print_available_camera_backends()
{
    std::vector<VideoCaptureAPIs> backends = videoio_registry::getCameraBackends();
    std::cout << "available backends: ";
    for(int i : backends)
    {
        std::cout << i << ", ";
    }
    std::cout << std::endl;
}