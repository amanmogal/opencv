#include "../precomp.hpp"
#include "layer_loaders.hpp"
#include <opencv2/dnn/shape_utils.hpp>
#include <climits>
#include "layers/layers_common.hpp"

namespace cv
{
namespace dnn
{

//Layers

//Convolution and Deconvolution
static void initConvDeconvLayerFromCaffe(Ptr<BaseConvolutionLayer> l, LayerParams &params)
{
    l->setParamsFrom(params);
    getConvolutionKernelParams(params, l->kernel.height, l->kernel.width, l->pad.height,
                               l->pad.width, l->stride.height, l->stride.width, l->dilation.height,
                               l->dilation.width, l->padMode);

    bool bias = params.get<bool>("bias_term", true);
    int numOutput = params.get<int>("num_output");
    int group = params.get<int>("group", 1);

    l->adjustPad.height = params.get<int>("adj_h", 0);
    l->adjustPad.width = params.get<int>("adj_w", 0);

    CV_Assert(numOutput % group == 0);
    CV_Assert((bias && l->blobs.size() == 2) || (!bias && l->blobs.size() == 1));
}

template<>
Ptr<Layer> createLayerFromCaffe<ConvolutionLayer>(LayerParams &params)
{
    Ptr<BaseConvolutionLayer> l = ConvolutionLayer::create();
    initConvDeconvLayerFromCaffe(l, params);
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
    int type = PoolingLayer::MAX;
    Size kernel, stride, pad;
    bool globalPooling;
    cv::String padMode;

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

    getPoolingKernelParams(params, kernel.height, kernel.width, globalPooling,
                           pad.height, pad.width, stride.height, stride.width, padMode);
    //getCaffeConvParams(params, kernel, pad, stride);

    Ptr<Layer> l;
    if (!globalPooling)
        l = PoolingLayer::create(type, kernel, stride, pad, padMode);
    else
        l = PoolingLayer::createGlobal(type);
    l->setParamsFrom(params);
    return l;
}

template<>
Ptr<Layer> createLayerFromCaffe<SoftmaxLayer>(LayerParams &params)
{
    int axis = params.get<int>("axis", 1);
    Ptr<Layer> l(SoftmaxLayer::create(axis));
    l->setParamsFrom(params);
    return l;
}

template<> //InnerProduct specialization
Ptr<Layer> createLayerFromCaffe<InnerProductLayer>(LayerParams &params)
{
    const std::vector<Mat> &blobs = params.blobs;
    CV_Assert(1 <= blobs.size() && blobs.size() <= 2);

    int numOutputs = params.get<int>("num_output");
    int innerSize = (int)blobs[0].total() / numOutputs;
    bool bias = params.get<bool>("bias_term", true);
    int axis = params.get<int>("axis", 1);

    CV_Assert(blobs[0].dims >= 2 && (size_t)(innerSize * numOutputs) == blobs[0].total());
    CV_Assert(!bias || (blobs.size() == 2 && (size_t)numOutputs == blobs[1].total()));

    Ptr<InnerProductLayer> l = InnerProductLayer::create(axis);
    l->setParamsFrom(params);
    l->blobs[0] = l->blobs[0].reshape(1, numOutputs);
    if (bias)
        l->blobs[1] = l->blobs[1].reshape(1, 1);

    return Ptr<Layer>(l);
}

template<> //LRNLayer specialization
Ptr<Layer> createLayerFromCaffe<LRNLayer>(LayerParams& params)
{
    int type = -1;
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
    double bias = params.get<double>("bias", 1);
    bool normBySize = params.get<bool>("norm_by_size", true);

    Ptr<Layer> l(LRNLayer::create(type, size, alpha, beta, bias, normBySize));
    l->setParamsFrom(params);
    return l;
}

template<>
Ptr<Layer> createLayerFromCaffe<MVNLayer>(LayerParams &params)
{
    Ptr<Layer> l(MVNLayer::create(
        params.get<bool>("normalize_variance", true),
        params.get<bool>("across_channels", false),
        params.get<double>("eps", 1e-9)
    ));
    l->setParamsFrom(params);
    return l;
}

/* Reshape layers */

template<>
Ptr<Layer> createLayerFromCaffe<ReshapeLayer>(LayerParams &params)
{
    int axis = params.get<int>("axis", 0);
    int numAxes = params.get<int>("num_axes", -1);
    bool enableReordering = params.get<bool>("reorder_dims", false);
    CV_Assert(numAxes >= -1);
    Range applyingRange = (numAxes == -1) ? Range(axis, INT_MAX) : Range(axis, axis + numAxes);

    std::vector<int> newShape;
    if (params.has("dim"))
    {
        const DictValue &paramShape = params.get("dim");
        int i, dims = paramShape.size();
        newShape.resize(dims);
        for (i = 0; i < dims; i++)
            newShape[i] = paramShape.get<int>(i);
    }

    Ptr<Layer> l(ReshapeLayer::create(newShape, applyingRange, enableReordering));
    l->setParamsFrom(params);
    return l;
}

template<>
Ptr<Layer> createLayerFromCaffe<ConcatLayer>(LayerParams& params)
{
    Ptr<Layer> l(ConcatLayer::create(params.get<int>("axis", 1)));
    l->setParamsFrom(params);
    return l;
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

    Ptr<Layer> l(SplitLayer::create(outputsCount));
    l->setParamsFrom(params);
    return l;
}

template<>
Ptr<Layer> createLayerFromCaffe<SliceLayer>(LayerParams& params)
{
    int axis = params.get<int>("axis", 1);

    Ptr<Layer> l;
    if (!params.has("slice_point"))
    {
        l = SliceLayer::create(axis);
    }
    else
    {
        const DictValue &indicesValue = params.get("slice_point");
        std::vector<int> sliceIndices(indicesValue.size());
        for (int i = 0; i < indicesValue.size(); i++)
            sliceIndices[i] = indicesValue.get<int>(i);

        l = SliceLayer::create(axis, sliceIndices);
    }
    l->setParamsFrom(params);
    return l;
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
    Ptr<Layer> l(ReLULayer::create(negative_slope));
    l->setParamsFrom(params);
    return l;
}

template<> //Power specialization
Ptr<Layer> createLayerFromCaffe<PowerLayer>(LayerParams& params)
{
    float power = params.get<float>("power", 1.0f);
    float scale = params.get<float>("scale", 1.0f);
    float shift = params.get<float>("shift", 0.0f);
    Ptr<Layer> l(PowerLayer::create(power, scale, shift));
    l->setParamsFrom(params);
    return l;
}

template<> //CropLayer specialization
Ptr<Layer> createLayerFromCaffe<CropLayer>(LayerParams& params)
{
    int start_axis = params.get<int>("axis", 2);
    DictValue *paramOffset = params.ptr("offset");

    std::vector<int> offset;
    if (paramOffset)
    {
        for (int i = 0; i < paramOffset->size(); i++)
            offset.push_back(paramOffset->get<int>(i));
    }

    Ptr<Layer> l(CropLayer::create(start_axis, offset));
    l->setParamsFrom(params);
    return l;
}

template<> //Eltwise specialization
Ptr<Layer> createLayerFromCaffe<EltwiseLayer>(LayerParams& params)
{
    EltwiseLayer::EltwiseOp op = EltwiseLayer::SUM;
    if (params.has("operation"))
    {
        String operation = params.get<String>("operation").toLowerCase();
        if (operation == "prod")
            op = EltwiseLayer::PROD;
        else if (operation == "sum")
            op = EltwiseLayer::SUM;
        else if (operation == "max")
            op = EltwiseLayer::MAX;
        else
            CV_Error(cv::Error::StsBadArg, "Unknown operaticon type \"" + operation + "\"");
    }

    std::vector<int> coeffs;
    if (params.has("coeff"))
    {
        DictValue paramCoeff = params.get("coeff");
        coeffs.resize(paramCoeff.size(), 1);
        for (int i = 0; i < paramCoeff.size(); i++)
        {
            coeffs[i] = paramCoeff.get<int>(i);
        }
    }
    Ptr<Layer> l(EltwiseLayer::create(op, coeffs));
    l->setParamsFrom(params);
    return l;
}

template<> //BatchNormLayer specialization
Ptr<Layer> createLayerFromCaffe<BatchNormLayer>(LayerParams& params)
{
    const std::vector<Mat> &blobs = params.blobs;
    CV_Assert(blobs.size() >= 3);

    bool hasWeights = params.get<bool>("has_weight", false);
    bool hasBias = params.get<bool>("has_bias", false);
    float epsilon = params.get<float>("eps", 1E-5);
    Ptr<BatchNormLayer> l = BatchNormLayer::create(hasWeights, hasBias, epsilon);
    l->setParamsFrom(params);

    return Ptr<Layer>(l);
}

template<> //ChannelsPReLULayer specialization
Ptr<Layer> createLayerFromCaffe<ChannelsPReLULayer>(LayerParams& params)
{
   CV_Assert(params.blobs.size() == 1);
   Ptr<ChannelsPReLULayer> l = ChannelsPReLULayer::create();
   l->setParamsFrom(params);

   return Ptr<Layer>(l);
}

template<> //MaxUnpoolLayer specialization
Ptr<Layer> createLayerFromCaffe<MaxUnpoolLayer>(LayerParams& params)
{
   Size poolKernel(params.get<int>("pool_k_w"), params.get<int>("pool_k_h")),
        poolPad(params.get<int>("pool_pad_w"), params.get<int>("pool_pad_h")),
        poolStride(params.get<int>("pool_stride_w"), params.get<int>("pool_stride_h"));
   Ptr<MaxUnpoolLayer> l = MaxUnpoolLayer::create(poolKernel, poolPad, poolStride);
   l->setParamsFrom(params);

   return Ptr<Layer>(l);
}

template<> //ScaleLayer specialization
Ptr<Layer> createLayerFromCaffe<ScaleLayer>(LayerParams& params)
{
   Ptr<ScaleLayer> l = ScaleLayer::create(params.get<bool>("bias_term", false));
   l->setParamsFrom(params);

   return Ptr<Layer>(l);
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

template Ptr<Layer> createLayerFromCaffe<CropLayer>(LayerParams&);
template Ptr<Layer> createLayerFromCaffe<EltwiseLayer>(LayerParams&);
template Ptr<Layer> createLayerFromCaffe<BatchNormLayer>(LayerParams&);
template Ptr<Layer> createLayerFromCaffe<ChannelsPReLULayer>(LayerParams&);
template Ptr<Layer> createLayerFromCaffe<MaxUnpoolLayer>(LayerParams&);
template Ptr<Layer> createLayerFromCaffe<ScaleLayer>(LayerParams&);
}
}
