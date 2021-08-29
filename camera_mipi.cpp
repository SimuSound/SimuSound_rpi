#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>

extern "C" {

#include "bcm_host.h"
#include "interface/vcos/vcos.h"

#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_parameters_camera.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"
}

#include <opencv2/opencv.hpp>

#include "camera_mipi.h"

/*
ACTUAL DOCUMENTATION KIND OF
http://www.jvcref.com/files/PI/documentation/html/

more info:
https://blog.oklahome.net/2014/09/what-ive-learned-about-mmal-raspberry.html
https://picamera.readthedocs.io/en/release-1.13/fov.html#pipelines

TL;DR:
things are connected kind of like an audio mixer system with blocks and wires
components are blocks and they do stuff like talk to the mipi camera or render directly to the monitor
components have ports where they can send/receive data and also a control port
ports are connected via connections
ports send and receive buffer headers which are structures that have metadata plus a pointer to a buffer located elsewhere

current connection scheme is camera preview port -> splitter
can't take from camera video port directly because there is this bug
https://forum.stereopi.com/viewtopic.php?t=1114

so we send opaque data from camera preview port to splitter and splitter converts to BGR

       / preview <--> \
camera -  video        input - splitter-output1 <--> callback() -> OpenCV Mat
       \ stills

http://www.jvcref.com/files/PI/documentation/html/group___mmal_port.html#gaa723c325260a53f553c6ab7ee8038b70
*/


#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT 1

#define CAMERA_FPS 20

#define CHECK_STATUS(func, err_str) \
    do {MMAL_STATUS_T status = func; if (status != MMAL_SUCCESS) {printf("Error: %s (%d)\n", err_str, status); return -1;} } while(0);

// void CHECK_STATUS(MMAL_STATUS_T status, std::string err_str)
// {
//     if (status != MMAL_SUCCESS) {
//         printf("Error: %s (%d)\n", err_str.c_str(), status);
//         exit(1);
//     }
// }

// callback is run on separate thread
// cannot be member function as function signature would be different and incompatible with MMAL API
void video_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
    static int frame_count = 0;
    static struct timespec t1;
    struct timespec t2;
    MMAL_BUFFER_HEADER_T *new_buffer;
    PORT_USERDATA * userdata = (PORT_USERDATA *) port->userdata;
    MMAL_POOL_T *pool = userdata->splitter_port_pool;
    int frame_size = userdata->frame_width * userdata->frame_height * 3;
    // int frame_gray_size = userdata->frame_width * userdata->frame_height;

    if (frame_count == 0)
    {
        clock_gettime(CLOCK_MONOTONIC, &t1);
    }

    frame_count++;

    // TODO: not sure if this function is invoked by reentry so may deadlock if mutex is held by another thread for too long
    pthread_mutex_lock(userdata->mutex);
    mmal_buffer_header_mem_lock(buffer);
    // data is top-bottom BGR so can do this
    memcpy(userdata->image0_data, buffer->data, frame_size);
    memcpy(userdata->image1_data, buffer->data + frame_size, frame_size);
    mmal_buffer_header_mem_unlock(buffer);
    pthread_mutex_unlock(userdata->mutex);

    if (frame_count % 16 == 0) {
        // print framerate every n frame
        clock_gettime(CLOCK_MONOTONIC, &t2);
        float d = (t2.tv_sec + t2.tv_nsec / 1000000000.0) - (t1.tv_sec + t1.tv_nsec / 1000000000.0);
        float fps = 0.0;

        if (d > 0) {
            fps = frame_count / d;
        } else {
            fps = frame_count;
        }
        userdata->video_fps = fps;
       printf("  Frame = %d, Framerate = %.0f fps \n", frame_count, fps);
    }

    mmal_buffer_header_release(buffer);
    // and send one back to the port (if still open)
    // TODO: if execution is stopped at above mutex too long then buffer may not be released back and producer may not have buffer available
    if (port->is_enabled) {
        MMAL_STATUS_T status = MMAL_SUCCESS;

        new_buffer = mmal_queue_get(pool->queue);

        if (new_buffer)
            status = mmal_port_send_buffer(port, new_buffer);

        if (!new_buffer || status != MMAL_SUCCESS)
            printf("Unable to return a buffer to the video port\n");
    }
}

StereoMipiCameraSet::StereoMipiCameraSet(pthread_mutex_t *mutex, int cam_num_l, int cam_num_r, int image_width, int image_height)
{
    this->mutex = mutex;
    this->cam_num_l = cam_num_l;
    this->cam_num_r = cam_num_r;
    this->image_width = image_width;
    this->image_height = image_height;
    // this->image0 = cv::Mat(image_height, image_width, CV_8UC3);
    // this->image1 = cv::Mat(image_height, image_width, CV_8UC3);
    this->userdata.image0_data = malloc(image_width*image_height*3);
    this->userdata.image1_data = malloc(image_width*image_height*3);
}

