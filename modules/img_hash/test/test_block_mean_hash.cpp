/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                        Intel License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2000, Intel Corporation, all rights reserved.
// Third party copyrights are property of their respective owners.
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
//   * The name of Intel Corporation may not be used to endorse or promote products
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

#include "test_precomp.hpp"

using namespace cv;


namespace cv
{

namespace img_hash
{

class BlockMeanHashTester
{
public:
    std::vector<double> const&
    getMean(BlockMeanHash &input) const
    {
        return input.mean_;
    }
};

}
}

/**
 *The expected results of this test case are come from the Phash library,
 *I use it as golden model
 */
class CV_BlockMeanHashTest : public cvtest::BaseTest
{
public:
    CV_BlockMeanHashTest();
protected:
    void run(int /* idx */);

    void testMeanMode0();
    void testMeanMode1();
    void testHashMode0();
    void testHashMode1();

    cv::Mat input;
    cv::Mat hash;
    cv::img_hash::BlockMeanHash bmh;
    cv::img_hash::BlockMeanHashTester tester;
};

CV_BlockMeanHashTest::CV_BlockMeanHashTest()
{
    input.create(256, 256, CV_8U);
    for(int row = 0; row != input.rows; ++row)
    {
        uchar value = static_cast<uchar>(row);
        for(int col = 0; col != input.cols; ++col)
        {
            input.at<uchar>(row, col) = value++;
        }
    }
}

void CV_BlockMeanHashTest::testMeanMode0()
{
    std::vector<double> const &features = tester.getMean(bmh);
    double const expectResult[] =
    {15,31,47,63,79,95,111,127,143,159,175,191,207,223,239,135,
     31,47,63,79,95,111,127,143,159,175,191,207,223,239,135,15,
     47,63,79,95,111,127,143,159,175,191,207,223,239,135,15,31,
     63,79,95,111,127,143,159,175,191,207,223,239,135,15,31,47,
     79,95,111,127,143,159,175,191,207,223,239,135,15,31,47,63,
     95,111,127,143,159,175,191,207,223,239,135,15,31,47,63,79,
     111,127,143,159,175,191,207,223,239,135,15,31,47,63,79,95,
     127,143,159,175,191,207,223,239,135,15,31,47,63,79,95,111,
     143,159,175,191,207,223,239,135,15,31,47,63,79,95,111,127,
     159,175,191,207,223,239,135,15,31,47,63,79,95,111,127,143,
     175,191,207,223,239,135,15,31,47,63,79,95,111,127,143,159,
     191,207,223,239,135,15,31,47,63,79,95,111,127,143,159,175,
     207,223,239,135,15,31,47,63,79,95,111,127,143,159,175,191,
     223,239,135,15,31,47,63,79,95,111,127,143,159,175,191,207,
     239,135,15,31,47,63,79,95,111,127,143,159,175,191,207,223,
     135,15,31,47,63,79,95,111,127,143,159,175,191,207,223,239,};
    for(size_t i = 0; i != features.size(); ++i)
    {
        ASSERT_NEAR(features[i], expectResult[i], 0.0001);
    }
}

