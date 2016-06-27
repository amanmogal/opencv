/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                           License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2015, University of Ostrava, Institute for Research and Applications of Fuzzy Modeling,
// Pavel Vlasanek, all rights reserved. Third party copyrights are property of their respective owners.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of the copyright holders may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the Intel Corporation or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/

#ifndef __OPENCV_PHASH_HPP__
#define __OPENCV_PHASH_HPP__

#include "opencv2/core.hpp"

namespace cv
{

namespace img_hash
{
    //! @addtogroup p_hash
    //! @{

    /** @brief Computes pHash value of the input image
    @param input Input CV_8UC3, CV_8UC1 array.
    @param hash Hash value of input, it will contain 8 uchar value
     */
    CV_EXPORTS_W void pHash(CV_IN_OUT cv::Mat const &input,
                            CV_OUT cv::Mat &hash);

    class CV_EXPORTS_W PHash : public ImgHashBase
    {
    public:
      CV_WRAP ~PHash();

      /** @brief Computes PHash of the input image
          @param input input CV_8UC3, CV_8UC1 array
          @param hash hash of the image
      */
      CV_WRAP virtual void compute(cv::Mat const &input, cv::Mat &hash);

      /** @brief Compare the hash value between inOne and inTwo
      @param hashOne Hash value one
      @param hashTwo Hash value two
      @return zero means the images are likely very similar;
      5 means a few things maybe different; 10 or more means
      they maybe are very different image
      */
      CV_WRAP virtual double compare(cv::Mat const &hashOne, cv::Mat const &hashTwo) const;

      CV_WRAP static Ptr<PHash> create();

    private:
        cv::Mat bitsImg;
        cv::Mat dctImg;
        cv::Mat grayFImg;
        cv::Mat grayImg;
        cv::Mat resizeImg;
        cv::Mat topLeftDCT;
    };

    //! @}
}
}

#endif // __OPENCV_PHASH_HPP__
