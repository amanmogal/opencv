#ifndef OPENCV_TWIST_HPP
#define OPENCV_TWIST_HPP

#include "opencv2/core.hpp"

namespace cv
{
class CV_EXPORTS Twist
{
};

namespace detail
{
inline namespace tracking
{
//! @addtogroup tracking_detail
//! @{

class CV_EXPORTS Twist
{
public:
    Twist() = default;

    /**
     * @brief Compute the camera twist from a set of 2D pixel locations, their
     * velocities, depth values and intrinsic parameters of the camera. The pixel
     * velocities are usually obtained from optical flow algorithms, both dense and
     * sparse flow can be used to compute the flow between images and duv computed by
     * dividing the flow by the time interval between the images.
     *
     * @param uv 2xN matrix of 2D pixel locations
     * @param duv 2Nx1 matrix of 2D pixel velocities
     * @param depths 1xN matrix of depth values
     * @param K 3x3 camera intrinsic matrix
     *
     * @return cv::Vec6d 6x1 camera twist
     */
    cv::Vec6d compute(const cv::Mat& uv, const cv::Mat& duv, const cv::Mat depths,
                      const cv::Mat& K);

    /**
     * @brief Compute the interaction matrix for a set of 2D pixels. This is usually
     * used in visual servoing applications to command a robot to move at desired pixel
     * locations/velocities. By inverting this matrix one can estimate camera spatial
     * velocity i.e., the twist.
     *
     * @param uv 2xN matrix of 2D pixel locations
     * @param depth 1xN matrix of depth values
     * @param K 3x3 camera intrinsic matrix
     * @param J 2Nx6 interaction matrix
     *
     */
    void interactionMatrix(const cv::Mat& uv, const cv::Mat& depth, const cv::Mat& K, cv::Mat& J);
};

//! @}

} // namespace tracking
} // namespace detail
} // namespace cv

#endif
