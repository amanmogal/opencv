#include "../precomp.hpp"
#include "layer_loaders.hpp"
#include <opencv2/dnn/shape_utils.hpp>
#include <climits>

namespace cv
{
namespace dnn
{

//Utils

//Extracts params used into Conv, Deconv and Pooling layers
static void getCaffeConvParams(LayerParams &params, Size &kernel, Size &pad, Size &stride)
{
    if (params.has("kernel_h") && params.has("kernel_w"))
    {
        kernel.height = params.get<int>("kernel_h");
        kernel.width = params.get<int>("kernel_w");
    }
    else if (params.has("kernel_size"))
    {
        kernel.height = kernel.width = params.get<int>("kernel_size");
    }
    else
    {
        CV_Error(Error::StsBadArg, "kernel_size (or kernel_h and kernel_w) not specified");
    }
    CV_Assert(kernel.height > 0 && kernel.width > 0);

    if (params.has("pad_h") && params.has("pad_w"))
    {
        pad.height = params.get<int>("pad_h");
        pad.width = params.get<int>("pad_w");
    }
    else
    {
        pad.height = pad.width = params.get<int>("pad", 0);
    }
    CV_Assert(pad.height >= 0 && pad.width >= 0);

    if (params.has("stride_h") && params.has("stride_w"))
    {
        stride.height = params.get<int>("stride_h");
        stride.width = params.get<int>("stride_w");
    }
    else
    {
        stride.height = stride.width = params.get<int>("stride", 1);
    }
    CV_Assert(stride.height > 0 && stride.width > 0);
}

static void getCaffeConvDilation(LayerParams &params, Size &dilation)
{
    if (params.has("dilation_h") && params.has("dilation_w"))
    {
        dilation.height = params.get<int>("dilation_h");
        dilation.width = params.get<int>("dilation_w");
    }
    else
    {
        dilation.height = dilation.width = params.get<int>("dilation", 1);
    }
    CV_Assert(dilation.height > 0 && dilation.width > 0);
}

//Layers

//Convolution and Deconvolution
static void initConvDeconvLayerFromCaffe(Ptr<BaseConvolutionLayer> l, LayerParams &params)
{
    l->setParamsFrom(params);
    getCaffeConvParams(params, l->kernel, l->pad, l->stride);

    bool bias = params.get<bool>("bias_term", true);
    int numOutput = params.get<int>("num_output");
    int group = params.get<int>("group", 1);

    CV_Assert(numOutput % group == 0);
    CV_Assert((bias && l->blobs.size() == 2) || (!bias && l->blobs.size() == 1));
}

template<>
Ptr<Layer> createLayerFromCaffe<ConvolutionLayer>(LayerParams &params)
{
    Ptr<BaseConvolutionLayer> l = ConvolutionLayer::create();
    initConvDeconvLayerFromCaffe(l, params);
    getCaffeConvDilation(params, l->dilation);
    return Ptr<Layer>(l);
}

template<>
Ptr<Layer> createLayerFromCaffe<DeconvolutionLayer>(LayerParams &params)
{
    Ptr<BaseConvolutionLayer> l = DeconvolutionLayer::create();
    initConvDeconvLayerFromCaffe(l, params);
    return Ptr<Layer>(l);
}

template<>
Ptr<Layer> createLayerFromCaffe<PoolingLayer>(LayerParams &params)
{
    int type;
    Size kernel, stride, pad;
    bool globalPooling;

    if (params.has("pool"))
    {
        String pool = params.get<String>("pool").toLowerCase();
        if (pool == "max")
            type = PoolingLayer::MAX;
        else if (pool == "ave")
            type = PoolingLayer::AVE;
        else if (pool == "stochastic")
            type = PoolingLayer::STOCHASTIC;
        else
            CV_Error(Error::StsBadArg, "Unknown pooling type \"" + pool + "\"");
    }
    else
    {
        type = PoolingLayer::MAX;
    }

    getCaffeConvParams(params, kernel, pad, stride);

    globalPooling = params.has("global_pooling");

    if (globalPooling)
    {
        if(params.has("kernel_h") || params.has("kernel_w") || params.has("kernel_size"))
        {
            CV_Error(cv::Error::StsBadArg, "In global_pooling mode, kernel_size (or kernel_h and kernel_w) cannot be specified");
        }
        if(pad.height != 0 || pad.width != 0 || stride.height != 1 || stride.width != 1)
        {
            CV_Error(cv::Error::StsBadArg, "In global_pooling mode, pad_h and pad_w must be = 0, and stride_h and stride_w must be = 1");
        }
    }

    return Ptr<Layer>(PoolingLayer::create(type, kernel, stride, pad));
}

template<>
Ptr<Layer> createLayerFromCaffe<SoftmaxLayer>(LayerParams &params)
{
    int axis = params.get<int>("axis", 1);
    return Ptr<Layer>(SoftmaxLayer::create(axis));
}

template<> //InnerProduct specialization
Ptr<Layer> createLayerFromCaffe<InnerProductLayer>(LayerParams &params)
{
    const std::vector<Blob> &blobs = params.blobs;
    CV_Assert(1 <= blobs.size() && blobs.size() <= 2);

    int numOutputs = params.get<int>("num_output");
    int innerSize = (int)blobs[0].total() / numOutputs;
    bool bias = params.get<bool>("bias_term", true);
    int axis = params.get<int>("axis", 1);

    CV_Assert(blobs[0].dims() >= 2 && (size_t)(innerSize * numOutputs) == blobs[0].total());
    CV_Assert(!bias || (blobs.size() == 2 && (size_t)numOutputs == blobs[1].total()));

    Ptr<InnerProductLayer> l = InnerProductLayer::create(axis);
    l->setParamsFrom(params);
    l->blobs[0].reshape(Shape(numOutputs, innerSize));
    if (bias)
        l->blobs[1].reshape(Shape(1, numOutputs));

    return Ptr<Layer>(l);
}

template<> //LRNLayer specialization
Ptr<Layer> createLayerFromCaffe<LRNLayer>(LayerParams& params)
{
    int type;
    String nrmType = params.get<String>("norm_region", "ACROSS_CHANNELS");
    if (nrmType == "ACROSS_CHANNELS")
        type = LRNLayer::CHANNEL_NRM;
    else if (nrmType == "WITHIN_CHANNEL")
        type = LRNLayer::SPATIAL_NRM;
    else
        CV_Error(Error::StsBadArg, "Unknown region type \"" + nrmType + "\"");

    int size = params.get<int>("local_size", 5);
    if (size % 2 != 1 || size <= 0)
        CV_Error(Error::StsBadArg, "LRN layer supports only positive odd values for local_size");

    double alpha = params.get<double>("alpha", 1);
    double beta = params.get<double>("beta", 0.75);

    return Ptr<Layer>(LRNLayer::create(type, size, alpha, beta));
}

template<>
Ptr<Layer> createLayerFromCaffe<MVNLayer>(LayerParams &params)
{
    return Ptr<Layer>(MVNLayer::create(
                          params.get<bool>("normalize_variance", true),
                          params.get<bool>("across_channels", false),
                          params.get<double>("eps", 1e-9)
                      ));
}

/* Reshape layers */

template<>
Ptr<Layer> createLayerFromCaffe<ReshapeLayer>(LayerParams &params)
{
    int axis = params.get<int>("axis", 0);
    int numAxes = params.get<int>("num_axes", -1);
    CV_Assert(numAxes >= -1);
    Range applyingRange = (numAxes == -1) ? Range(axis, INT_MAX) : Range(axis, axis + numAxes);

    Shape newShape;
    if (params.has("dim"))
    {
        const DictValue &paramShape = params.get("dim");
        newShape = Shape::all(paramShape.size());
        for (int i = 0; i < paramShape.size(); i++)
            newShape[i] = paramShape.get<int>(i);
    }
    else
        newShape = Shape::all(0);

    return Ptr<Layer>(ReshapeLayer::create(newShape, applyingRange));
}

Ptr<Layer> createFlattenLayerFromCaffe(LayerParams&)
{
    return Ptr<Layer>(ReshapeLayer::create(Shape(0, -1)));
}

template<>
Ptr<Layer> createLayerFromCaffe<ConcatLayer>(LayerParams& params)
{
    return Ptr<Layer>(ConcatLayer::create(params.get<int>("axis", 1)));
}

template<>
Ptr<Layer> createLayerFromCaffe<SplitLayer>(LayerParams &params)
{
    int outputsCount;

    //TODO: maybe "top_count" param is useless because it can be determined by output connections number
    if (params.has("top_count"))
    {
        outputsCount = params.get<int>("top_count");
        CV_Assert(outputsCount >= 0);
    }
    else
    {
        outputsCount = -1;
    }

    return Ptr<Layer>(SplitLayer::create(outputsCount));
}

template<>
Ptr<Layer> createLayerFromCaffe<SliceLayer>(LayerParams& params)
{
    int axis = params.get<int>("axis", 1);

    if (!params.has("slice_point"))
    {
        return Ptr<Layer>(SliceLayer::create(axis));
    }
    else
    {
        const DictValue &indicesValue = params.get("slice_point");
        std::vector<int> sliceIndices(indicesValue.size());
        for (int i = 0; i < indicesValue.size(); i++)
            sliceIndices[i] = indicesValue.get<int>(i);

        return Ptr<Layer>(SliceLayer::create(axis, sliceIndices));
    }
}

/* Activation layers */

template <typename ActivationLayer> //Intended for parameters-free activations
Ptr<Layer> createLayerFromCaffe(LayerParams&)
{
    return Ptr<Layer>(ActivationLayer::create());
}

template<> //ReLU specialization
Ptr<Layer> createLayerFromCaffe<ReLULayer>(LayerParams& params)
{
    float negative_slope = params.get<float>("negative_slope", 0.f);
    return Ptr<Layer>(ReLULayer::create(negative_slope));
}

template<> //Power specialization
Ptr<Layer> createLayerFromCaffe<PowerLayer>(LayerParams& params)
{
    float power = params.get<float>("power", 1.0f);
    float scale = params.get<float>("scale", 1.0f);
    float shift = params.get<float>("shift", 0.0f);
    return Ptr<Layer>(PowerLayer::create(power, scale, shift));
}

//Explicit instantiation
template Ptr<Layer> createLayerFromCaffe<ConvolutionLayer>(LayerParams&);
template Ptr<Layer> createLayerFromCaffe<DeconvolutionLayer>(LayerParams&);
template Ptr<Layer> createLayerFromCaffe<SoftmaxLayer>(LayerParams&);
template Ptr<Layer> createLayerFromCaffe<InnerProductLayer>(LayerParams&);
template Ptr<Layer> createLayerFromCaffe<LRNLayer>(LayerParams&);
template Ptr<Layer> createLayerFromCaffe<MVNLayer>(LayerParams&);

template Ptr<Layer> createLayerFromCaffe<ConcatLayer>(LayerParams&);
template Ptr<Layer> createLayerFromCaffe<SliceLayer>(LayerParams&);
template Ptr<Layer> createLayerFromCaffe<SplitLayer>(LayerParams&);

template Ptr<Layer> createLayerFromCaffe<ReLULayer>(LayerParams&);
template Ptr<Layer> createLayerFromCaffe<SigmoidLayer>(LayerParams&);
template Ptr<Layer> createLayerFromCaffe<TanHLayer>(LayerParams&);
template Ptr<Layer> createLayerFromCaffe<AbsLayer>(LayerParams&);
template Ptr<Layer> createLayerFromCaffe<BNLLLayer>(LayerParams&);
template Ptr<Layer> createLayerFromCaffe<PowerLayer>(LayerParams&);

}
}
