// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html
#include "hash_tsdf.hpp"

#include <atomic>
#include <functional>
#include <iostream>
#include <limits>
#include <vector>

#include "kinfu_frame.hpp"
#include "opencv2/core/cvstd.hpp"
#include "opencv2/core/utility.hpp"
#include "opencv2/core/utils/trace.hpp"
#include "precomp.hpp"
#include "utils.hpp"

namespace cv
{
namespace kinfu
{
template<typename Derived>
HashTSDFVolume<Derived>::HashTSDFVolume(float _voxelSize, Matx44f _pose, float _raycastStepFactor, float _truncDist,
                                        int _maxWeight, float _truncateThreshold, int _volumeUnitRes, bool _zFirstMemOrder)
    : Volume(_voxelSize, _pose, _raycastStepFactor),
      maxWeight(_maxWeight),
      truncateThreshold(_truncateThreshold),
      volumeUnitResolution(_volumeUnitRes),
      volumeUnitSize(voxelSize * volumeUnitResolution),
      zFirstMemOrder(_zFirstMemOrder)
{
    truncDist = std::max(_truncDist, 4.0f * voxelSize);
}

HashTSDFVolumeCPU::HashTSDFVolumeCPU(float _voxelSize, Matx44f _pose, float _raycastStepFactor, float _truncDist,
                                     int _maxWeight, float _truncateThreshold, int _volumeUnitRes, bool _zFirstMemOrder)
    : Base(_voxelSize, _pose, _raycastStepFactor, _truncDist, _maxWeight, _truncateThreshold, _volumeUnitRes,
           _zFirstMemOrder)
{
}

HashTSDFVolumeCPU::HashTSDFVolumeCPU(const VolumeParams& _params, bool _zFirstMemOrder)
    : Base(_params.voxelSize, _params.pose.matrix, _params.raycastStepFactor, _params.tsdfTruncDist, _params.maxWeight,
           _params.depthTruncThreshold, _params.unitResolution, _zFirstMemOrder)
{
}
// zero volume, leave rest params the same
void HashTSDFVolumeCPU::reset_()
{
    CV_TRACE_FUNCTION();
    volumeUnits.clear();
}

struct AllocateVolumeUnitsInvoker : ParallelLoopBody
{
    AllocateVolumeUnitsInvoker(HashTSDFVolumeCPU& _volume, const int _frameId, const Depth& _depth, Intr intrinsics,
                               Matx44f cameraPose, float _depthFactor, int _depthStride = 4)
        : ParallelLoopBody(),
          volume(_volume),
          frameId(_frameId),
          depth(_depth),
          reproj(intrinsics.makeReprojector()),
          cam2vol(_volume.pose.inv() * Affine3f(cameraPose)),
          depthFactor(1.0f / _depthFactor),
          depthStride(_depthStride)
    {
    }

