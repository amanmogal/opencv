// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html

// This code is also subject to the license terms in the LICENSE_KinectFusion.md file found in this module's directory

//#include "precomp.hpp"
#include "kinfu_impl.hpp"
#include "kinfu_functions.hpp"

namespace cv {

KinFu::Impl::Impl()
{
	this->odometrySettings = OdometrySettings();
	this->volumeSettings = VolumeSettings(VolumeType::TSDF);

	this->odometry = Odometry(OdometryType::DEPTH, this->odometrySettings, OdometryAlgoType::COMMON);
	this->volume = Volume(VolumeType::TSDF, this->volumeSettings);
}


KinFu_Common::KinFu_Common()
	: Impl()
{
	this->frameCounter = 0;
//	this->odometrySettings = OdometrySettings();
//	this->volumeSettings = VolumeSettings(VolumeType::TSDF);

//	this->odometry = Odometry(OdometryType::DEPTH, this->odometrySettings, OdometryAlgoType::COMMON);
//	this->volume = Volume(VolumeType::TSDF, this->volumeSettings);
}

KinFu_Common::~KinFu_Common()
{
}

OdometryFrame KinFu_Common::createOdometryFrame() const
{
	return OdometryFrame();
}

bool KinFu_Common::update(InputArray _depth)
{
	CV_Assert(!_depth.empty() && _depth.size() == Size(volumeSettings.getIntegrateHeight(), volumeSettings.getIntegrateWidth()));
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

}

void KinFu_Common::getCloud(OutputArray points, OutputArray normals) const
{

}

void KinFu_Common::getPoints(OutputArray points) const
{

}

void KinFu_Common::getNormals(InputArray points, OutputArray normals) const
{

}

const Affine3f KinFu_Common::getPose() const
{
	return Affine3f(this->pose);
}

} // namespace cv
