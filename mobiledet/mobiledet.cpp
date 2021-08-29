#include <vector>
#include <string>
#include <iostream>
#include <cstdint>
#include <memory>

#include <unistd.h>

#include <opencv2/opencv.hpp>

#include "edgetpu_c.h"
#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model.h"

#include "mobiledet.h"

const std::vector<std::string> mobiledet_labels = {
  "person",
  "bicycle",
  "car",
  "motorcycle",
  "airplane",
  "bus",
  "train",
  "truck",
  "boat",
  "traffic light",
  "stop sign",
  "bench",
  "dog",
  "umbrella",
  "chair",
  "couch",
  "bed",
  "dining table",
  "toilet",
  "laptop",
  "sink",
  "refrigerator",
  "door"
};


void print_tensor_sizes(const TfLiteTensor* tensor, std::string name)
{
  std::cout << name << " number of dimensions: " << tensor->dims->size << std::endl;
  std::cout << "sizes: ";
  for(int i = 0; i < tensor->dims->size - 1; ++i)
  {
    std::cout << tensor->dims->data[i] << ", ";
  }
  std::cout << tensor->dims->data[tensor->dims->size - 1] << std::endl;
}


// https://stackoverflow.com/a/40048370/15920289
cv::Mat resize_keep_aspect_ratio(const cv::Mat &input, const cv::Size &dstSize, const cv::Scalar &bgcolor)
{
    cv::Mat output;

    double h1 = dstSize.width * (input.rows/(double)input.cols);
    double w2 = dstSize.height * (input.cols/(double)input.rows);
    if( h1 <= dstSize.height) {
        cv::resize( input, output, cv::Size(dstSize.width, h1));
    } else {
        cv::resize( input, output, cv::Size(w2, dstSize.height));
    }

    int top = (dstSize.height-output.rows) / 2;
    int down = (dstSize.height-output.rows+1) / 2;
    int left = (dstSize.width - output.cols) / 2;
    int right = (dstSize.width - output.cols+1) / 2;

    cv::copyMakeBorder(output, output, top, down, left, right, cv::BORDER_CONSTANT, bgcolor );

    return output;
}



void print_detected_objects(std::vector<DetectedObjects> &detected_objects)
{
  for(int i = 0; i < detected_objects.size(); ++i)
  {
    std::cout << "num: " << i << std::endl;
    std::cout << "label name: " << mobiledet_labels[detected_objects[i].class_id] << ", id: " << detected_objects[i].class_id << std::endl;
    std::cout << "confidence: " << detected_objects[i].confidence << std::endl;
    std::cout << "bbox (tldr): " << detected_objects[i].top << ", " << detected_objects[i].left << ", " << detected_objects[i].bottom << ", " << detected_objects[i].right << std::endl;
  }
}


cv::Mat draw_detected_objects(std::vector<DetectedObjects> &detected_objects, cv::Mat background_image, float dist_mult, float conf_threshold)
{
  cv::Mat output_image = background_image.clone();

  for(int i = 0; i < detected_objects.size(); ++i)
  {
    if (detected_objects[i].confidence < conf_threshold)
    {
      continue;
    }

    cv::Point top_left = cv::Point(detected_objects[i].left * dist_mult, detected_objects[i].top * dist_mult);
    cv::Point bottom_right = cv::Point(detected_objects[i].right * dist_mult, detected_objects[i].bottom * dist_mult);

    cv::rectangle(
      output_image,
      top_left,
      bottom_right,
      cv::Scalar(255, 0, 0),
      1,
      cv::LINE_8
    );

    char text_cstr[100];
    sprintf(text_cstr, "%s %.2f", mobiledet_labels[detected_objects[i].class_id].c_str(), detected_objects[i].confidence);
    std::string text(text_cstr);
    cv::putText(
      output_image,
      text,
      top_left,
      cv::FONT_HERSHEY_DUPLEX,
      0.5,
      cv::Scalar(255, 0, 0),
      1,
      cv::LINE_4
    );
  }

  return output_image;
}



