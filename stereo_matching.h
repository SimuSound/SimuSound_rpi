#ifndef STEREO_MATCHING_H
#define STEREO_MATCHING_H

#include <opencv2/opencv.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/ximgproc.hpp>

#define SGBM_WINDOW_SIZE 9

void cv_stereo_16sc1_to_8uc1(const cv::Mat &stereo_16sc1, cv::Mat &cv_8uc1);


struct StereoParameters
{
    int minDisparity = 0;
    int numDisparities = 128;
    int blockSize = 9;
    int P1 = 8*3*SGBM_WINDOW_SIZE*SGBM_WINDOW_SIZE;
    int P2 = 32*3*SGBM_WINDOW_SIZE*SGBM_WINDOW_SIZE;
    int disp12MaxDiff = 0;
    int preFilterCap = 0;
    int uniquenessRatio = 0;
    int speckleWindowSize = 0;
    int speckleRange = 0;
    int mode = cv::StereoSGBM::MODE_SGBM;
};

class StereoUndistort
{
public:
    int setup_stereo_undistort(int img_width, int img_height, std::string L1_mat_path, std::string L2_mat_path, std::string R1_mat_path, std::string R2_mat_path);
    void undistort(const cv::Mat &img_l_in, const cv::Mat &img_r_in, cv::Mat &img_l_out, cv::Mat &img_r_out);

    int read_float_file_into_array(long len, std::string path, float *buf);

    int img_width;
    int img_height;
    cv::Mat L1;
    cv::Mat L2;
    cv::Mat R1;
    cv::Mat R2;
};

class StereoMatcherBM
{
public:
    int setup_stereo_matching(StereoParameters params);
    void stereo_match(const cv::Mat &img_l, const cv::Mat &img_r, cv::Mat &disparity_map);

    cv::Ptr<cv::StereoBM> stereo_matcher;
    StereoParameters params;
};

class StereoMatcherSGBM
{
public:
    int setup_stereo_matching(StereoParameters params);
    void stereo_match(const cv::Mat &img_l, const cv::Mat &img_r, cv::Mat &disparity_map);

    cv::Ptr<cv::StereoSGBM> stereo_matcher;
    StereoParameters params;
};

struct WLSFilterParameters
{
    int ddr = 0.33;
    double lambda = 8000.0;
    double sigma = 1.5;
    int blockSize = 9;
};

class WLSFilter
{
public:
    int setup_wls_filter(WLSFilterParameters params, cv::Ptr<cv::StereoMatcher> left_matcher);
    void filter(const cv::Mat &img_disparity_in, const cv::Mat &img_in, cv::Mat &img_disparity_filtered);

    int depthDisRadius;
    WLSFilterParameters params;
    cv::Ptr<cv::ximgproc::DisparityWLSFilter> wls_filter;
    cv::Ptr<cv::StereoMatcher> left_matcher;
};

#endif /* STEREO_MATCHING_H */