    virtual void operator()(const Range& range) const override
    {
        for (int y = range.start; y < range.end; y += depthStride)
        {
            const depthType* depthRow = depth[y];
            for (int x = 0; x < depth.cols; x += depthStride)
            {
                depthType z = depthRow[x] * depthFactor;
                if (z <= 0 || z > volume.truncateThreshold)
                    continue;

                Point3f camPoint = reproj(Point3f((float)x, (float)y, z));
                Point3f volPoint = cam2vol * camPoint;

                //! Find accessed TSDF volume unit for valid 3D vertex
                Vec3i lower_bound =
                    volume.volumeToVolumeUnitIdx(volPoint - Point3f(volume.truncDist, volume.truncDist, volume.truncDist));
                Vec3i upper_bound =
                    volume.volumeToVolumeUnitIdx(volPoint + Point3f(volume.truncDist, volume.truncDist, volume.truncDist));
                VolumeUnitIndexSet localAccessVolUnits;
                for (int i = lower_bound[0]; i <= upper_bound[0]; i++)
                    for (int j = lower_bound[1]; j <= upper_bound[1]; j++)
                        for (int k = lower_bound[2]; k <= lower_bound[2]; k++)
                        {
                            const Vec3i tsdf_idx = Vec3i(i, j, k);
                            if (!localAccessVolUnits.count(tsdf_idx))
                            {
                                localAccessVolUnits.emplace(tsdf_idx);
                            }
                        }
                AutoLock al(mutex);
                for (const auto& tsdf_idx : localAccessVolUnits)
                {
                    //! If the insert into the global set passes
                    if (!volume.volumeUnits.count(tsdf_idx))
                    {
                        VolumeUnit volumeUnit;
                        Point3i volumeDims(volume.volumeUnitResolution, volume.volumeUnitResolution,
                                           volume.volumeUnitResolution);

                        Matx44f subvolumePose = volume.pose.translate(volume.volumeUnitIdxToVolume(tsdf_idx)).matrix;
                        volumeUnit.pVolume =
                            makePtr<TSDFVolumeCPU>(volume.voxelSize, subvolumePose, volume.raycastStepFactor,
                                                   volume.truncDist, volume.maxWeight, volumeDims);
                        //! This volume unit will definitely be required for current integration
                        volumeUnit.isActive          = true;
                        volumeUnit.lastVisibleIndex  = frameId;
                        volume.volumeUnits[tsdf_idx] = volumeUnit;
                    }
                }
            }
        }
    }

    HashTSDFVolumeCPU& volume;
    const int frameId;
    const Depth& depth;
    const Intr::Reprojector reproj;
    const Affine3f cam2vol;
    const float depthFactor;
    const int depthStride;
    mutable Mutex mutex;
};

void HashTSDFVolumeCPU::integrate_(InputArray _depth, float depthFactor, const Matx44f& cameraPose, const Intr& intrinsics,
                                   const int frameId)
{
    CV_TRACE_FUNCTION();

    CV_Assert(_depth.type() == DEPTH_TYPE);
    Depth depth = _depth.getMat();

    //! Compute volumes to be allocated
    AllocateVolumeUnitsInvoker allocate_i(*this, frameId, depth, intrinsics, cameraPose, depthFactor);
    Range allocateRange(0, depth.rows);
    parallel_for_(allocateRange, allocate_i);

    //! Get keys for all the allocated volume Units
    std::vector<Vec3i> totalVolUnits;
    for (const auto& keyvalue : volumeUnits)
    {
        totalVolUnits.push_back(keyvalue.first);
    }

    //! Mark volumes in the camera frustum as active
    Range inFrustumRange(0, (int)volumeUnits.size());
    parallel_for_(inFrustumRange, [&](const Range& range) {
        const Affine3f vol2cam(Affine3f(cameraPose.inv()) * pose);
        const Intr::Projector proj(intrinsics.makeProjector());

        for (int i = range.start; i < range.end; ++i)
        {
            Vec3i tsdf_idx             = totalVolUnits[i];
            VolumeUnitMap::iterator it = volumeUnits.find(tsdf_idx);
            if (it == volumeUnits.end())
                return;

            Point3f volumeUnitPos     = volumeUnitIdxToVolume(it->first);
            Point3f volUnitInCamSpace = vol2cam * volumeUnitPos;
            if (volUnitInCamSpace.z < 0 || volUnitInCamSpace.z > truncateThreshold)
            {
                it->second.isActive = false;
                return;
            }
            Point2f cameraPoint = proj(volUnitInCamSpace);
            if (cameraPoint.x >= 0 && cameraPoint.y >= 0 && cameraPoint.x < depth.cols && cameraPoint.y < depth.rows)
            {
                assert(it != volumeUnits.end());
                it->second.lastVisibleIndex = frameId;
                it->second.isActive         = true;
            }
        }
    });

    //! Integrate the correct volumeUnits
    parallel_for_(Range(0, (int)totalVolUnits.size()), [&](const Range& range) {
        for (int i = range.start; i < range.end; i++)
        {
            Vec3i tsdf_idx             = totalVolUnits[i];
            VolumeUnitMap::iterator it = volumeUnits.find(tsdf_idx);
            if (it == volumeUnits.end())
                return;

            VolumeUnit& volumeUnit = it->second;
            if (volumeUnit.isActive)
            {
                //! The volume unit should already be added into the Volume from the allocator
                volumeUnit.pVolume->integrate(depth, depthFactor, cameraPose, intrinsics);
                //! Ensure all active volumeUnits are set to inactive for next integration
                volumeUnit.isActive = false;
            }
        }
    });
}

Vec3i HashTSDFVolumeCPU::volumeToVolumeUnitIdx_(const Point3f& p) const
{
    return Vec3i(cvFloor(p.x / volumeUnitSize), cvFloor(p.y / volumeUnitSize), cvFloor(p.z / volumeUnitSize));
}

Point3f HashTSDFVolumeCPU::volumeUnitIdxToVolume_(const Vec3i& volumeUnitIdx) const
{
    return Point3f(volumeUnitIdx[0] * volumeUnitSize, volumeUnitIdx[1] * volumeUnitSize, volumeUnitIdx[2] * volumeUnitSize);
}

Point3f HashTSDFVolumeCPU::voxelCoordToVolume_(const Vec3i& voxelIdx) const
{
    return Point3f(voxelIdx[0] * voxelSize, voxelIdx[1] * voxelSize, voxelIdx[2] * voxelSize);
}

Vec3i HashTSDFVolumeCPU::volumeToVoxelCoord_(const Point3f& point) const
{
    return Vec3i(cvFloor(point.x * voxelSizeInv), cvFloor(point.y * voxelSizeInv), cvFloor(point.z * voxelSizeInv));
}

struct HashRaycastInvoker : ParallelLoopBody
{
    HashRaycastInvoker(Points& _points, Normals& _normals, const Matx44f& cameraPose, const Intr& intrinsics,
                       const HashTSDFVolumeCPU& _volume)
        : ParallelLoopBody(),
          points(_points),
          normals(_normals),
          volume(_volume),
          tstep(_volume.truncDist * _volume.raycastStepFactor),
          cam2vol(volume.pose.inv() * Affine3f(cameraPose)),
          vol2cam(Affine3f(cameraPose.inv()) * volume.pose),
          reproj(intrinsics.makeReprojector())
    {
    }

