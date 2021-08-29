#ifndef UTILS_H
#define UTILS_H

bool DirectoryExists(const char* pzPath);
std::string fourcc_to_str(int fourcc);
void downsample_vertical(cv::Mat &img_in, cv::Mat &img_out, int factor = 2);
void print_available_camera_backends();


#endif /* UTILS_H */
