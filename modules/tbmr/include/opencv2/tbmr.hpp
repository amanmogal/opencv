// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.

#include "opencv2/core.hpp"
#include "opencv2/features2d.hpp"

namespace cv {
    namespace tbmr {

        /** @defgroup tbmr Tree Based Morse Features

        The opencv tbmr module contains an algorithm to ...
        This module is implemented based on the paper Tree-Based Morse Regions: A Topological Approach to Local
        Feature Detection, IEEE 2014.


        Introduction to Tree-Based Morse Regions
        ----------------------------------------------


        This algorithm is executed in 2 stages:

        In the first stage, the algorithm computes Component trees (Min-tree and Max-tree) based on the input image.

        In the second stage, the Min- and Max-trees are used to extract TBMR candidates. The extraction can be compared to MSER,
        but uses a different criterion: Instead of calculating a stable path along the tree, we look for nodes in the tree,
        that have one child while their parent has more than one child.

        The Component tree calculation is based on union-find [Berger 2007 ICIP] + rank.

        */

        //! @addtogroup tbmr
        //! @{
        class CV_EXPORTS_W TBMR : public Feature2D {
        public:

            /** @brief Full constructor for %TBMR detector

            @param _min_area prune the area which smaller than minArea
            @param _max_area_relative prune the area which bigger than maxArea (max_area = _max_area_relative * image_size)
            */
            CV_WRAP static Ptr<TBMR> create(int _min_area = 60, float _max_area_relative = 0.01);


            /** @brief Detect %MSER regions

            @param image input image (8UC1)
            @param tbmrs resulting list of point sets
            */
            CV_WRAP virtual void detectRegions(InputArray image, CV_OUT std::vector<KeyPoint>& tbmrs) = 0;

            CV_WRAP virtual void setMinArea(int minArea) = 0;
            CV_WRAP virtual int getMinArea() const = 0;

            CV_WRAP virtual void setMaxAreaRelative(float maxArea) = 0;
            CV_WRAP virtual float getMaxAreaRelative() const = 0;
            CV_WRAP virtual String getDefaultName() const CV_OVERRIDE;
        };

        //! @}

    }
} // namespace cv { namespace hfs {
