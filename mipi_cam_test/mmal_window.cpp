#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>

#include "bcm_host.h"
#include "interface/vcos/vcos.h"

#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_parameters_camera.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"

#include <opencv2/opencv.hpp>

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
// #define MMAL_CAMERA_CAPTURE_PORT 2

typedef struct {
    uint32_t video_width;
    uint32_t video_height;
    uint32_t frame_width;
    uint32_t frame_height;
    float video_fps;
    MMAL_POOL_T *splitter_port_pool;
    void *image0_data;
    void *image1_data;
    void *full_image_data;
    VCOS_SEMAPHORE_T complete_semaphore;
} PORT_USERDATA;

void check_status(MMAL_STATUS_T status, std::string err_str)
{
    if (status != MMAL_SUCCESS) {
        printf(err_str.c_str(), status);
        exit(1);
    }
}

// callback is run on separate thread
static void video_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
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

    mmal_buffer_header_mem_lock(buffer);
    memcpy(userdata->full_image_data, buffer->data, frame_size*2);
    // memcpy(userdata->full_image_data + frame_gray_size, buffer->data + frame_size, frame_gray_size);
    memcpy(userdata->image0_data, buffer->data, frame_size);
    memcpy(userdata->image1_data, buffer->data + frame_size, frame_size);
    mmal_buffer_header_mem_unlock(buffer);
    if (vcos_semaphore_trywait(&(userdata->complete_semaphore)) != VCOS_SUCCESS) {
        vcos_semaphore_post(&(userdata->complete_semaphore));
    }

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
    if (port->is_enabled) {
        MMAL_STATUS_T status;

        new_buffer = mmal_queue_get(pool->queue);

        if (new_buffer)
            status = mmal_port_send_buffer(port, new_buffer);

        if (!new_buffer || status != MMAL_SUCCESS)
            printf("Unable to return a buffer to the video port\n");
    }
}



