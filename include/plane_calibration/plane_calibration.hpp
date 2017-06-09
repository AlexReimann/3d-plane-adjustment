#ifndef plane_calibration_SRC_PLANE_CALIBRATION_HPP_
#define plane_calibration_SRC_PLANE_CALIBRATION_HPP_

#include <mutex>
#include <vector>
#include <Eigen/Dense>

#include "camera_model.hpp"

namespace plane_calibration
{

class PlaneCalibration
{
public:
  class Parameters
  {
  public:
    Parameters()
    {
      max_deviation_ = 0.0;
    }

    Parameters(const Eigen::Vector3d& ground_plane_offset, const double& max_deviation)
    {
      ground_plane_offset_ = ground_plane_offset;
      max_deviation_ = max_deviation;
    }

    Eigen::Vector3d ground_plane_offset_;
    double max_deviation_;
  };

  class PlaneWithTransform
  {
  public:
    Eigen::MatrixXf plane;
    Eigen::Affine3d transform;
  };
  typedef std::vector<PlaneWithTransform> PlanesWithTransforms;

  PlaneCalibration();

  virtual ~PlaneCalibration()
  {
  }

  void updateParameters(const CameraModel& camera_model);
  void updateParameters(const Parameters& parameters);

  virtual bool updateMaxDeviationPlanesIfNeeded();
  virtual PlanesWithTransforms getDeviationPlanes();

  virtual std::vector<double> getDistancesToMaxDeviations(const Eigen::MatrixXf& plane);

protected:
  virtual void updateMaxDeviationPlanesImages();
  virtual std::vector<Eigen::Affine3d> getMaxDeviationTransforms();

  CameraModel camera_model_;
  std::mutex mutex_;
  Parameters parameters_;

  bool update_max_deviation_planes_;
  std::vector<PlaneWithTransform> max_deviation_planes_images_;
};

} /* end namespace */

#endif