    virtual void operator()(const Range& range) const override
    {
        const Point3f cam2volTrans = cam2vol.translation();
        const Matx33f cam2volRot   = cam2vol.rotation();
        const Matx33f vol2camRot   = vol2cam.rotation();

        const float blockSize = volume.volumeUnitSize;

        for (int y = range.start; y < range.end; y++)
        {
            ptype* ptsRow = points[y];
            ptype* nrmRow = normals[y];

            for (int x = 0; x < points.cols; x++)
            {
                //! Initialize default value
                Point3f point = nan3, normal = nan3;

                //! Ray origin and direction in the volume coordinate frame
                Point3f orig    = cam2volTrans;
                Point3f rayDirV = normalize(Vec3f(cam2volRot * reproj(Point3f(float(x), float(y), 1.f))));

                float tmin  = 0;
                float tmax  = volume.truncateThreshold;
                float tcurr = tmin;

                Vec3i prevVolumeUnitIdx =
                    Vec3i(std::numeric_limits<int>::min(), std::numeric_limits<int>::min(), std::numeric_limits<int>::min());

                float tprev       = tcurr;
                TsdfType prevTsdf = volume.truncDist;
                Ptr<TSDFVolumeCPU> currVolumeUnit;
                while (tcurr < tmax)
                {
                    Point3f currRayPos      = orig + tcurr * rayDirV;
                    Vec3i currVolumeUnitIdx = volume.volumeToVolumeUnitIdx(currRayPos);

                    VolumeUnitMap::const_iterator it = volume.volumeUnits.find(currVolumeUnitIdx);

                    TsdfType currTsdf = prevTsdf;
                    int currWeight    = 0;
                    float stepSize    = 0.5f * blockSize;
                    Vec3i volUnitLocalIdx;

                    //! The subvolume exists in hashtable
                    if (it != volume.volumeUnits.end())
                    {
                        currVolumeUnit         = std::dynamic_pointer_cast<TSDFVolumeCPU>(it->second.pVolume);
                        Point3f currVolUnitPos = volume.volumeUnitIdxToVolume(currVolumeUnitIdx);
                        volUnitLocalIdx        = volume.volumeToVoxelCoord(currRayPos - currVolUnitPos);

                        //! TODO: Figure out voxel interpolation
                        TsdfVoxel currVoxel = currVolumeUnit->at(volUnitLocalIdx);
                        currTsdf            = currVoxel.tsdf;
                        currWeight          = currVoxel.weight;
                        stepSize            = tstep;
                    }
                    //! Surface crossing
                    if (prevTsdf > 0.f && currTsdf <= 0.f && currWeight > 0)
                    {
                        float tInterp = (tcurr * prevTsdf - tprev * currTsdf) / (prevTsdf - currTsdf);
                        if (!cvIsNaN(tInterp) && !cvIsInf(tInterp))
                        {
                            Point3f pv = orig + tInterp * rayDirV;
                            Point3f nv = volume.getNormalVoxel(pv);

                            if (!isNaN(nv))
                            {
                                normal = vol2camRot * nv;
                                point  = vol2cam * pv;
                            }
                        }
                        break;
                    }
                    prevVolumeUnitIdx = currVolumeUnitIdx;
                    prevTsdf          = currTsdf;
                    tprev             = tcurr;
                    tcurr += stepSize;
                }
                ptsRow[x] = toPtype(point);
                nrmRow[x] = toPtype(normal);
            }
        }
    }

