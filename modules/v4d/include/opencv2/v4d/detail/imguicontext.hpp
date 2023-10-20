// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.
// Copyright Amir Hassan (kallaballa) <amir@viel-zu.org>

#ifndef SRC_OPENCV_IMGUIContext_HPP_
#define SRC_OPENCV_IMGUIContext_HPP_

#include "opencv2/v4d/detail/framebuffercontext.hpp"
#include "imgui.h"

#include "opencv2/v4d/detail/imguicontext.hpp"

#if defined(OPENCV_V4D_USE_ES3) || defined(EMSCRIPTEN)
#   define IMGUI_IMPL_OPENGL_ES3
#endif

#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace cv {
namespace v4d {
namespace detail {
class CV_EXPORTS ImGuiContextImpl {
    friend class cv::v4d::V4D;
    cv::Ptr<FrameBufferContext> mainFbContext_;
    ImGuiContext* context_;
    std::function<void(ImGuiContext*)> renderCallback_;
    bool firstFrame_ = true;
public:
    CV_EXPORTS ImGuiContextImpl(cv::Ptr<FrameBufferContext> fbContext);
    CV_EXPORTS void build(std::function<void(ImGuiContext*)> fn);
protected:
    CV_EXPORTS void makeCurrent();
    CV_EXPORTS void render(bool displayFPS);
};
}
}
}

#endif /* SRC_OPENCV_IMGUIContext_HPP_ */