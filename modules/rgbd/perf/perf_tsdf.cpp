// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html

#include "perf_precomp.hpp"

namespace opencv_test { namespace {

using namespace cv;

/** Reprojects screen point to camera space given z coord. */
struct Reprojector
{
    Reprojector() {}
    inline Reprojector(Matx33f intr)
    {
        fxinv = 1.f / intr(0, 0), fyinv = 1.f / intr(1, 1);
        cx = intr(0, 2), cy = intr(1, 2);
    }
    template<typename T>
    inline cv::Point3_<T> operator()(cv::Point3_<T> p) const
    {
        T x = p.z * (p.x - cx) * fxinv;
        T y = p.z * (p.y - cy) * fyinv;
        return cv::Point3_<T>(x, y, p.z);
    }

    float fxinv, fyinv, cx, cy;
};

template<class Scene>
struct RenderInvoker : ParallelLoopBody
{
    RenderInvoker(Mat_<float>& _frame, Affine3f _pose,
        Reprojector _reproj,
        float _depthFactor) : ParallelLoopBody(),
        frame(_frame),
        pose(_pose),
        reproj(_reproj),
        depthFactor(_depthFactor)
    { }

    virtual void operator ()(const cv::Range& r) const
    {
        for (int y = r.start; y < r.end; y++)
        {
            float* frameRow = frame[y];
            for (int x = 0; x < frame.cols; x++)
            {
                float pix = 0;

                Point3f orig = pose.translation();
                // direction through pixel
                Point3f screenVec = reproj(Point3f((float)x, (float)y, 1.f));
                float xyt = 1.f / (screenVec.x * screenVec.x +
                    screenVec.y * screenVec.y + 1.f);
                Point3f dir = normalize(Vec3f(pose.rotation() * screenVec));
                // screen space axis
                dir.y = -dir.y;

                const float maxDepth = 20.f;
                const float maxSteps = 256;
                float t = 0.f;
                for (int step = 0; step < maxSteps && t < maxDepth; step++)
                {
                    Point3f p = orig + dir * t;
                    float d = Scene::map(p);
                    if (d < 0.000001f)
                    {
                        float depth = std::sqrt(t * t * xyt);
                        pix = depth * depthFactor;
                        break;
                    }
                    t += d;
                }

                frameRow[x] = pix;
            }
        }
    }

    Mat_<float>& frame;
    Affine3f pose;
    Reprojector reproj;
    float depthFactor;
};

struct Scene
{
    virtual ~Scene() {}
    static Ptr<Scene> create(Size sz, Matx33f _intr, float _depthFactor);
    virtual Mat depth(Affine3f pose) = 0;
    virtual std::vector<Affine3f> getPoses() = 0;
};

struct RotatingScene : Scene
{
    const int framesPerCycle = 32;
    const float nCycles = 0.5f;
    const Affine3f startPose = Affine3f(Vec3f(-1.f, 0.f, 0.f), Vec3f(1.5f, 2.f, -1.5f));

    RotatingScene(Size sz, Matx33f _intr, float _depthFactor) :
        frameSize(sz), intr(_intr), depthFactor(_depthFactor)
    {
        cv::RNG rng(0);
        rng.fill(randTexture, cv::RNG::UNIFORM, 0.f, 1.f);
    }

    static float map(Point3f p)
    {
        const Point3f torPlace(0.f, 0.f, 0.f);
        Point3f torPos(p - torPlace);
        const Point2f torusParams(1.f, 0.2f);
        Point2f torq(std::sqrt(torPos.x * torPos.x + torPos.z * torPos.z) - torusParams.x, torPos.y);
        float torus = (float)cv::norm(torq) - torusParams.y;

        const Point3f cylShift(0.25f, 0.25f, 0.25f);

        Point3f cylPos = Point3f(abs(std::fmod(p.x - 0.1f, cylShift.x)),
            p.y,
            abs(std::fmod(p.z - 0.2f, cylShift.z))) - cylShift * 0.5f;

        const Point2f cylParams(0.1f,
            0.1f + 0.1f * sin(p.x * p.y * 5.f /* +std::log(1.f+abs(p.x*0.1f)) */));
        Point2f cyld = Point2f(abs(std::sqrt(cylPos.x * cylPos.x + cylPos.z * cylPos.z)), abs(cylPos.y)) - cylParams;
        float pins = min(max(cyld.x, cyld.y), 0.0f) + (float)cv::norm(Point2f(max(cyld.x, 0.f), max(cyld.y, 0.f)));

        float res = max(-pins, torus);

        return res;
    }

