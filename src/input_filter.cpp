#include "plane_calibration/input_filter.hpp"

#include <iostream>
#include "plane_calibration/plane_to_depth_image.hpp"

namespace plane_calibration
{

InputFilter::InputFilter(const CameraModel& camera_model, const CalibrationParametersPtr& parameters,
                         const std::shared_ptr<DepthVisualizer>& depth_visualizer, const Config& config) :
    camera_model_(camera_model)
{
  parameters_ = parameters;
  config_ = config;
  depth_visualizer_ = depth_visualizer;

  updateBorders_();
}

void InputFilter::updateConfig(const Config& config)
{
  std::lock_guard<std::mutex> lock(mutex_);
  config_ = config;

  updateBorders_();
}

void InputFilter::updateBorders_()
{
  double threshold = config_.threshold_from_ground + config_.max_error;

  Eigen::Affine3d ground_transform = parameters_->getTransform();
  Eigen::Translation3d top_offset(threshold * Eigen::Vector3d::UnitZ());
  Eigen::Translation3d bottom_offset(-threshold * Eigen::Vector3d::UnitZ());

  Eigen::Affine3d top_transform = ground_transform * top_offset;
  Eigen::Affine3d bottom_transform = ground_transform * bottom_offset;

  min_plane_ = PlaneToDepthImage::convert(top_transform, camera_model_.getParameters());
  max_plane_ = PlaneToDepthImage::convert(bottom_transform, camera_model_.getParameters());

}

void InputFilter::filter(Eigen::MatrixXf& matrix, bool debug)
{
  std::lock_guard<std::mutex> lock(mutex_);
  Eigen::Matrix<bool, Eigen::Dynamic, Eigen::Dynamic> far_enough = matrix.array() >= min_plane_.array();
  Eigen::Matrix<bool, Eigen::Dynamic, Eigen::Dynamic> close_enough = matrix.array() <= max_plane_.array();

  Eigen::MatrixXf valid = (far_enough.array() && close_enough.array()).cast<float>();
  matrix = matrix.cwiseProduct(valid);

  matrix = matrix.unaryExpr([](float v)
  { return v == 0.0f ? std::nanf("") : v;});

  if (debug)
  {
    std::string frame_id = "camera_depth_optical_frame";
    depth_visualizer_->publishCloud("debug/filter/top_border", max_plane_, frame_id);
    depth_visualizer_->publishCloud("debug/filter/bottom_border", min_plane_, frame_id);
    depth_visualizer_->publishImage("debug/filter/min_plane", max_plane_, frame_id);
    depth_visualizer_->publishImage("debug/filter/max_plane", max_plane_, frame_id);

    depth_visualizer_->publishImage("debug/filter/far_enough", far_enough.cast<float>(), frame_id);
    depth_visualizer_->publishImage("debug/filter/close_enough", close_enough.cast<float>(), frame_id);

    depth_visualizer_->publishImage("debug/filter/diff_min", min_plane_ - matrix, frame_id);
    depth_visualizer_->publishImage("debug/filter/diff_max", max_plane_ - matrix, frame_id);
    depth_visualizer_->publishImage("debug/filter/not_filtered", valid, frame_id);
    depth_visualizer_->publishCloud("debug/filter/filtered", matrix, frame_id);
  }
}

bool InputFilter::dataIsUsable(const Eigen::MatrixXf& data)
{
  std::lock_guard<std::mutex> lock(mutex_);
  double data_size = data.size();

  int nan_count = (data.array() != data.array()).count();
  double nan_ratio = nan_count / data_size;

  if (nan_ratio > config_.max_nan_ratio)
  {
    return false;
  }

  data_size = data_size - nan_count;
  int zero_count = (data.array() == 0.0).count();
  double zero_ratio = zero_count / data_size;

  if (zero_ratio > config_.max_zero_ratio)
  {
    return false;
  }

  double data_ratio = data_size / data.size();
  if (data_ratio < config_.min_data_ratio)
  {
    return false;
  }

  return true;
}

} /* end namespace */