int StereoMipiCameraSet::initialize_cameras()
{
    MMAL_COMPONENT_T *splitter = 0;
    camera = 0;
    MMAL_ES_FORMAT_T *format;
    MMAL_PORT_T *camera_video_port = NULL;
    MMAL_PORT_T *camera_preview_port = NULL;
    MMAL_PORT_T *splitter_input_port = NULL;
    MMAL_PORT_T *splitter_output_port = NULL;
    MMAL_POOL_T *splitter_port_pool;     // pool of buffers
    // PORT_USERDATA userdata;     // user-defined data
    MMAL_CONNECTION_T *camera_splitter_connection;  // Pointer to the connection from camera to splitter

    // printf("Running...\n");

    bcm_host_init();

    userdata.mutex = mutex;

    // Image resolution to be acquired for processing
    userdata.video_width = image_width;
    userdata.video_height = image_height * 2;  // 2x because of top-bottom stereo
    userdata.frame_width = image_width;
    userdata.frame_height = image_height;  // one stereo video frame has two images laid out top-bottom

    // userdata.image0_data = image0.data;
    // userdata.image1_data = image1.data;

    CHECK_STATUS(mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera), "failed to create camera");

    CHECK_STATUS(mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_SPLITTER, &splitter), "failed to create splitter");
    if (!splitter->input_num)
    {
        printf("Splitter doesn't have any input port\n");
        return -1;
    }

    camera_video_port = camera->output[MMAL_CAMERA_VIDEO_PORT];
    camera_preview_port = camera->output[MMAL_CAMERA_PREVIEW_PORT];
    splitter_input_port = splitter->input[0];
    splitter_output_port = splitter->output[0];

    {
        MMAL_PARAMETER_STEREOSCOPIC_MODE_T stereo_config = {
            { MMAL_PARAMETER_STEREOSCOPIC_MODE, sizeof (stereo_config)},
            .mode = MMAL_STEREOSCOPIC_MODE_TOP_BOTTOM,
            .decimate = MMAL_FALSE,
            .swap_eyes = MMAL_FALSE
        };
        CHECK_STATUS(mmal_port_parameter_set(camera_video_port, &stereo_config.hdr), "failed to set video top-bottom stereo mode");
        CHECK_STATUS(mmal_port_parameter_set(camera_preview_port, &stereo_config.hdr), "failed to set preview top-bottom stereo mode");
    }

    printf("left camera num: %d\n", cam_num_l);
    CHECK_STATUS(mmal_port_parameter_set_uint32(camera->control, MMAL_PARAMETER_CAMERA_NUM, cam_num_l), "failed to set camera num");


    // https://picamera.readthedocs.io/en/release-1.13/fov.html?highlight=pixel%20binning#sensor-modes
    CHECK_STATUS(mmal_port_parameter_set_uint32(camera->control, MMAL_PARAMETER_CAMERA_CUSTOM_SENSOR_CONFIG, 5), "failed to set sensor mode");

    {
        MMAL_PARAMETER_CAMERA_CONFIG_T cam_config = {
            { MMAL_PARAMETER_CAMERA_CONFIG, sizeof (cam_config)},
            .max_stills_w = userdata.video_width,
            .max_stills_h = userdata.video_height,
            .stills_yuv422 = 0,
            .one_shot_stills = 1,
            .max_preview_video_w = userdata.video_width,
            .max_preview_video_h = userdata.video_height,
            .num_preview_video_frames = 2,
            .stills_capture_circular_buffer_height = 0,
            .fast_preview_resume = 0,
            .use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC
        };
        mmal_port_parameter_set(camera->control, &cam_config.hdr);
    }

    format = camera_preview_port->format;
    format->encoding = MMAL_ENCODING_OPAQUE;
    format->encoding_variant = MMAL_ENCODING_I420;
    format->es->video.width = userdata.video_width;
    format->es->video.height = userdata.video_width;
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = userdata.video_width;
    format->es->video.crop.height = userdata.video_height;
    format->es->video.frame_rate.num = CAMERA_FPS;
    format->es->video.frame_rate.den = 1;

    CHECK_STATUS(mmal_port_format_commit(camera_preview_port), "failed to commit camera preview port format");

    // format = camera_video_port->format;
    // format->encoding = MMAL_ENCODING_OPAQUE;
    // format->encoding_variant = MMAL_ENCODING_I420;
    // format->es->video.width = userdata.video_width;
    // format->es->video.height = userdata.video_width;
    // format->es->video.crop.x = 0;
    // format->es->video.crop.y = 0;
    // format->es->video.crop.width = userdata.video_width;
    // format->es->video.crop.height = userdata.video_height;
    // format->es->video.frame_rate.num = CAMERA_FPS;
    // format->es->video.frame_rate.den = 1;

    mmal_format_copy(camera_video_port->format, camera_preview_port->format);
    CHECK_STATUS(mmal_port_format_commit(camera_video_port), "failed to commit camera video port format");

    mmal_format_copy(splitter->input[0]->format, camera_preview_port->format);
    CHECK_STATUS(mmal_port_format_commit(splitter->input[0]), "failed to set format on splitter input port");

    CHECK_STATUS(mmal_connection_create(&camera_splitter_connection, camera_preview_port, splitter_input_port,
        MMAL_CONNECTION_FLAG_TUNNELLING | MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT), "failed to create camera splitter connection");
    CHECK_STATUS(mmal_connection_enable(camera_splitter_connection), "failed to enable camera splitter connection");


    mmal_format_copy(splitter_output_port->format, splitter_input_port->format);
    format = splitter_output_port->format;
    format->encoding = MMAL_ENCODING_BGR24;
    format->encoding_variant = MMAL_ENCODING_BGR24;

    CHECK_STATUS(mmal_port_format_commit(splitter_output_port), "failed to set format on splitter output port");

    CHECK_STATUS(mmal_component_enable(splitter), "failed to enable splitter");


    splitter_output_port->buffer_size = userdata.video_width * userdata.video_height * 3;
    splitter_output_port->buffer_num = 1; // how many buffers of that size will be used
    printf("splitter output buffer_size = %d\n", splitter_output_port->buffer_size);
    // crate pool form camera video port
    splitter_port_pool = (MMAL_POOL_T *) mmal_port_pool_create(splitter_output_port, splitter_output_port->buffer_num, splitter_output_port->buffer_size);
    userdata.splitter_port_pool = splitter_port_pool;
    splitter_output_port->userdata = (struct MMAL_PORT_USERDATA_T *) &userdata;  // vscode false positive error squiggle under equals sign (C/C++(513))


    CHECK_STATUS(mmal_port_enable(splitter_output_port, video_buffer_callback), "failed to enable splitter output port");

    CHECK_STATUS(mmal_component_enable(camera), "failed to enable camera");

    {
        // Send all the buffers to the camera video port
        int num = mmal_queue_length(splitter_port_pool->queue);
        int q;
        for (q = 0; q < num; q++) {
            MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(splitter_port_pool->queue);
            if (!buffer) {
                printf("Error: failed to get a required buffer %d from pool queue", q);
                return -1;
            }
            if (mmal_port_send_buffer(splitter_output_port, buffer) != MMAL_SUCCESS) {
                printf("Error: failed to send a buffer to encoder output port (%d)", q);
                return -1;
            }
        }
    }

    CHECK_STATUS(mmal_port_parameter_set_int32(camera_preview_port, MMAL_PARAMETER_ROTATION, 180), "failed to set preview rotation");
    CHECK_STATUS(mmal_port_parameter_set_int32(camera_video_port, MMAL_PARAMETER_ROTATION, 180), "failed to set video rotation");
    // CHECK_STATUS(mmal_port_parameter_set_boolean(camera->output[MMAL_CAMERA_VIDEO_PORT], MMAL_PARAMETER_CAPTURE, 1), "failed to start capture");

    // while (1);

    return 0;
}