    Mat depth(Affine3f pose) override
    {
        Mat_<float> frame(frameSize);
        Reprojector reproj(intr);

        Range range(0, frame.rows);
        parallel_for_(range, RenderInvoker<RotatingScene>(frame, pose, reproj, depthFactor));

        return std::move(frame);
    }

    std::vector<Affine3f> getPoses() override
    {
        std::vector<Affine3f> poses;
        for (int i = 0; i < framesPerCycle * nCycles; i++)
        {
            float angle = (float)(CV_2PI * i / framesPerCycle);
            Affine3f pose;
            pose = pose.rotate(startPose.rotation());
            pose = pose.rotate(Vec3f(0.f, -1.f, 0.f) * angle);
            pose = pose.translate(Vec3f(startPose.translation()[0] * sin(angle),
                startPose.translation()[1],
                startPose.translation()[2] * cos(angle)));
            poses.push_back(pose);
        }

        return poses;
    }

    Size frameSize;
    Matx33f intr;
    float depthFactor;
    static cv::Mat_<float> randTexture;
};

Mat_<float> RotatingScene::randTexture(256, 256);

Ptr<Scene> Scene::create(Size sz, Matx33f _intr, float _depthFactor)
{
    return makePtr<RotatingScene>(sz, _intr, _depthFactor);
}

// this is a temporary solution
// ----------------------------

typedef cv::Vec4f ptype;
typedef cv::Mat_< ptype > Points;
typedef Points Normals;
typedef Size2i Size;

template<int p>
inline float specPow(float x)
{
    if (p % 2 == 0)
    {
        float v = specPow<p / 2>(x);
        return v * v;
    }
    else
    {
        float v = specPow<(p - 1) / 2>(x);
        return v * v * x;
    }
}

template<>
inline float specPow<0>(float /*x*/)
{
    return 1.f;
}

template<>
inline float specPow<1>(float x)
{
    return x;
}

inline cv::Vec3f fromPtype(const ptype& x)
{
    return cv::Vec3f(x[0], x[1], x[2]);
}

inline Point3f normalize(const Vec3f& v)
{
    double nv = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    return v * (nv ? 1. / nv : 0.);
}

void renderPointsNormals(InputArray _points, InputArray _normals, OutputArray image, Affine3f lightPose)
{
    Size sz = _points.size();
    image.create(sz, CV_8UC4);

    Points  points = _points.getMat();
    Normals normals = _normals.getMat();

    Mat_<Vec4b> img = image.getMat();

    Range range(0, sz.height);
    const int nstripes = -1;
    parallel_for_(range, [&](const Range&)
        {
            for (int y = range.start; y < range.end; y++)
            {
                Vec4b* imgRow = img[y];
                const ptype* ptsRow = points[y];
                const ptype* nrmRow = normals[y];

                for (int x = 0; x < sz.width; x++)
                {
                    Point3f p = fromPtype(ptsRow[x]);
                    Point3f n = fromPtype(nrmRow[x]);

                    Vec4b color;

                    if (cvIsNaN(p.x) || cvIsNaN(p.y) || cvIsNaN(p.z) )
                    {
                        color = Vec4b(0, 32, 0, 0);
                    }
                    else
                    {
                        const float Ka = 0.3f;  //ambient coeff
                        const float Kd = 0.5f;  //diffuse coeff
                        const float Ks = 0.2f;  //specular coeff
                        const int   sp = 20;  //specular power

                        const float Ax = 1.f;   //ambient color,  can be RGB
                        const float Dx = 1.f;   //diffuse color,  can be RGB
                        const float Sx = 1.f;   //specular color, can be RGB
                        const float Lx = 1.f;   //light color

                        Point3f l = normalize(lightPose.translation() - Vec3f(p));
                        Point3f v = normalize(-Vec3f(p));
                        Point3f r = normalize(Vec3f(2.f * n * n.dot(l) - l));

                        uchar ix = (uchar)((Ax * Ka * Dx + Lx * Kd * Dx * max(0.f, n.dot(l)) +
                            Lx * Ks * Sx * specPow<sp>(max(0.f, r.dot(v)))) * 255.f);
                        color = Vec4b(ix, ix, ix, 0);
                    }

                    imgRow[x] = color;
                }
            }
        }, nstripes);
}

// ----------------------------

class Settings
{
public:
    Ptr<kinfu::Params> _params;
    Ptr<kinfu::Volume> volume;
    Ptr<Scene> scene;
    std::vector<Affine3f> poses;

