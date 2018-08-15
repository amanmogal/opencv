// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html

// This code is also subject to the license terms in the LICENSE_KinectFusion.md file found in this module's directory

#include "precomp.hpp"
#include "fast_icp.hpp"
#include "tsdf.hpp"
#include "kinfu_frame.hpp"

namespace cv {
namespace kinfu {

Ptr<Params> Params::defaultParams()
{
    Params p;

    p.frameSize = Size(640, 480);

    float fx, fy, cx, cy;
    fx = fy = 525.f;
    cx = p.frameSize.width/2 - 0.5f;
    cy = p.frameSize.height/2 - 0.5f;
    p.intr = Matx33f(fx,  0, cx,
                      0, fy, cy,
                      0,  0,  1);

    // 5000 for the 16-bit PNG files
    // 1 for the 32-bit float images in the ROS bag files
    p.depthFactor = 5000;

    // sigma_depth is scaled by depthFactor when calling bilateral filter
    p.bilateral_sigma_depth = 0.04f;  //meter
    p.bilateral_sigma_spatial = 4.5; //pixels
    p.bilateral_kernel_size = 7;     //pixels

    p.icpAngleThresh = (float)(30. * CV_PI / 180.); // radians
    p.icpDistThresh = 0.1f; // meters
    // first non-zero numbers are accepted
    const int iters[] = {10, 5, 4, 0};

    for(size_t i = 0; i < sizeof(iters)/sizeof(int); i++)
    {
        if(iters[i])
        {
            p.icpIterations.push_back(iters[i]);
        }
        else
            break;
    }
    p.pyramidLevels = (int)p.icpIterations.size();

    p.tsdf_min_camera_movement = 0.f; //meters, disabled

    p.volumeDims = 512;  //number of voxels

    p.volumeSize = 3.f;  //meters

    // default pose of volume cube
    p.volumePose = Affine3f().translate(Vec3f(-p.volumeSize/2, -p.volumeSize/2, 0.5f));
    p.tsdf_trunc_dist = 0.04f; //meters;
    p.tsdf_max_weight = 64;   //frames

    p.raycast_step_factor = 0.25f;  //in voxel sizes
    // gradient delta factor is fixed at 1.0f and is not used
    //p.gradient_delta_factor = 0.5f; //in voxel sizes

    //p.lightPose = p.volume_pose.translation()/4; //meters
    p.lightPose = Vec3f::all(0.f); //meters

    // depth truncation is not used by default
    //p.icp_truncate_depth_dist = 0.f;        //meters, disabled

    return makePtr<Params>(p);
}

Ptr<Params> Params::coarseParams()
{
    Ptr<Params> p = defaultParams();

    // first non-zero numbers are accepted
    const int iters[] = {5, 3, 2};

    p->icpIterations.clear();
    for(size_t i = 0; i < sizeof(iters)/sizeof(int); i++)
    {
        if(iters[i])
        {
            p->icpIterations.push_back(iters[i]);
        }
        else
            break;
    }
    p->pyramidLevels = (int)p->icpIterations.size();

    p->volumeDims = 128; //number of voxels

    p->raycast_step_factor = 0.75f;  //in voxel sizes

    return p;
}

// T should be Mat or UMat
template< typename T >
class KinFuImpl : public KinFu
{
public:
    KinFuImpl(const Params& _params);
    virtual ~KinFuImpl();

    const Params& getParams() const CV_OVERRIDE;
    void setParams(const Params&) CV_OVERRIDE;

    void render(OutputArray image, const Matx44f& cameraPose) const CV_OVERRIDE;

    void getCloud(OutputArray points, OutputArray normals) const CV_OVERRIDE;
    void getPoints(OutputArray points) const CV_OVERRIDE;
    void getNormals(InputArray points, OutputArray normals) const CV_OVERRIDE;

    void reset() CV_OVERRIDE;

    const Affine3f getPose() const CV_OVERRIDE;

    bool update(InputArray depth) CV_OVERRIDE;

    bool updateT(T depth);

private:
    Params params;

    cv::Ptr<ICP> icp;
    cv::Ptr<TSDFVolume> volume;

