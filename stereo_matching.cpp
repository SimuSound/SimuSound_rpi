#include <cmath>
#include <cstdio>

#include <opencv2/opencv.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/ximgproc.hpp>

#include "stereo_matching.h"

using namespace cv;


Rect computeROI(Size2i src_sz, Ptr<StereoMatcher> matcher_instance)
{
    int min_disparity = matcher_instance->getMinDisparity();
    int num_disparities = matcher_instance->getNumDisparities();
    int block_size = matcher_instance->getBlockSize();

    int bs2 = block_size/2;
    int minD = min_disparity, maxD = min_disparity + num_disparities - 1;

    int xmin = maxD + bs2;
    int xmax = src_sz.width + minD - bs2;
    int ymin = bs2;
    int ymax = src_sz.height - bs2;

    Rect r(xmin, ymin, xmax - xmin, ymax - ymin);
    return r;
}


void cv_stereo_16sc1_to_8uc1(const cv::Mat &stereo_16sc1, cv::Mat &cv_8uc1)
{
    Mat temp;
    temp = stereo_16sc1 / 16;
    temp.convertTo(cv_8uc1, CV_8UC1);
}


int StereoUndistort::read_float_file_into_array(long len, std::string path, float *buf)
{
    FILE *file = fopen(path.c_str(), "rb");
    if(file == NULL)
    {
        printf("opening %s failed\n", path.c_str());
        return 1;
    }
    long ret;
    ret = fseek(file, 0L, SEEK_END);
    if(ret)
    {
        printf("Reading to end of %s failed\n", path.c_str());
        return 1;
    }
    long sz = ftell(file)/sizeof(float);
    if(sz != len)
    {
        printf("size of file was %ld but expected %ld\n", sz, len);
        return 1;
    }
    rewind(file);

    ret = fread((void*)buf, sizeof(float), len, file);
    if(ret != len)
    {
        printf("read %ld elements but expected %ld\n", ret, len);
        return 1;
    }

    fclose(file);

    return 0;
}

int StereoUndistort::setup_stereo_undistort(int img_width, int img_height, std::string L1_mat_path, std::string L2_mat_path, std::string R1_mat_path, std::string R2_mat_path)
{
    long size = img_width * img_height;
    int ret;

    this->img_width = img_width;
    this->img_height = img_height;
    cv::Mat temp;

    temp = cv::Mat::zeros(cv::Size(img_width, img_height), CV_32FC1);
    ret = read_float_file_into_array(size, L1_mat_path, (float *)temp.data);
    if(ret)
    {
        return ret;
    }
    temp.copyTo(this->L1);
    printf("L1[0]: %f\n", L1.at<float>(0,0));

    temp = cv::Mat::zeros(cv::Size(img_width, img_height), CV_32FC1);
    ret = read_float_file_into_array(size, L2_mat_path, (float *)temp.data);
    if(ret)
    {
        return ret;
    }
    temp.copyTo(this->L2);
    printf("L2[0]: %f\n", L2.at<float>(0,0));

    temp = cv::Mat::zeros(cv::Size(img_width, img_height), CV_32FC1);
    ret = read_float_file_into_array(size, R1_mat_path, (float *)temp.data);
    if(ret)
    {
        return ret;
    }
    temp.copyTo(this->R1);
    printf("R1[0]: %f\n", R1.at<float>(0,0));

    temp = cv::Mat::zeros(cv::Size(img_width, img_height), CV_32FC1);
    ret = read_float_file_into_array(size, R2_mat_path, (float *)temp.data);
    if(ret)
    {
        return ret;
    }
    temp.copyTo(this->R2);
    printf("R2[0]: %f\n", R2.at<float>(0,0));

    return 0;
}

void StereoUndistort::undistort(const cv::Mat &img_l_in, const cv::Mat &img_r_in, cv::Mat &img_l_out, cv::Mat &img_r_out)
{
    cv::remap(img_l_in, img_l_out, this->L1, this->L2, cv::INTER_LINEAR);
    cv::remap(img_r_in, img_r_out, this->R1, this->R2, cv::INTER_LINEAR);
}


int StereoMatcherBM::setup_stereo_matching(StereoParameters params)
{
    this->params = params;
    stereo_matcher = StereoBM::create(
        params.numDisparities,
        params.blockSize
    );
    return 0;
}

/*
    Params:
        img_l         - Mat CV_8UC3 or CV_8UC1
        img_r         - Mat CV_8UC3 or CV_8UC1
        disparity_map - Mat CV_16SC1
*/
void StereoMatcherBM::stereo_match(const cv::Mat &img_l, const cv::Mat &img_r, cv::Mat &disparity_map)
{
    stereo_matcher->compute(img_l, img_r, disparity_map);
    // result is in CV_16SC1 fixed point with 4 LSbits as fractional values and -1 values indicating no disparity found
    // turn -1 values into 0's
    threshold(disparity_map, disparity_map, 0, params.numDisparities * 16, THRESH_TOZERO);
}




int StereoMatcherSGBM::setup_stereo_matching(StereoParameters params)
{
    this->params = params;
    stereo_matcher = StereoSGBM::create(
        params.minDisparity,
        params.numDisparities,
        params.blockSize,
        params.P1,
        params.P2,
        params.disp12MaxDiff,
        params.preFilterCap,
        params.uniquenessRatio,
        params.speckleWindowSize,
        params.speckleRange,
        params.mode
    );
    return 0;
}

/*
    Params:
        img_l         - Mat CV_8UC3 or CV_8UC1
        img_r         - Mat CV_8UC3 or CV_8UC1
        disparity_map - Mat CV_16SC1
*/
void StereoMatcherSGBM::stereo_match(const cv::Mat &img_l, const cv::Mat &img_r, cv::Mat &disparity_map)
{
    stereo_matcher->compute(img_l, img_r, disparity_map);
    // result is in CV_16SC1 fixed point with 4 LSbits as fractional values and -1 values indicating no disparity found
    // turn -1 values into 0's
    threshold(disparity_map, disparity_map, 0, params.numDisparities * 16, THRESH_TOZERO);
}


int WLSFilter::setup_wls_filter(WLSFilterParameters params, Ptr<StereoMatcher> left_matcher)
{
    this->params = params;
    this->depthDisRadius = (int)ceil(params.ddr * params.blockSize);
    this->wls_filter = ximgproc::createDisparityWLSFilterGeneric(false);
    wls_filter->setLambda(params.lambda);
    wls_filter->setSigmaColor(params.sigma);
    wls_filter->setDepthDiscontinuityRadius(this->depthDisRadius);
    this->left_matcher = left_matcher;

    return 0;
}

void WLSFilter::filter(const cv::Mat &img_disparity_in, const cv::Mat &img_in, cv::Mat &img_disparity_filtered)
{
    wls_filter->filter(img_disparity_in, img_in, img_disparity_filtered, Mat(), computeROI(img_in.size(), left_matcher));
}
