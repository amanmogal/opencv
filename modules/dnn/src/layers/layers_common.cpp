#include "layers_common.hpp"

namespace cv
{
namespace dnn
{

void getKernelParams(LayerParams &params, int &kernelH, int &kernelW, int &padH, int &padW, int &strideH, int &strideW)
{
    if (params.has("kernel_h") && params.has("kernel_w"))
    {
        kernelH = params.get<int>("kernel_h");
        kernelW = params.get<int>("kernel_w");
    }
    else if (params.has("kernel_size"))
    {
        kernelH = kernelW = params.get<int>("kernel_size");
    }
    else
    {
        CV_Error(cv::Error::StsBadArg, "kernel_size (or kernel_h and kernel_w) not specified");
    }

    if (params.has("pad_h") && params.has("pad_w"))
    {
        padH = params.get<int>("pad_h");
        padW = params.get<int>("pad_w");
    }
    else
    {
        padH = padW = params.get<int>("pad", 0);
    }

    if (params.has("stride_h") && params.has("stride_w"))
    {
        strideH = params.get<int>("stride_h");
        strideW = params.get<int>("stride_w");
    }
    else
    {
        strideH = strideW = params.get<int>("stride", 1);
    }

    CV_Assert(kernelH > 0 && kernelW > 0 && padH >= 0 && padW >= 0 && strideH > 0 && strideW > 0);
}

}
}