int MobileDetCoral::setup(std::string model_file)
{
  // must check return value, if not 0 then should call setup_cpu()

  std::cout << "mobiledet setup coral" << std::endl;
  // Find TPU device.
  size_t num_devices;
  std::unique_ptr<edgetpu_device, decltype(&edgetpu_free_devices)> devices(
      edgetpu_list_devices(&num_devices), &edgetpu_free_devices);

  if (num_devices == 0) {
    std::cerr << "No connected TPU found" << std::endl;
    return 1;
  }
  device = devices.get()[0];

  // Load model.
  model = tflite::FlatBufferModel::BuildFromFile(model_file.c_str());
  if (!model) {
    std::cerr << "Cannot read model from " << model_file << std::endl;
    return 1;
  }

  // Create interpreter.
  if (tflite::InterpreterBuilder(*model, resolver)(&interpreter) != kTfLiteOk) {
    std::cerr << "Cannot create interpreter" << std::endl;
    return 1;
  }

  // https://coral.ai/docs/reference/cpp/edgetpu/#_CPPv4N7edgetpu14EdgeTpuManager10OpenDeviceE10DeviceTypeRKNSt6stringERK13DeviceOptions
  // edgetpu_verbosity(10);

  edgetpu_option option;
  option.name = "Performance";
  option.value = "Low";

  for (int i = 0; i < 4; ++i) {
    delegate = edgetpu_create_delegate(device.type, device.path, &option, 1);
    if (delegate) {
      break;
    }
    else {
      std::cerr << "Cannot create delegate " << i << std::endl;
    }
    usleep(200*1000);
  }

  if (!delegate) {
      std::cerr << "Failed to create delegate." << std::endl;
      return 1;
  }

  // edgetpu_verbosity(0);

  // std::unique_ptr<TfLiteDelegate, void (*)(TfLiteDelegate*)> my_type1 = {delegate, edgetpu_free_delegate};
  interpreter->ModifyGraphWithDelegate(std::unique_ptr<TfLiteDelegate, void (*)(TfLiteDelegate*)>(delegate, edgetpu_free_delegate));

  // Allocate tensors.
  if (interpreter->AllocateTensors() != kTfLiteOk) {
    std::cerr << "Cannot allocate interpreter tensors" << std::endl;
    return 1;
  }

  // Set interpreter input.
  const auto* input_tensor = interpreter->input_tensor(0);
  print_tensor_sizes(input_tensor, "input_tensor");

  if (input_tensor->type != kTfLiteUInt8 ||
      input_tensor->dims->data[0] != 1)
  {
    std::cerr << "bad input tensor type or dimensions, could be wrong or corrupt model file" << std::endl;
    return 1;
  }

  input_height = input_tensor->dims->data[1];
  input_width = input_tensor->dims->data[2];

  const auto* output_classes_tensor = interpreter->output_tensor(1);
  print_tensor_sizes(output_classes_tensor, "output_classes_tensor");
  max_objects = output_classes_tensor->dims->data[1];


  detected_objects.clear();
  detected_objects.reserve(max_objects);

  is_cpu = 0;

  return 0;
}


int MobileDetCoral::setup_cpu(std::string model_file)
{
  std::cout << "mobiledet setup cpu" << std::endl;
  // Load model.
  model = tflite::FlatBufferModel::BuildFromFile(model_file.c_str());
  if (!model) {
    std::cerr << "Cannot read model from " << model_file << std::endl;
    return 1;
  }

  // Create interpreter.
  if (tflite::InterpreterBuilder(*model, resolver)(&interpreter) != kTfLiteOk) {
    std::cerr << "Cannot create interpreter" << std::endl;
    return 1;
  }

  // Allocate tensors.
  if (interpreter->AllocateTensors() != kTfLiteOk) {
    std::cerr << "Cannot allocate interpreter tensors" << std::endl;
    return 1;
  }

  // Set interpreter input.
  const auto* input_tensor = interpreter->input_tensor(0);
  print_tensor_sizes(input_tensor, "input_tensor");

  if (input_tensor->type != kTfLiteUInt8 ||
      input_tensor->dims->data[0] != 1)
  {
    std::cerr << "bad input tensor type or dimensions, could be wrong or corrupt model file" << std::endl;
    return 1;
  }

  input_height = input_tensor->dims->data[1];
  input_width = input_tensor->dims->data[2];

  const auto* output_classes_tensor = interpreter->output_tensor(1);
  print_tensor_sizes(output_classes_tensor, "output_classes_tensor");
  max_objects = output_classes_tensor->dims->data[1];


  detected_objects.clear();
  detected_objects.reserve(max_objects);

  is_cpu = 1;

  return 0;
}


int MobileDetCoral::evaluate(cv::Mat input_image)
{
  if(! input_image.isContinuous())
  {
    std::cout << "input_image Mat is not continuous for some reason, exiting" << std::endl;
    return 1;
  }
  cv::Mat input_image_preserve_ratio;
  input_image_preserve_ratio = resize_keep_aspect_ratio(input_image, cv::Size(input_width,input_height), cv::Scalar(0));
  cv::cvtColor(input_image_preserve_ratio, input_image_preserve_ratio, cv::COLOR_BGR2RGB);

  return MobileDetCoral::evaluate_direct(input_image_preserve_ratio);
}


int MobileDetCoral::evaluate_direct(cv::Mat input_image_preserve_ratio)
{
  if(! input_image_preserve_ratio.isContinuous())
  {
    std::cout << "input_image_preserve_ratio Mat is not continuous for some reason, exiting" << std::endl;
    return 1;
  }

  memcpy(interpreter->typed_input_tensor<uint8_t>(0), input_image_preserve_ratio.data, input_image_preserve_ratio.total() * input_image_preserve_ratio.elemSize());

  // Run inference.
  if (interpreter->Invoke() != kTfLiteOk) {
    std::cerr << "Cannot invoke interpreter" << std::endl;
    return 1;
  }

  const auto* output_location_tensor = interpreter->typed_output_tensor<float>(0);
  const auto* output_classes_tensor = interpreter->typed_output_tensor<float>(1);
  const auto* output_scores_tensor = interpreter->typed_output_tensor<float>(2);
  const auto* output_numdetections_tensor = interpreter->typed_output_tensor<float>(3);

  int num_detections = round(output_numdetections_tensor[0]);

  detected_objects.clear();

  // https://www.tensorflow.org/lite/examples/object_detection/overview#output_signature
  for(int i = 0; i < num_detections; ++i)
  {
    DetectedObjects detected_object;
    detected_object.top    = round(input_height * output_location_tensor[i*4 + 0]);
    detected_object.left   = round(input_width  * output_location_tensor[i*4 + 1]);
    detected_object.bottom = round(input_height * output_location_tensor[i*4 + 2]);
    detected_object.right  = round(input_width  * output_location_tensor[i*4 + 3]);

    detected_object.confidence = output_scores_tensor[i];
    detected_object.class_id = round(output_classes_tensor[i]);

    detected_objects.push_back(detected_object);
  }

  return 0;
}
