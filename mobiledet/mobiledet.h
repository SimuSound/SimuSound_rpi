#ifndef MOBILEDET_H
#define MOBILEDET_H

#include <vector>
#include <string>
#include <memory>

#include "edgetpu_c.h"
#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model.h"

#include <opencv2/opencv.hpp>

struct DetectedObjects
{
  int top;
  int left;
  int bottom;
  int right;

  float confidence;
  int class_id;
};


extern const std::vector<std::string> mobiledet_labels;


void print_tensor_sizes(const TfLiteTensor* tensor, std::string name);
cv::Mat resize_keep_aspect_ratio(const cv::Mat &input, const cv::Size &dstSize, const cv::Scalar &bgcolor);
void print_detected_objects(std::vector<DetectedObjects> &detected_objects);
cv::Mat draw_detected_objects(std::vector<DetectedObjects> &detected_objects, cv::Mat background_image, float dist_mult, float conf_threshold = 0.4);


class MobileDetCoral
{
public:

  int input_width;
  int input_height;
  std::vector<DetectedObjects> detected_objects;
  int max_objects;

  int is_cpu = -1;  // 0 is coral, 1 is cpu, -1 is unknown (means error)
  
  std::unique_ptr<tflite::Interpreter> interpreter;

  int setup(std::string model_file);
  int setup_cpu(std::string model_file);
  // make sure input Mat is contiguous
  int evaluate(cv::Mat input_image);  // assumes BGR Mat and will convert to RGB and resize
  int evaluate_direct(cv::Mat input_image);  // no resizing or BGR to RGB conversion


protected:
  edgetpu_device device;
  std::unique_ptr<tflite::FlatBufferModel> model;
  tflite::ops::builtin::BuiltinOpResolver resolver;
  TfLiteDelegate* delegate;
};


#endif /* MOBILEDET_H */
