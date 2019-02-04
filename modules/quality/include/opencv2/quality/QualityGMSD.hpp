// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.

#ifndef OPENCV_QUALITY_QUALITYGMSD_HPP
#define OPENCV_QUALITY_QUALITYGMSD_HPP

#include "QualityBase.hpp"

namespace cv {
namespace quality {
namespace detail {
namespace gmsd {

    using mat_type = UMat;

    // holds computed values for an input mat
    struct mat_data
    {
        mat_type
            gradient_map
            , gradient_map_squared
            ;

        mat_data(const mat_type&);
    };  // mat_data
}   // gmsd
}   // detail

    /**
    @brief Full reference GMSD algorithm
    http://www4.comp.polyu.edu.hk/~cslzhang/IQA/GMSD/GMSD.htm
    */
    class CV_EXPORTS_W QualityGMSD
        : public QualityBase {
    public:

        /**
        @brief Compute GMSD
        @param cmpImgs Comparison images
        @returns Per-channel GMSD
        */
        CV_WRAP cv::Scalar compute(InputArrayOfArrays cmpImgs) CV_OVERRIDE;

        /** @brief Implements Algorithm::empty()  */
        CV_WRAP bool empty() const CV_OVERRIDE { return _refImgData.empty() && QualityBase::empty(); }

        /** @brief Implements Algorithm::clear()  */
        CV_WRAP void clear() CV_OVERRIDE { _refImgData.clear(); QualityBase::clear(); }

        /**
        @brief Create an object which calculates image quality
        @param refImgs input image(s) to use as the source for comparison
        */
        CV_WRAP static Ptr<QualityGMSD> create(InputArrayOfArrays refImgs);

        /**
        @brief static method for computing quality
        @param refImgs reference image(s)
        @param cmpImgs comparison image(s)
        @param qualityMaps output quality map(s), or cv::noArray()
        @returns cv::Scalar with per-channel quality value.  Values range from 0 (worst) to 1 (best)
        */
        CV_WRAP static cv::Scalar compute(InputArrayOfArrays refImgs, InputArrayOfArrays cmpImgs, OutputArrayOfArrays qualityMaps);

    protected:

        /** @brief Reference image data */
        std::vector<detail::gmsd::mat_data> _refImgData;

        /**
        @brief Constructor
        @param refImgData vector of reference images, converted to internal type
        */
        QualityGMSD(std::vector<detail::gmsd::mat_data> refImgData)
            : _refImgData(std::move(refImgData))
        {}

    };	// QualityGMSD
}
}
#endif