int main(int argc, char** argv) {
    MMAL_COMPONENT_T *splitter = 0;
    MMAL_COMPONENT_T *camera = 0;
    // MMAL_COMPONENT_T *preview = 0;
    MMAL_ES_FORMAT_T *format;
    // MMAL_PORT_T *camera_preview_port = NULL;
    MMAL_PORT_T *camera_video_port = NULL;
    MMAL_PORT_T *camera_preview_port = NULL;
    MMAL_PORT_T *splitter_input_port = NULL;
    MMAL_PORT_T *splitter_output_port = NULL;
    // MMAL_PORT_T *camera_still_port = NULL;
    // MMAL_PORT_T *preview_input_port = NULL;
    MMAL_POOL_T *splitter_port_pool;     // pool of buffers
    PORT_USERDATA userdata;     // user-defined data
    int camera_num = 0;
    MMAL_CONNECTION_T *camera_splitter_connection;  // Pointer to the connection from camera to splitter

    // MMAL_CONNECTION_T *camera_preview_connection = 0;

    printf("Running...\n");

    // cv::setNumThreads(0);


    bcm_host_init();


    // Image resolution to be acquired for processing
    userdata.video_width = 768;
    userdata.video_height = 1024;
    userdata.frame_width = userdata.video_width;
    userdata.frame_height = userdata.video_height / 2;  // one stereo video frame has two images laid out top-bottom

    static cv::Mat image0(userdata.frame_height, userdata.frame_width, CV_8UC3);
    static cv::Mat image1(userdata.frame_height, userdata.frame_width, CV_8UC3);
    static cv::Mat full_image(userdata.video_height, userdata.video_width, CV_8UC3);
    userdata.image0_data = image0.data;
    userdata.image1_data = image1.data;
    userdata.full_image_data = full_image.data;

    check_status(mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera), "Error: create camera %d\n");

    check_status(mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_SPLITTER, &splitter), "Error: create splitter %d\n");
    if (!splitter->input_num)
    {
        printf("Splitter doesn't have any input port\n");
        return -1;
    }

    camera_video_port = camera->output[MMAL_CAMERA_VIDEO_PORT];		// this port is for the frame to be capture as raw data
    camera_preview_port = camera->output[MMAL_CAMERA_PREVIEW_PORT];		// this port is for the frame to be capture as raw data
    splitter_input_port = splitter->input[0];
    splitter_output_port = splitter->output[0];

    {
        MMAL_PARAMETER_STEREOSCOPIC_MODE_T stereo_config = {
            { MMAL_PARAMETER_STEREOSCOPIC_MODE, sizeof (stereo_config)},
            .mode = MMAL_STEREOSCOPIC_MODE_TOP_BOTTOM,
            .decimate = MMAL_FALSE,
            .swap_eyes = MMAL_FALSE
        };
        check_status(mmal_port_parameter_set(camera_video_port, &stereo_config.hdr), "Failed to set top-bottom stereo mode %d\n");
        check_status(mmal_port_parameter_set(camera_preview_port, &stereo_config.hdr), "Failed to set top-bottom stereo mode %d\n");
    }

    camera_num = argc > 1 ? atoi(argv[1]) : 0;
    printf("camera num: %d\n", camera_num);
    check_status(mmal_port_parameter_set_uint32(camera->control, MMAL_PARAMETER_CAMERA_NUM, camera_num), "Failed to set camera num %d\n");


    // https://picamera.readthedocs.io/en/release-1.13/fov.html?highlight=pixel%20binning#sensor-modes
    check_status(mmal_port_parameter_set_uint32(camera->control, MMAL_PARAMETER_CAMERA_CUSTOM_SENSOR_CONFIG, 5), "Could not set sensor mode %d\n");

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
    format->es->video.frame_rate.num = 15;
    format->es->video.frame_rate.den = 1;

    check_status(mmal_port_format_commit(camera_preview_port), "Error: unable to commit camera preview port format %d\n");

    format = camera_video_port->format;
    format->encoding = MMAL_ENCODING_OPAQUE;
    format->encoding_variant = MMAL_ENCODING_I420;
    format->es->video.width = userdata.video_width;
    format->es->video.height = userdata.video_width;
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = userdata.video_width;
    format->es->video.crop.height = userdata.video_height;
    format->es->video.frame_rate.num = 15;
    format->es->video.frame_rate.den = 1;

    check_status(mmal_port_format_commit(camera_video_port), "Error: unable to commit camera video port format %d\n");

    mmal_format_copy(splitter->input[0]->format, camera_preview_port->format);
    check_status(mmal_port_format_commit(splitter->input[0]), "Unable to set format on splitter input port\n");

    check_status(mmal_connection_create(&camera_splitter_connection, camera_preview_port, splitter_input_port,
        MMAL_CONNECTION_FLAG_TUNNELLING | MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT), "failed to create camera splitter connection\n");
    check_status(mmal_connection_enable(camera_splitter_connection), "failed to enable camera splitter connection\n");


    mmal_format_copy(splitter_output_port->format, splitter_input_port->format);
    format = splitter_output_port->format;
    format->encoding = MMAL_ENCODING_BGR24;
    format->encoding_variant = MMAL_ENCODING_BGR24;

    check_status(mmal_port_format_commit(splitter_output_port), "Unable to set format on splitter output port\n");

    check_status(mmal_component_enable(splitter), "splitter component couldn't be enabled\n");

    
    splitter_output_port->buffer_size = userdata.video_width * userdata.video_height * 3;
    splitter_output_port->buffer_num = 1; // how many buffers of that size will be used
    printf("splitter output buffer_size = %d\n", splitter_output_port->buffer_size);
    // crate pool form camera video port
    splitter_port_pool = (MMAL_POOL_T *) mmal_port_pool_create(splitter_output_port, splitter_output_port->buffer_num, splitter_output_port->buffer_size);
    userdata.splitter_port_pool = splitter_port_pool;
    splitter_output_port->userdata = (struct MMAL_PORT_USERDATA_T *) &userdata;  // vscode false positive error squiggle under equals sign (C/C++(513))


    check_status(mmal_port_enable(splitter_output_port, video_buffer_callback), "Error: unable to enable splitter output port %d\n");

    check_status(mmal_component_enable(camera), "Error: unable to enable camera %d\n");
    
    {
        // Send all the buffers to the camera video port
        int num = mmal_queue_length(splitter_port_pool->queue);
        int q;
        for (q = 0; q < num; q++) {
            MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(splitter_port_pool->queue);
            if (!buffer) {
                printf("Unable to get a required buffer %d from pool queue\n", q);
            }
            if (mmal_port_send_buffer(splitter_output_port, buffer) != MMAL_SUCCESS) {
                printf("Unable to send a buffer to encoder output port (%d)\n", q);
            }
        }
    }

    check_status(mmal_port_parameter_set_int32(camera_preview_port, MMAL_PARAMETER_ROTATION, 180), "Failed to set rotation\n");
    check_status(mmal_port_parameter_set_int32(camera_video_port, MMAL_PARAMETER_ROTATION, 180), "Failed to set rotation\n");

    check_status(mmal_port_parameter_set_boolean(camera_video_port, MMAL_PARAMETER_CAPTURE, 1), "Failed to start capture\n");

    vcos_semaphore_create(&userdata.complete_semaphore, "mmal_opencv_demo-sem", 0);

    cv::namedWindow("image0", cv::WINDOW_NORMAL);
    cv::resizeWindow("image0", userdata.frame_width, userdata.frame_height);
    cv::namedWindow("image1", cv::WINDOW_NORMAL);
    cv::resizeWindow("image1", userdata.frame_width, userdata.frame_height);

    while (1)
    {
        // printf("main thread tid: %lu\n", pthread_self());
        if (vcos_semaphore_wait(&(userdata.complete_semaphore)) == VCOS_SUCCESS) 
        {
            // cv::cvtColor(full_image, full_image, cv::COLOR_RGB2BGR);
            cv::imshow("image0", image0);
            cv::imshow("image1", image1);
            cv::imshow("full_image", full_image);
            cv::waitKey(10);
            // usleep(10 * 1000);
        }
    }

    return 0;
}