    int frameCounter;
    Affine3f pose;
    std::vector<T> pyrPoints;
    std::vector<T> pyrNormals;
};


template< typename T >
KinFuImpl<T>::KinFuImpl(const Params &_params) :
    params(_params),
    icp(makeICP(params.intr, params.icpIterations, params.icpAngleThresh, params.icpDistThresh)),
    volume(makeTSDFVolume(params.volumeDims, params.volumeSize, params.volumePose,
                          params.tsdf_trunc_dist, params.tsdf_max_weight,
                          params.raycast_step_factor)),
    pyrPoints(), pyrNormals()
{
    reset();
}

template< typename T >
void KinFuImpl<T>::reset()
{
    frameCounter = 0;
    pose = Affine3f::Identity();
    volume->reset();
}

template< typename T >
KinFuImpl<T>::~KinFuImpl()
{ }

template< typename T >
const Params& KinFuImpl<T>::getParams() const
{
    return params;
}

template< typename T >
void KinFuImpl<T>::setParams(const Params& p)
{
    params = p;
}

template< typename T >
const Affine3f KinFuImpl<T>::getPose() const
{
    return pose;
}


template<>
bool KinFuImpl<Mat>::update(InputArray _depth)
{
    CV_Assert(!_depth.empty() && _depth.size() == params.frameSize);

    Mat depth;
    if(_depth.isUMat())
    {
        _depth.copyTo(depth);
        return updateT(depth);
    }
    else
    {
        return updateT(_depth.getMat());
    }
}


template<>
bool KinFuImpl<UMat>::update(InputArray _depth)
{
    CV_Assert(!_depth.empty() && _depth.size() == params.frameSize);

    UMat depth;
    if(!_depth.isUMat())
    {
        _depth.copyTo(depth);
        return updateT(depth);
    }
    else
    {
        return updateT(_depth.getUMat());
    }
}


template< typename T >
bool KinFuImpl<T>::updateT(T depth)
{
    CV_TRACE_FUNCTION();

    ScopeTime st("kinfu update");

    if(depth.type() != DEPTH_TYPE)
        depth.convertTo(depth, DEPTH_TYPE);

    std::vector<T> newPoints, newNormals;
    makeFrameFromDepth(depth, newPoints, newNormals, params.intr,
                       params.pyramidLevels,
                       params.depthFactor,
                       params.bilateral_sigma_depth,
                       params.bilateral_sigma_spatial,
                       params.bilateral_kernel_size);

    if(frameCounter == 0)
    {
        // use depth instead of distance
        volume->integrate(depth, params.depthFactor, pose, params.intr);

        pyrPoints  = newPoints;
        pyrNormals = newNormals;
    }
    else
    {
        Affine3f affine;
        bool success = icp->estimateTransform(affine, pyrPoints, pyrNormals, newPoints, newNormals);
        if(!success)
            return false;

        pose = pose * affine;

        float rnorm = (float)cv::norm(affine.rvec());
        float tnorm = (float)cv::norm(affine.translation());
        // We do not integrate volume if camera does not move
        if((rnorm + tnorm)/2 >= params.tsdf_min_camera_movement)
        {
            // use depth instead of distance
            volume->integrate(depth, params.depthFactor, pose, params.intr);
        }

        T& points  = pyrPoints [0];
        T& normals = pyrNormals[0];
        volume->raycast(pose, params.intr, params.frameSize, points, normals);
        // build a pyramid of points and normals
        buildPyramidPointsNormals(points, normals, pyrPoints, pyrNormals,
                                  params.pyramidLevels);
    }

    frameCounter++;
    return true;
}


template< typename T >
void KinFuImpl<T>::render(OutputArray image, const Matx44f& _cameraPose) const
{
    CV_TRACE_FUNCTION();

    ScopeTime st("kinfu render");

    Affine3f cameraPose(_cameraPose);

    const Affine3f id = Affine3f::Identity();
    if((cameraPose.rotation() == pose.rotation() && cameraPose.translation() == pose.translation()) ||
       (cameraPose.rotation() == id.rotation()   && cameraPose.translation() == id.translation()))
    {
        renderPointsNormals(pyrPoints[0], pyrNormals[0], image, params.lightPose);
    }
    else
    {
        T points, normals;
        volume->raycast(cameraPose, params.intr, params.frameSize, points, normals);
        renderPointsNormals(points, normals, image, params.lightPose);
    }
}


template< typename T >
void KinFuImpl<T>::getCloud(OutputArray p, OutputArray n) const
{
    volume->fetchPointsNormals(p, n);
}


template< typename T >
void KinFuImpl<T>::getPoints(OutputArray points) const
{
    volume->fetchPointsNormals(points, noArray());
}


template< typename T >
void KinFuImpl<T>::getNormals(InputArray points, OutputArray normals) const
{
    volume->fetchNormals(points, normals);
}

// importing class

Ptr<KinFu> KinFu::create(const Ptr<Params>& params)
{
#ifdef HAVE_OPENCL
    if(cv::ocl::isOpenCLActivated())
        return makePtr< KinFuImpl<UMat> >(*params);
#endif
    return makePtr< KinFuImpl<Mat> >(*params);
}

KinFu::~KinFu() {}

} // namespace kinfu
} // namespace cv
