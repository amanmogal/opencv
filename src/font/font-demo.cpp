#define CL_TARGET_OPENCL_VERSION 120

#include "../common/subsystems.hpp"

#include <string>
#include <algorithm>
#include <vector>
#include <sstream>
#include <limits>

constexpr unsigned int WIDTH = 1920;
constexpr unsigned int HEIGHT = 1080;
constexpr bool OFFSCREEN = false;
constexpr const char* OUTPUT_FILENAME = "font-demo.mkv";
constexpr const int VA_HW_DEVICE_INDEX = 0;
constexpr double FPS = 60;

constexpr float FONT_SIZE = 40.0f;

using std::cerr;
using std::endl;
using std::string;
using std::vector;
using std::stringstream;

int main(int argc, char **argv) {
    using namespace kb;

    //Initialize the application
    kb::init(WIDTH, HEIGHT);

    //Initialize VP9 HW encoding using VAAPI
    cv::VideoWriter writer(OUTPUT_FILENAME, cv::CAP_FFMPEG, cv::VideoWriter::fourcc('V', 'P', '9', '0'), FPS, cv::Size(WIDTH, HEIGHT), {
            cv::VIDEOWRITER_PROP_HW_ACCELERATION, cv::VIDEO_ACCELERATION_VAAPI,
            cv::VIDEOWRITER_PROP_HW_ACCELERATION_USE_OPENCL, 1
    });

    //Copy OpenCL Context for VAAPI. Must be called right after first VideoWriter/VideoCapture initialization.
    va::copy();

    //If we render offscreen we don't need x11.
    if (!OFFSCREEN)
        x11::init("font-demo");
    //Passing 'true' to egl::init() creates a debug OpenGL-context.
    egl::init(4, 6, 16);
    //Initialize OpenGL.
    gl::init();
    //Initialize nanovg.
    nvg::init();

    cerr << "EGL Version: " << egl::get_info() << endl;
    cerr << "OpenGL Version: " << gl::get_info() << endl;
    cerr << "OpenCL Platforms: " << endl << cl::get_info() << endl;

    //BGRA frame buffer and warped image
    cv::UMat frameBuffer, warped;
    //BGR video frame.
    cv::UMat videoFrame;

    //The text to display
    string text = cv::getBuildInformation();
    //Create a istringstream that we will read and the rewind. over again.
    std::istringstream iss(text);
    //Count the number of lines.
    off_t numLines = std::count(text.begin(), text.end(), '\n');

    //Derive the transformation matrix M for the pseudo 3D effect from src and dst.
    vector<cv::Point2f> src = {{0,0},{WIDTH,0},{WIDTH,HEIGHT},{0,HEIGHT}};
    vector<cv::Point2f> dst = {{WIDTH/3,0},{WIDTH/1.5,0},{WIDTH,HEIGHT},{0,HEIGHT}};
    cv::Mat M = cv::getPerspectiveTransform(src, dst);

    //Frame count.
    size_t cnt = 0;
    //Y-position of the current line in pixels.
    float y;

    while (true) {
        y = 0;

        //Activate the OpenCL context for OpenGL.
        gl::bind();
        //Begin a nanovg frame.
        nvg::begin();
        //Clear the screen with black. The clear color can be specified.
        nvg::clear();
        {
            using kb::nvg::vg;
            nvgFontSize(vg, FONT_SIZE);
            nvgFontFace(vg, "serif");
            nvgFillColor(vg, nvgHSLA(0.15, 1, 0.5, 255));
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);

            /** only draw lines that are visible **/

            //Progress measured in lines.
            off_t progressLines = cnt / FONT_SIZE;
            //How many lines to skip.
            off_t skipLines = (numLines - progressLines) - 1;
            skipLines = skipLines < 0 ? 0 : skipLines;
            //How many lines fit on the page.
            off_t pageLines = HEIGHT / FONT_SIZE;
            //How many pixels to translate the text down.
            off_t translateY = cnt - ((numLines - skipLines) * FONT_SIZE);
            nvgTranslate(vg, 0, translateY);

            for (std::string line; std::getline(iss, line); ) {
                //Check if all yet-to-crawl lines have been skipped.
                if(skipLines == 0) {
                    //Check if the current lines fits in the page.
                    if(((translateY + y) / FONT_SIZE) < pageLines) {
                        nvgText(vg, WIDTH/2.0, y, line.c_str(), line.c_str() + line.size());
                        y += FONT_SIZE;
                    } else {
                        //We can stop reading lines if the current line exceeds the page.
                        break;
                    }
                } else {
                   --skipLines;
                }
            }
        }
        //End a nanovg frame
        nvg::end();

        if(y == 0) {
            //Nothing drawn, exit.
            break;
        }

        //Rewind the istringstream.
        iss.clear(std::stringstream::goodbit);
        iss.seekg(0);

        //Aquire frame buffer from OpenGL.
        gl::acquire_from_gl(frameBuffer);
        //Pseudo 3D text effect.
        cv::warpPerspective(frameBuffer, warped, M, videoFrame.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar());
        //Copy back to frame buffer
        warped.copyTo(frameBuffer);
        //Color-conversion from BGRA to RGB. OpenCV/OpenCL.
        cv::cvtColor(warped, videoFrame, cv::COLOR_BGRA2RGB);
        //Transfer buffer ownership back to OpenGL.
        gl::release_to_gl(frameBuffer);

        //If x11 is enabled it displays the framebuffer in the native window. Returns false if the window was closed.
        if(!gl::display())
            break;

        //Activate the OpenCL context for VAAPI.
        va::bind();
        //Encode the frame using VAAPI on the GPU.
        writer << videoFrame;

        ++cnt;
        //Wrap the cnt around if it becomes to big.
        if(cnt == std::numeric_limits<size_t>().max())
            cnt = 0;

        print_fps();
    }

    return 0;
}
