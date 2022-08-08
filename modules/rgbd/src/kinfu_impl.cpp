// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html

// This code is also subject to the license terms in the LICENSE_KinectFusion.md file found in this module's directory

//#include "precomp.hpp"
#include "kinfu_impl.hpp"
#include "kinfu_functions.hpp"

namespace cv {

KinFu::Impl::Impl(VolumeType vt, bool isHighDense) :
	volumeSettings(vt),
	odometrySettings()
{
	odometrySettings.setMaxRotation(30.f);
	float voxelSize = volumeSettings.getVoxelSize();
	Vec3i res;
	volumeSettings.getVolumeResolution(res);
	odometrySettings.setMaxTranslation(voxelSize * res[0] * 0.5f);

	if (isHighDense && vt == VolumeType::TSDF)
	{
		float volSize = 3.f;
		volumeSettings.setVolumeResolution(Vec3i::all(512));
		volumeSettings.setVoxelSize(volSize / 512.f);
		volumeSettings.setTsdfTruncateDistance(3.f * volSize / 512.f);
	}

	odometry = Odometry(OdometryType::DEPTH, this->odometrySettings, OdometryAlgoType::FAST);
	volume = Volume(vt, this->volumeSettings);
}


KinFu_Common::KinFu_Common(VolumeType vt, bool isHighDense)
	: Impl(vt, isHighDense)
{
	reset();
}

KinFu_Common::~KinFu_Common()
{
}

VolumeSettings KinFu_Common::getVolumeSettings() const
{
	return this->volumeSettings;
}

bool KinFu_Common::update(InputArray _depth)
{
	CV_Assert(!_depth.empty());
	CV_Assert(_depth.size() == Size(volumeSettings.getIntegrateWidth(), volumeSettings.getIntegrateHeight()));
	return kinfuCommonUpdate(odometry, volume, _depth, prevFrame, renderFrame, pose, frameCounter);
}

void KinFu_Common::render(OutputArray image) const
{
	kinfuCommonRender(volume, renderFrame, image, lightPose);
}

void KinFu_Common::render(OutputArray image, const Matx44f& cameraPose) const
{
	kinfuCommonRender(volume, renderFrame, image, cameraPose, lightPose);
}

void KinFu_Common::reset()
{
	frameCounter = 0;
	pose = Affine3f::Identity().matrix;
	volume.reset();
}

void KinFu_Common::getCloud(OutputArray points, OutputArray normals) const
{
	volume.fetchPointsNormals(points, normals);
}

void KinFu_Common::getPoints(OutputArray points) const
{
	volume.fetchPointsNormals(points, noArray());
}

void KinFu_Common::getNormals(InputArray points, OutputArray normals) const
{
	volume.fetchNormals(points, normals);
}

const Affine3f KinFu_Common::getPose() const
{
	return Affine3f(this->pose);
}

} // namespace cv
