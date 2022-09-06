// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.
#include "precomp.hpp"

namespace cv{
    namespace imgaug{
        uint64 state = getTickCount();
        RNG rng(state);

        void setSeed(uint64 seed){
            rng.state = seed;
        }
    }
}