    Points& points;
    Normals& normals;
    const HashTSDFVolumeCPU& volume;
    const float tstep;
    const Affine3f cam2vol;
    const Affine3f vol2cam;
    const Intr::Reprojector reproj;
};

void HashTSDFVolumeCPU::raycast_(const Matx44f& cameraPose, const kinfu::Intr& intrinsics, const Size& frameSize,
                                 OutputArray _points, OutputArray _normals) const
{
    CV_TRACE_FUNCTION();
    CV_Assert(frameSize.area() > 0);

    _points.create(frameSize, POINT_TYPE);
    _normals.create(frameSize, POINT_TYPE);

    Points points   = _points.getMat();
    Normals normals = _normals.getMat();

    HashRaycastInvoker ri(points, normals, cameraPose, intrinsics, *this);

    const int nstripes = -1;
    parallel_for_(Range(0, points.rows), ri, nstripes);
}

struct HashFetchPointsNormalsInvoker : ParallelLoopBody
{
    HashFetchPointsNormalsInvoker(const HashTSDFVolumeCPU& _volume, const std::vector<Vec3i>& _totalVolUnits,
                                  std::vector<std::vector<ptype>>& _pVecs, std::vector<std::vector<ptype>>& _nVecs,
                                  bool _needNormals)
        : ParallelLoopBody(),
          volume(_volume),
          totalVolUnits(_totalVolUnits),
          pVecs(_pVecs),
          nVecs(_nVecs),
          needNormals(_needNormals)
    {
    }

    virtual void operator()(const Range& range) const override
    {
        std::vector<ptype> points, normals;
        for (int i = range.start; i < range.end; i++)
        {
            Vec3i tsdf_idx = totalVolUnits[i];

            VolumeUnitMap::const_iterator it = volume.volumeUnits.find(tsdf_idx);
            Point3f base_point               = volume.volumeUnitIdxToVolume(tsdf_idx);
            if (it != volume.volumeUnits.end())
            {
                Ptr<TSDFVolumeCPU> volumeUnit = std::dynamic_pointer_cast<TSDFVolumeCPU>(it->second.pVolume);
                std::vector<ptype> localPoints;
                std::vector<ptype> localNormals;
                for (int x = 0; x < volume.volumeUnitResolution; x++)
                    for (int y = 0; y < volume.volumeUnitResolution; y++)
                        for (int z = 0; z < volume.volumeUnitResolution; z++)
                        {
                            Vec3i voxelIdx(x, y, z);
                            TsdfVoxel voxel = volumeUnit->at(voxelIdx);

                            if (voxel.tsdf != 1.f && voxel.weight != 0)
                            {
                                Point3f point = base_point + volume.voxelCoordToVolume(voxelIdx);
                                localPoints.push_back(toPtype(point));
                                if (needNormals)
                                {
                                    Point3f normal = volume.getNormalVoxel(point);
                                    localNormals.push_back(toPtype(normal));
                                }
                            }
                        }

                AutoLock al(mutex);
                pVecs.push_back(localPoints);
                nVecs.push_back(localNormals);
            }
        }
    }