    Settings(bool useHashTSDF)
    {
        if (useHashTSDF)
            _params = kinfu::Params::hashTSDFParams(true);
        else
            _params = kinfu::Params::coarseParams();

        volume = kinfu::makeVolume(_params->volumeType, _params->voxelSize, _params->volumePose.matrix,
            _params->raycast_step_factor, _params->tsdf_trunc_dist, _params->tsdf_max_weight,
            _params->truncateThreshold, _params->volumeDims);

        scene = Scene::create(_params->frameSize, _params->intr, _params->depthFactor);
        poses = scene->getPoses();
    }
};

void displayImage(Mat depth, UMat _points, UMat _normals, float depthFactor, Vec3f lightPose)
{
    Mat  points, normals, image;
    AccessFlag af = ACCESS_READ;
    normals = _normals.getMat(af);
    points = _points.getMat(af);
    patchNaNs(points);

    imshow("depth", depth * (1.f / depthFactor / 4.f));
    renderPointsNormals(points, normals, image, lightPose);
    imshow("render", image);
    waitKey(2000);
}

static const bool display = true;

PERF_TEST(Perf_TSDF, integrate)
{
    Settings settings(false);
    for (size_t i = 0; i < settings.poses.size(); i++)
    {
        Matx44f pose = settings.poses[i].matrix;
        Mat depth = settings.scene->depth(pose);
        startTimer();
        settings.volume->integrate(depth, settings._params->depthFactor, pose, settings._params->intr);
        stopTimer();
    }
    SANITY_CHECK_NOTHING();
}

PERF_TEST(Perf_TSDF, raycast)
{
    Settings settings(false);
    for (size_t i = 0; i < settings.poses.size(); i++)
    {
        UMat _points, _normals;
        Matx44f pose = settings.poses[i].matrix;
        Mat depth = settings.scene->depth(pose);

        settings.volume->integrate(depth, settings._params->depthFactor, pose, settings._params->intr);
        startTimer();
        settings.volume->raycast(pose, settings._params->intr, settings._params->frameSize, _points, _normals);
        stopTimer();

        if (display)
            displayImage(depth, _points, _normals, settings._params->depthFactor, settings._params->lightPose);
    }
    SANITY_CHECK_NOTHING();
}

PERF_TEST(Perf_HashTSDF, integrate)
{
    Settings settings(true);

    for (size_t i = 0; i < settings.poses.size(); i++)
    {
        Matx44f pose = settings.poses[i].matrix;
        Mat depth = settings.scene->depth(pose);
        startTimer();
        settings.volume->integrate(depth, settings._params->depthFactor, pose, settings._params->intr);
        stopTimer();
    }
    SANITY_CHECK_NOTHING();
}

PERF_TEST(Perf_HashTSDF, raycast)
{
    Settings settings(true);
    for (size_t i = 0; i < settings.poses.size(); i++)
    {
        UMat _points, _normals;
        Matx44f pose = settings.poses[i].matrix;
        Mat depth = settings.scene->depth(pose);

        settings.volume->integrate(depth, settings._params->depthFactor, pose, settings._params->intr);
        startTimer();
        settings.volume->raycast(pose, settings._params->intr, settings._params->frameSize, _points, _normals);
        stopTimer();
        
        if (display)
            displayImage(depth, _points, _normals, settings._params->depthFactor, settings._params->lightPose);
    }
    SANITY_CHECK_NOTHING();
}

}} // namespace
