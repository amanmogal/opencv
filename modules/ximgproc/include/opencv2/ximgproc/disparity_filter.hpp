/*
 *  By downloading, copying, installing or using the software you agree to this license.
 *  If you do not agree to this license, do not download, install,
 *  copy or use the software.
 *
 *
 *  License Agreement
 *  For Open Source Computer Vision Library
 *  (3 - clause BSD License)
 *
 *  Redistribution and use in source and binary forms, with or without modification,
 *  are permitted provided that the following conditions are met :
 *
 *  *Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and / or other materials provided with the distribution.
 *
 *  * Neither the names of the copyright holders nor the names of the contributors
 *  may be used to endorse or promote products derived from this software
 *  without specific prior written permission.
 *
 *  This software is provided by the copyright holders and contributors "as is" and
 *  any express or implied warranties, including, but not limited to, the implied
 *  warranties of merchantability and fitness for a particular purpose are disclaimed.
 *  In no event shall copyright holders or contributors be liable for any direct,
 *  indirect, incidental, special, exemplary, or consequential damages
 *  (including, but not limited to, procurement of substitute goods or services;
 *  loss of use, data, or profits; or business interruption) however caused
 *  and on any theory of liability, whether in contract, strict liability,
 *  or tort(including negligence or otherwise) arising in any way out of
 *  the use of this software, even if advised of the possibility of such damage.
 */

#ifndef __OPENCV_DISPARITYFILTER_HPP__
#define __OPENCV_DISPARITYFILTER_HPP__
#ifdef __cplusplus

#include <opencv2/core.hpp>

namespace cv
{
namespace ximgproc
{
    class CV_EXPORTS_W DisparityFilter : public Algorithm
    {
    public:
        CV_WRAP virtual void filter(InputArray disparity_map, InputArray left_view, OutputArray filtered_disparity_map,Rect ROI) = 0;
    };

    /////////////////////////////////////////////////////////////////////////////////////////////////

    class DisparityDTFilter : public DisparityFilter
    {
    public:
        virtual double getSigmaSpatial() = 0;
        virtual void setSigmaSpatial(double _sigmaSpatial) = 0;
        virtual double getSigmaColor() = 0;
        virtual void setSigmaColor(double _sigmaColor) = 0;
    };

    CV_EXPORTS_W
    Ptr<DisparityDTFilter> createDisparityDTFilter();

    class DisparityGuidedFilter : public DisparityFilter
    {
    public:
        virtual double getEps() = 0;
        virtual void setEps(double _eps) = 0;
        virtual int getRadius() = 0;
        virtual void setRadius(int _radius) = 0;
    };

    CV_EXPORTS_W
    Ptr<DisparityGuidedFilter> createDisparityGuidedFilter();

    class DisparityWLSFilter : public DisparityFilter
    {
        virtual double getLambda() = 0;
        virtual void setLambda(double _lambda) = 0;
        virtual double getSigmaColor() = 0;
        virtual void setSigmaColor(double _sigma_color) = 0;
    };

    CV_EXPORTS_W
    Ptr<DisparityWLSFilter> createDisparityWLSFilter();
}
}
#endif
#endif