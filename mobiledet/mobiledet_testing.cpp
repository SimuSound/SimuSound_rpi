
#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <cmath>

#include <opencv2/opencv.hpp>

#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model.h"
#include "edgetpu_c.h"

#include "mobiledet.h"

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::vector;


int main(int argc, char* argv[]) {
  if (argc != 5) {
    cerr << argv[0]
              << " <cpu or coral> <model_file> <image_file> <multithread y/n>"
              << endl;
    return 1;
  }

  const string cpu_or_coral = argv[1];
  const string model_file = argv[2];
  const string image_file = argv[3];
  const string multithread_y_n = argv[4];

  MobileDetCoral mobiledet;
  // to test cpu instead of coral, just don't have a coral plugged in
  if (cpu_or_coral == "coral")
  {
    if (mobiledet.setup(model_file))
    {
      cerr << "couldn't setup mobiledet for coral, exiting" << endl;
      return 1;
    }
  }
  else if (cpu_or_coral == "cpu")
  {
    if (mobiledet.setup_cpu(model_file))
    {
      cerr << "couldn't setup mobiledet for cpu, exiting" << endl;
      return 1;
    }
    if (multithread_y_n == "y")
    {
      mobiledet.interpreter->SetNumThreads(4);
    }
  }
  else
  {
    cerr << "invalid argument, not cpu or coral" << endl;
    return 1;
  }

  // Load image.
  cv::Mat orig_image = cv::imread(image_file, cv::IMREAD_COLOR);
  cv::imshow("orig_image", orig_image);

  cv::Mat input_image_preserve_ratio_bgr;
  input_image_preserve_ratio_bgr = resize_keep_aspect_ratio(orig_image, cv::Size(mobiledet.input_width, mobiledet.input_height), cv::Scalar(0));
  cv::imshow("input_image_preserve_ratio_bgr", input_image_preserve_ratio_bgr);

  cv::Mat input_image_preserve_ratio;
  cv::cvtColor(input_image_preserve_ratio_bgr, input_image_preserve_ratio, cv::COLOR_BGR2RGB);

  cv::waitKey(0);


  for (int i = 0;; ++i) {
    auto start = std::chrono::steady_clock::now();

    mobiledet.evaluate_direct(input_image_preserve_ratio);  // make sure Mat is 8-bit 3-channel RGB of size 320x320

    auto end = std::chrono::steady_clock::now();
    cout << "inference time: "
    << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
    << " ms" << endl;


    print_detected_objects(mobiledet.detected_objects);

    cv::Mat display_image;

    cv::resize(input_image_preserve_ratio_bgr, display_image, cv::Size(), 3, 3, cv::INTER_LINEAR);

    display_image = draw_detected_objects(mobiledet.detected_objects, display_image, 3);
    cv::imshow("display_image", display_image);
    cv::waitKey(0);
  }


  return 0;
}