void CV_BlockMeanHashTest::testMeanMode1()
{
    std::vector<double> const &features = tester.getMean(bmh);
    double const expectResult[] =
    {15,23,31,39,47,55,63,71,79,87,95,103,111,119,127,135,143,151,159,167,175,183,191,199,207,215,223,231,239,219,135,
     23,31,39,47,55,63,71,79,87,95,103,111,119,127,135,143,151,159,167,175,183,191,199,207,215,223,231,239,219,135,43,
     31,39,47,55,63,71,79,87,95,103,111,119,127,135,143,151,159,167,175,183,191,199,207,215,223,231,239,219,135,43,15,
     39,47,55,63,71,79,87,95,103,111,119,127,135,143,151,159,167,175,183,191,199,207,215,223,231,239,219,135,43,15,23,
     47,55,63,71,79,87,95,103,111,119,127,135,143,151,159,167,175,183,191,199,207,215,223,231,239,219,135,43,15,23,31,
     55,63,71,79,87,95,103,111,119,127,135,143,151,159,167,175,183,191,199,207,215,223,231,239,219,135,43,15,23,31,39,
     63,71,79,87,95,103,111,119,127,135,143,151,159,167,175,183,191,199,207,215,223,231,239,219,135,43,15,23,31,39,47,
     71,79,87,95,103,111,119,127,135,143,151,159,167,175,183,191,199,207,215,223,231,239,219,135,43,15,23,31,39,47,55,
     79,87,95,103,111,119,127,135,143,151,159,167,175,183,191,199,207,215,223,231,239,219,135,43,15,23,31,39,47,55,63,
     87,95,103,111,119,127,135,143,151,159,167,175,183,191,199,207,215,223,231,239,219,135,43,15,23,31,39,47,55,63,71,
     95,103,111,119,127,135,143,151,159,167,175,183,191,199,207,215,223,231,239,219,135,43,15,23,31,39,47,55,63,71,79,
     103,111,119,127,135,143,151,159,167,175,183,191,199,207,215,223,231,239,219,135,43,15,23,31,39,47,55,63,71,79,87,
     111,119,127,135,143,151,159,167,175,183,191,199,207,215,223,231,239,219,135,43,15,23,31,39,47,55,63,71,79,87,95,
     119,127,135,143,151,159,167,175,183,191,199,207,215,223,231,239,219,135,43,15,23,31,39,47,55,63,71,79,87,95,103,
     127,135,143,151,159,167,175,183,191,199,207,215,223,231,239,219,135,43,15,23,31,39,47,55,63,71,79,87,95,103,111,
     135,143,151,159,167,175,183,191,199,207,215,223,231,239,219,135,43,15,23,31,39,47,55,63,71,79,87,95,103,111,119,
     143,151,159,167,175,183,191,199,207,215,223,231,239,219,135,43,15,23,31,39,47,55,63,71,79,87,95,103,111,119,127,
     151,159,167,175,183,191,199,207,215,223,231,239,219,135,43,15,23,31,39,47,55,63,71,79,87,95,103,111,119,127,135,
     159,167,175,183,191,199,207,215,223,231,239,219,135,43,15,23,31,39,47,55,63,71,79,87,95,103,111,119,127,135,143,
     167,175,183,191,199,207,215,223,231,239,219,135,43,15,23,31,39,47,55,63,71,79,87,95,103,111,119,127,135,143,151,
     175,183,191,199,207,215,223,231,239,219,135,43,15,23,31,39,47,55,63,71,79,87,95,103,111,119,127,135,143,151,159,
     183,191,199,207,215,223,231,239,219,135,43,15,23,31,39,47,55,63,71,79,87,95,103,111,119,127,135,143,151,159,167,
     191,199,207,215,223,231,239,219,135,43,15,23,31,39,47,55,63,71,79,87,95,103,111,119,127,135,143,151,159,167,175,
     199,207,215,223,231,239,219,135,43,15,23,31,39,47,55,63,71,79,87,95,103,111,119,127,135,143,151,159,167,175,183,
     207,215,223,231,239,219,135,43,15,23,31,39,47,55,63,71,79,87,95,103,111,119,127,135,143,151,159,167,175,183,191,
     215,223,231,239,219,135,43,15,23,31,39,47,55,63,71,79,87,95,103,111,119,127,135,143,151,159,167,175,183,191,199,
     223,231,239,219,135,43,15,23,31,39,47,55,63,71,79,87,95,103,111,119,127,135,143,151,159,167,175,183,191,199,207,
     231,239,219,135,43,15,23,31,39,47,55,63,71,79,87,95,103,111,119,127,135,143,151,159,167,175,183,191,199,207,215,
     239,219,135,43,15,23,31,39,47,55,63,71,79,87,95,103,111,119,127,135,143,151,159,167,175,183,191,199,207,215,223,
     219,135,43,15,23,31,39,47,55,63,71,79,87,95,103,111,119,127,135,143,151,159,167,175,183,191,199,207,215,223,231,
     135,43,15,23,31,39,47,55,63,71,79,87,95,103,111,119,127,135,143,151,159,167,175,183,191,199,207,215,223,231,239,};
    for(size_t i = 0; i != features.size(); ++i)
    {
        ASSERT_NEAR(features[i], expectResult[i], 0.0001);
    }
}

void CV_BlockMeanHashTest::testHashMode0()
{
    uchar const expectResult[] =
    {0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,
     0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,0,
     0,0,0,0,0,0,1,1,1,1,1,1,1,1,0,0,
     0,0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,
     0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,
     0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,
     0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,
     0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,
     1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,
     1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,1,
     1,1,1,1,1,1,0,0,0,0,0,0,0,0,1,1,
     1,1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,
     1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,
     1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,1,
     1,1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,
     1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,
    };

    for(int i = 0; i != hash.cols; ++i)
    {
        EXPECT_EQ(expectResult[i], hash.at<uchar>(0, i));
    }
}

void CV_BlockMeanHashTest::testHashMode1()
{
    uchar const expectResult[] =
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
     1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,
     1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,
     1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,
     1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,
     1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,
     1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,
     1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,
     1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,
     1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,
     1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,
     1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,
     1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,
     1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    };

    for(int i = 0; i != hash.cols; ++i)
    {
        EXPECT_EQ(expectResult[i], hash.at<uchar>(0, i));
    }
}

void CV_BlockMeanHashTest::run(int)
{
    bmh.compute(input, hash);
    testMeanMode0();
    testHashMode0();

    bmh.setMode(1);
    bmh.compute(input, hash);
    testMeanMode1();
    testHashMode1();
}

TEST(block_mean_hash_test, accuracy) { CV_BlockMeanHashTest test; test.safe_run(); }
