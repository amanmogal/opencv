#include "precomp.hpp"

using namespace std;

namespace cv {
namespace ximgproc {

// look up table - there is one entry for each of the 2^8=256 possible
// combinations of 8 binary neighbors.
static uint8_t lut_zhang_iter0[] = {
    1, 1, 1, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 1,
    0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1,
    0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1,
    0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1,
    1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1,
    1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0,
    1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1,
    1, 1, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1};

static uint8_t lut_zhang_iter1[] = {
    1, 1, 1, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 1,
    0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1,
    0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1,
    0, 1, 1, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1,
    0, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0,
    1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1,
    0, 1, 1, 1};

static uint8_t lut_guo_iter0[] = {
    1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 1, 1,
    0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
    0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1,
    0, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 1, 0, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0,
    1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1};

static uint8_t lut_guo_iter1[] = {
    1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1,
    1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
    1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1,
    1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1,
    1, 1, 1, 1};

// Applies a thinning iteration to a binary image
static void thinningIteration(Mat img, int iter, int thinningType){
    Mat marker = Mat::zeros(img.size(), CV_8UC1);
    int rows = img.rows;
    int cols = img.cols;
    marker.col(0).setTo(1);
    marker.col(cols - 1).setTo(1);
    marker.row(0).setTo(1);
    marker.row(rows - 1).setTo(1);

    if(thinningType == THINNING_ZHANGSUEN){
        marker.forEach<uchar>([=](uchar& value, const int postion[]) {
            int i = postion[0];
            int j = postion[1];
            if (i == 0 || j == 0 || i == rows - 1 || j == cols - 1)
                return;

            auto ptr = img.ptr(i, j); // p1

            uchar p2 = ptr[-cols];
            uchar p3 = ptr[-cols + 1];
            uchar p4 = ptr[1];
            uchar p5 = ptr[cols + 1];
            uchar p6 = ptr[cols];
            uchar p7 = ptr[cols - 1];
            uchar p8 = ptr[-1];
            uchar p9 = ptr[-cols - 1];

            int neighbors = p9 | (p2 << 1) | (p3 << 2) | (p4 << 3) | (p5 << 4) | (p6 << 5) | (p7 << 6) | (p8 << 7);

            if (iter == 0)
                value = lut_zhang_iter0[neighbors];
            else
                value = lut_zhang_iter1[neighbors];
        });
    }
    if(thinningType == THINNING_GUOHALL){
        marker.forEach<uchar>([=](uchar& value, const int postion[]) {
            int i = postion[0];
            int j = postion[1];
            if (i == 0 || j == 0 || i == rows - 1 || j == cols - 1)
                return;

            auto ptr = img.ptr(i, j); // p1

            uchar p2 = ptr[-cols];
            uchar p3 = ptr[-cols + 1];
            uchar p4 = ptr[1];
            uchar p5 = ptr[cols + 1];
            uchar p6 = ptr[cols];
            uchar p7 = ptr[cols - 1];
            uchar p8 = ptr[-1];
            uchar p9 = ptr[-cols - 1];

            int neighbors = p9 | (p2 << 1) | (p3 << 2) | (p4 << 3) | (p5 << 4) | (p6 << 5) | (p7 << 6) | (p8 << 7);

            if (iter == 0)
                value = lut_guo_iter0[neighbors];
            else
                value = lut_guo_iter1[neighbors];
        });
    }

    img &= marker;
}

// Apply the thinning procedure to a given image
void thinning(InputArray input, OutputArray output, int thinningType){
    Mat processed = input.getMat().clone();
    CV_CheckTypeEQ(processed.type(), CV_8UC1, "");
    // Enforce the range of the input image to be in between 0 - 255
    processed /= 255;
    Mat prev = processed.clone();
    do {
        thinningIteration(processed, 0, thinningType);
        thinningIteration(processed, 1, thinningType);
        const auto res = cv::norm(processed, prev, cv::NORM_L1);
        if (res <= 0) { break; }
        processed.copyTo(prev);
    } while (true);

    processed *= 255;
    output.assign(processed);
}

} //namespace ximgproc
} //namespace cv