int StereoMipiCameraSet::start_capture()
{
    CHECK_STATUS(mmal_port_parameter_set_boolean(camera->output[MMAL_CAMERA_VIDEO_PORT], MMAL_PARAMETER_CAPTURE, 1), "failed to start capture");
    return 0;
}

int StereoMipiCameraSet::get_images(cv::Mat &img_l, cv::Mat &img_r)
{
    pthread_mutex_lock(mutex);
    // cv::Mat img_l_temp(this->image_height, this->image_width, CV_8UC3, this->userdata.image0_data);
    // cv::Mat img_r_temp(this->image_height, this->image_width, CV_8UC3, this->userdata.image1_data);
    memcpy(img_l.data, this->userdata.image0_data, this->image_width*this->image_height*3);
    memcpy(img_r.data, this->userdata.image1_data, this->image_width*this->image_height*3);
    pthread_mutex_unlock(mutex);
    // img_l_temp.copyTo(img_l);
    // img_r_temp.copyTo(img_r);
    // cv::imshow("img_l", img_l);
    // cv::imshow("img_r", img_r);
    // cv::waitKey(1000);
    return 0;
}

int StereoMipiCameraSet::stop_capture()
{
    CHECK_STATUS(mmal_port_parameter_set_boolean(camera->output[MMAL_CAMERA_VIDEO_PORT], MMAL_PARAMETER_CAPTURE, 0), "failed to stop capture");
    return 0;
}
