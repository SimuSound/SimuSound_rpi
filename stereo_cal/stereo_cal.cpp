#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <ctime>
#include <iostream>

#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#include <opencv2/opencv.hpp>

#include "camera_mipi.h"
#include "utils.h"
#include "camera_params.h"

using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::steady_clock;
using std::chrono::system_clock;

using namespace cv;

long get_ms_since_start()
{
    static auto time_start = steady_clock::now();
    auto time_current = steady_clock::now();
    auto duration = time_current - time_start;
    milliseconds d = duration_cast<milliseconds>(duration);
    long ms_since_start = d.count();
    return ms_since_start;
}

std::string getTimeStr(){
    static bool first_time = true;
    static std::time_t now = system_clock::to_time_t(system_clock::now());
    static char s[100] = {0};
    if(first_time)
    {
        std::strftime(&s[0], 100, "%Y-%m-%d_%H-%M-%S", std::localtime(&now));
        first_time = false;
    }
    std::string s2(s);
    return s;
}

int main(int argc, char** argv)
{
    printf("milliseconds since program start: %ld\n", get_ms_since_start());
    std::string curr_datetime = getTimeStr();
    printf("Current datetime: %s\n", curr_datetime.c_str());

    if(argc < 2)
    {
        printf("Specify directory to store captured images\n");
        return 1;
    }
    if(!DirectoryExists(argv[1]))
    {
        printf("Directory \"%s\" does not exist\n", argv[1]);
        return 1;
    }
    std::string img_dir(argv[1]);
    img_dir += "/" + curr_datetime + "/";
    printf("img_dir: %s\n", img_dir.c_str());
    if(DirectoryExists(img_dir.c_str()))
    {
        printf("Directory \"%s\" already exists\n", img_dir.c_str());
        return 1;
    }

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
    stereo_cams = new StereoMipiCameraSet(&mutex, 0, 1, camera_width, camera_height);
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

    // wait for camera thread to start getting images
    sleep(5);
    int ret = mkdir(img_dir.c_str(), 0777);
    if(ret)
    {
        printf("Create directory \"%s\" failed\n", img_dir.c_str());
        return 1;
    }
    printf("Created directory \"%s\"\n", img_dir.c_str());
    printf("start displaying frames\n");
    // system("stty raw");

    for(int i = 0; ;)
    {
        stereo_cams->get_images(img_local_l, img_local_r);

        imshow("left", img_local_l);
        imshow("right", img_local_r);

        printf("new frames displayed\n");

        int key = waitKey(50);  // get single character

        if(key == 113)  // stop capturing by pressing q
        {
            printf("exiting\n");
            break;
        }
        else if(key == 32)  // save image by pressing spacebar
        {
            printf("saving image set %d\n", i);
            imwrite(img_dir + std::to_string(i) + "_l.png", img_local_l);
            imwrite(img_dir + std::to_string(i) + "_r.png", img_local_r);
            printf("saved image set %d\n", i);
            ++i;
        }
        else  // any other key skips current image image
        {
            printf("skipping\n");
        }

    }

    // system("stty cooked");

    return 0;
}