    const HashTSDFVolumeCPU& volume;
    std::vector<Vec3i> totalVolUnits;
    std::vector<std::vector<ptype>>& pVecs;
    std::vector<std::vector<ptype>>& nVecs;
    const TsdfVoxel* volDataStart;
    bool needNormals;
    mutable Mutex mutex;
};

void HashTSDFVolumeCPU::fetchPointsNormals_(OutputArray _points, OutputArray _normals) const
{
    CV_TRACE_FUNCTION();

    if (_points.needed())
    {
        std::vector<std::vector<ptype>> pVecs, nVecs;

        std::vector<Vec3i> totalVolUnits;
        for (const auto& keyvalue : volumeUnits)
        {
            totalVolUnits.push_back(keyvalue.first);
        }
        HashFetchPointsNormalsInvoker fi(*this, totalVolUnits, pVecs, nVecs, _normals.needed());
        Range range(0, (int)totalVolUnits.size());
        const int nstripes = -1;
        parallel_for_(range, fi, nstripes);
        std::vector<ptype> points, normals;
        for (size_t i = 0; i < pVecs.size(); i++)
        {
            points.insert(points.end(), pVecs[i].begin(), pVecs[i].end());
            normals.insert(normals.end(), nVecs[i].begin(), nVecs[i].end());
        }

        _points.create((int)points.size(), 1, POINT_TYPE);
        if (!points.empty())
            Mat((int)points.size(), 1, POINT_TYPE, &points[0]).copyTo(_points.getMat());

        if (_normals.needed())
        {
            _normals.create((int)normals.size(), 1, POINT_TYPE);
            if (!normals.empty())
                Mat((int)normals.size(), 1, POINT_TYPE, &normals[0]).copyTo(_normals.getMat());
        }
    }
}

void HashTSDFVolumeCPU::fetchNormals_(InputArray _points, OutputArray _normals) const
{
    CV_TRACE_FUNCTION();

    if (_normals.needed())
    {
        Points points = _points.getMat();
        CV_Assert(points.type() == POINT_TYPE);

        _normals.createSameSize(_points, _points.type());
        Normals normals = _normals.getMat();

        const HashTSDFVolumeCPU& _volume = *this;
        auto HashPushNormals             = [&](const ptype& point, const int* position) {
            const HashTSDFVolumeCPU& volume(_volume);
            Affine3f invPose(volume.pose.inv());
            Point3f p = fromPtype(point);
            Point3f n = nan3;
            if (!isNaN(p))
            {
                Point3f voxelPoint = invPose * p;
                n                  = volume.pose.rotation() * volume.getNormalVoxel(voxelPoint);
            }
            normals(position[0], position[1]) = toPtype(n);
        };
        points.forEach(HashPushNormals);
    }
}

int HashTSDFVolumeCPU::getVisibleBlocks_(int currFrameId, int frameThreshold) const
{
    int numVisibleBlocks = 0;
    //! TODO: Iterate over map parallely?
    for (const auto& keyvalue : volumeUnits)
    {
        const VolumeUnit& volumeUnit = keyvalue.second;
        if (volumeUnit.lastVisibleIndex > (currFrameId - frameThreshold))
            numVisibleBlocks++;
    }
    return numVisibleBlocks;
}

}  // namespace kinfu
}  // namespace cv
