//TODO: add license

#ifndef __OPENCV_KINFU_TSDF_H__
#define __OPENCV_KINFU_TSDF_H__

#include "precomp.hpp"
#include "frame.hpp"

class TSDFVolume
{
public:
    // dimension in voxels, size in meters
    TSDFVolume(int _res, float _size, cv::Affine3f _pose, float _truncDist, int _maxWeight,
               float _raycastStepFactor);

    virtual void integrate(cv::Ptr<Frame> depth, float depthFactor, cv::Affine3f cameraPose, cv::kinfu::Intr intrinsics) = 0;
    virtual cv::Ptr<Frame> raycast(cv::Affine3f cameraPose, cv::kinfu::Intr intrinsics, cv::Size frameSize, int pyramidLevels,
                                   cv::Ptr<FrameGenerator> frameGenerator) const = 0;

    virtual void fetchPointsNormals(cv::OutputArray points, cv::OutputArray normals) const = 0;
    virtual void fetchNormals(cv::InputArray points, cv::OutputArray _normals) const = 0;

    virtual void reset() = 0;

    virtual ~TSDFVolume() { }

    float edgeSize;
    int edgeResolution;
    int maxWeight;
    cv::Affine3f pose;
};

cv::Ptr<TSDFVolume> makeTSDFVolume(cv::kinfu::KinFu::KinFuParams::PlatformType t,
                                   int _res, float _size, cv::Affine3f _pose, float _truncDist, int _maxWeight,
                                   float _raycastStepFactor);

#endif
