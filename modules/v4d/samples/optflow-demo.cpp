// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.
// Copyright Amir Hassan (kallaballa) <amir@viel-zu.org>

#include <opencv2/v4d/v4d.hpp>

#include <opencv2/features2d.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/optflow.hpp>
#include <opencv2/core/ocl.hpp>

#include <cmath>
#include <vector>
#include <set>
#include <string>
#include <random>
#include <tuple>
#include <array>
#include <utility>


using std::cerr;
using std::endl;
using std::vector;
using std::string;

/* Demo parameters */

#ifndef __EMSCRIPTEN__
constexpr long unsigned int WIDTH = 1920;
constexpr long unsigned int HEIGHT = 1080;
#else
constexpr long unsigned int WIDTH = 960;
constexpr long unsigned int HEIGHT = 960;
#endif
const unsigned long DIAG = hypot(double(WIDTH), double(HEIGHT));
#ifndef __EMSCRIPTEN__
constexpr const char* OUTPUT_FILENAME = "../optflow-demo.mkv";
#endif
constexpr bool OFFSCREEN = false;


/* Visualization parameters */

//How the background will be visualized
enum BackgroundModes {
    GREY,
    COLOR,
    VALUE,
    BLACK
};

//Post-processing modes for the foreground
enum PostProcModes {
    GLOW,
    BLOOM,
    DISABLED
};

// Generate the foreground at this scale.
static float fg_scale = 0.5f;
// On every frame the foreground loses on brightness. Specifies the loss in percent.
#ifndef __EMSCRIPTEN__
static float fg_loss = 2.5;
#else
static float fg_loss = 10.0;
#endif
//Convert the background to greyscale
static BackgroundModes background_mode = GREY;
// Peak thresholds for the scene change detection. Lowering them makes the detection more sensitive but
// the default should be fine.
static float scene_change_thresh = 0.29f;
static float scene_change_thresh_diff = 0.1f;
// The theoretical maximum number of points to track which is scaled by the density of detected points
// and therefor is usually much smaller.
#ifndef __EMSCRIPTEN__
static int max_points = 250000;
#else
static int max_points = 100000;
#endif
// How many of the tracked points to lose intentionally, in percent.
#ifndef __EMSCRIPTEN__
static float point_loss = 25;
#else
static float point_loss = 10;
#endif
// The theoretical maximum size of the drawing stroke which is scaled by the area of the convex hull
// of tracked points and therefor is usually much smaller.
static int max_stroke = 10;

// Red, green, blue and alpha. All from 0.0f to 1.0f
static float effect_color[4] = {1.0f, 0.75f, 0.4f, 1.0f};
//display on-screen FPS
static bool show_fps = true;
//Stretch frame buffer to window size
static bool stretch = false;
//The post processing mode
#ifndef __EMSCRIPTEN__
static PostProcModes post_proc_mode = GLOW;
#else
static PostProcModes post_proc_mode = DISABLED;
#endif
// Intensity of glow or bloom defined by kernel size. The default scales with the image diagonal.
static int glow_kernel_size = std::max(int(DIAG / 100 % 2 == 0 ? DIAG / 100 + 1 : DIAG / 100), 1);
//The lightness selection threshold
static int bloom_thresh = 210;
//The intensity of the bloom filter
static float bloom_gain = 3;

using namespace cv::v4d;


//Uses background subtraction to generate a "motion mask"
static void prepare_motion_mask(const cv::UMat& srcGrey, cv::UMat& motionMaskGrey) {
	thread_local cv::Ptr<cv::BackgroundSubtractor> bg_subtrator = cv::createBackgroundSubtractorMOG2(100, 16.0, false);
	thread_local int morph_size = 1;
	thread_local cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2 * morph_size + 1, 2 * morph_size + 1), cv::Point(morph_size, morph_size));

    bg_subtrator->apply(srcGrey, motionMaskGrey);
    //Surpress speckles
    cv::morphologyEx(motionMaskGrey, motionMaskGrey, cv::MORPH_OPEN, element, cv::Point(element.cols >> 1, element.rows >> 1), 2, cv::BORDER_CONSTANT, cv::morphologyDefaultBorderValue());
}

//Detect points to track
static void detect_points(const cv::UMat& srcMotionMaskGrey, vector<cv::Point2f>& points) {
	thread_local cv::Ptr<cv::FastFeatureDetector> detector = cv::FastFeatureDetector::create(1, false);
	thread_local vector<cv::KeyPoint> tmpKeyPoints;

    detector->detect(srcMotionMaskGrey, tmpKeyPoints);

    points.clear();
    for (const auto &kp : tmpKeyPoints) {
        points.push_back(kp.pt);
    }
}

//Detect extrem changes in scene content and report it
static bool detect_scene_change(const cv::UMat& srcMotionMaskGrey, const float thresh, const float theshDiff) {
	thread_local float last_movement = 0;

    float movement = cv::countNonZero(srcMotionMaskGrey) / float(srcMotionMaskGrey.cols * srcMotionMaskGrey.rows);
    float relation = movement > 0 && last_movement > 0 ? std::max(movement, last_movement) / std::min(movement, last_movement) : 0;
    float relM = relation * log10(1.0f + (movement * 9.0));
    float relLM = relation * log10(1.0f + (last_movement * 9.0));

    bool result = !((movement > 0 && last_movement > 0 && relation > 0)
            && (relM < thresh && relLM < thresh && fabs(relM - relLM) < theshDiff));
    last_movement = (last_movement + movement) / 2.0f;
    return result;
}

//Visualize the sparse optical flow
static void visualize_sparse_optical_flow(const cv::UMat &prevGrey, const cv::UMat &nextGrey, const vector<cv::Point2f> &detectedPoints, const float scaleFactor, const int maxStrokeSize, const cv::Scalar color, const int maxPoints, const float pointLossPercent) {
	thread_local vector<cv::Point2f> hull, prevPoints, nextPoints, newPoints;
	thread_local vector<cv::Point2f> upPrevPoints, upNextPoints;
	thread_local std::vector<uchar> status;
	thread_local std::vector<float> err;
	thread_local std::random_device rd;
	thread_local std::mt19937 g(rd());

    //less then 5 points is a degenerate case (e.g. the corners of a video frame)
    if (detectedPoints.size() > 4) {
        cv::convexHull(detectedPoints, hull);
        float area = cv::contourArea(hull);
        //make sure the area of the point cloud is positive
        if (area > 0) {
            float density = (detectedPoints.size() / area);
            //stroke size is biased by the area of the point cloud
            float strokeSize = maxStrokeSize * pow(area / (nextGrey.cols * nextGrey.rows), 0.33f);
            //max points is biased by the densitiy of the point cloud
            size_t currentMaxPoints = ceil(density * maxPoints);

            //lose a number of random points specified by pointLossPercent
            std::shuffle(prevPoints.begin(), prevPoints.end(), g);
            prevPoints.resize(ceil(prevPoints.size() * (1.0f - (pointLossPercent / 100.0f))));

            //calculate how many newly detected points to add
            size_t copyn = std::min(detectedPoints.size(), (size_t(std::ceil(currentMaxPoints)) - prevPoints.size()));
            if (prevPoints.size() < currentMaxPoints) {
                std::copy(detectedPoints.begin(), detectedPoints.begin() + copyn, std::back_inserter(prevPoints));
            }

            //calculate the sparse optical flow
            cv::calcOpticalFlowPyrLK(prevGrey, nextGrey, prevPoints, nextPoints, status, err);
            newPoints.clear();
            if (prevPoints.size() > 1 && nextPoints.size() > 1) {
                //scale the points to original size
                upNextPoints.clear();
                upPrevPoints.clear();
                for (cv::Point2f pt : prevPoints) {
                    upPrevPoints.push_back(pt /= scaleFactor);
                }

                for (cv::Point2f pt : nextPoints) {
                    upNextPoints.push_back(pt /= scaleFactor);
                }

                using namespace cv::v4d::nvg;
                //start drawing
                beginPath();
                strokeWidth(strokeSize);
                strokeColor(color);

                for (size_t i = 0; i < prevPoints.size(); i++) {
                    if (status[i] == 1 //point was found in prev and new set
                            && err[i] < (1.0 / density) //with a higher density be more sensitive to the feature error
                            && upNextPoints[i].y >= 0 && upNextPoints[i].x >= 0 //check bounds
                            && upNextPoints[i].y < nextGrey.rows / scaleFactor && upNextPoints[i].x < nextGrey.cols / scaleFactor //check bounds
                            ) {
                        float len = hypot(fabs(upPrevPoints[i].x - upNextPoints[i].x), fabs(upPrevPoints[i].y - upNextPoints[i].y));
                        //upper and lower bound of the flow vector lengthss
                        if (len > 0 && len < sqrt(area)) {
                            //collect new points
                            newPoints.push_back(nextPoints[i]);
                            //the actual drawing operations
                            moveTo(upNextPoints[i].x, upNextPoints[i].y);
                            lineTo(upPrevPoints[i].x, upPrevPoints[i].y);
                        }
                    }
                }
                //end drawing
                stroke();
            }
            prevPoints = newPoints;
        }
    }
}

//Bloom post-processing effect
static void bloom(const cv::UMat& src, cv::UMat &dst, int ksize = 3, int threshValue = 235, float gain = 4) {
	thread_local cv::UMat bgr;
	thread_local cv::UMat hls;
	thread_local cv::UMat ls16;
	thread_local cv::UMat ls;
	thread_local cv::UMat blur;
	thread_local std::vector<cv::UMat> hlsChannels;

    //remove alpha channel
    cv::cvtColor(src, bgr, cv::COLOR_BGRA2RGB);
    //convert to hls
    cv::cvtColor(bgr, hls, cv::COLOR_BGR2HLS);
    //split channels
    cv::split(hls, hlsChannels);
    //invert lightness
    cv::bitwise_not(hlsChannels[2], hlsChannels[2]);
    //multiply lightness and saturation
    cv::multiply(hlsChannels[1], hlsChannels[2], ls16, 1, CV_16U);
    //normalize
    cv::divide(ls16, cv::Scalar(255.0), ls, 1, CV_8U);
    //binary threhold according to threshValue
    cv::threshold(ls, blur, threshValue, 255, cv::THRESH_BINARY);
    //blur
    cv::boxFilter(blur, blur, -1, cv::Size(ksize, ksize), cv::Point(-1,-1), true, cv::BORDER_REPLICATE);
    //convert to BGRA
    cv::cvtColor(blur, blur, cv::COLOR_GRAY2BGRA);
    //add src and the blurred L-S-product according to gain
    addWeighted(src, 1.0, blur, gain, 0, dst);
}

//Glow post-processing effect
static void glow_effect(const cv::UMat &src, cv::UMat &dst, const int ksize) {
	thread_local cv::UMat resize;
	thread_local cv::UMat blur;
	thread_local cv::UMat dst16;

    cv::bitwise_not(src, dst);

    //Resize for some extra performance
    cv::resize(dst, resize, cv::Size(), 0.5, 0.5);
    //Cheap blur
    cv::boxFilter(resize, resize, -1, cv::Size(ksize, ksize), cv::Point(-1,-1), true, cv::BORDER_REPLICATE);
    //Back to original size
    cv::resize(resize, blur, src.size());

    //Multiply the src image with a blurred version of itself
    cv::multiply(dst, blur, dst16, 1, CV_16U);
    //Normalize and convert back to CV_8U
    cv::divide(dst16, cv::Scalar::all(255.0), dst, 1, CV_8U);

    cv::bitwise_not(dst, dst);
}

//Compose the different layers into the final image
static void composite_layers(cv::UMat& background, cv::UMat& foreground, const cv::UMat& frameBuffer, cv::UMat& dst, int kernelSize, float fgLossPercent, BackgroundModes bgMode, PostProcModes ppMode) {
    thread_local cv::UMat tmp;
    thread_local cv::UMat post;
    thread_local cv::UMat backgroundGrey;
    thread_local vector<cv::UMat> channels;

    //Lose a bit of foreground brightness based on fgLossPercent
    cv::subtract(foreground, cv::Scalar::all(255.0f * (fgLossPercent / 100.0f)), foreground);
    //Add foreground an the current framebuffer into foregound
    cv::add(foreground, frameBuffer, foreground);

    //Dependin on bgMode prepare the background in different ways
    switch (bgMode) {
    case GREY:
        cv::cvtColor(background, backgroundGrey, cv::COLOR_BGRA2GRAY);
        cv::cvtColor(backgroundGrey, background, cv::COLOR_GRAY2BGRA);
        break;
    case VALUE:
        cv::cvtColor(background, tmp, cv::COLOR_BGRA2BGR);
        cv::cvtColor(tmp, tmp, cv::COLOR_BGR2HSV);
        split(tmp, channels);
        cv::cvtColor(channels[2], background, cv::COLOR_GRAY2BGRA);
        break;
    case COLOR:
        break;
    case BLACK:
        background = cv::Scalar::all(0);
        break;
    default:
        break;
    }

    //Depending on ppMode perform post-processing
    switch (ppMode) {
    case GLOW:
        glow_effect(foreground, post, kernelSize);
        break;
    case BLOOM:
        bloom(foreground, post, kernelSize, bloom_thresh, bloom_gain);
        break;
    case DISABLED:
        foreground.copyTo(post);
        break;
    default:
        break;
    }

    //Add background and post-processed foreground into dst
    cv::add(background, post, dst);
}

using namespace cv::v4d;

//Build the GUI
static void setup_gui(cv::Ptr<V4D> main) {
    main->imgui([main](ImGuiContext* ctx){
        using namespace ImGui;
        SetCurrentContext(ctx);

        Begin("Effects");
        Text("Foreground");
        SliderFloat("Scale", &fg_scale, 0.1f, 4.0f);
        SliderFloat("Loss", &fg_loss, 0.1f, 99.9f);
        Text("Background");
        thread_local const char* bgm_items[4] = {"Grey", "Color", "Value", "Black"};
        thread_local int* bgm = (int*)&background_mode;
        ListBox("Mode", bgm, bgm_items, 4, 4);
        Text("Points");
        SliderInt("Max. Points", &max_points, 10, 1000000);
        SliderFloat("Point Loss", &point_loss, 0.0f, 100.0f);
        Text("Optical flow");
        SliderInt("Max. Stroke Size", &max_stroke, 1, 100);
        ColorPicker4("Color", effect_color);
        End();

        Begin("Post Processing");
        thread_local const char* ppm_items[3] = {"Glow", "Bloom", "None"};
        thread_local int* ppm = (int*)&post_proc_mode;
        ListBox("Effect",ppm, ppm_items, 3, 3);
        SliderInt("Kernel Size",&glow_kernel_size, 1, 63);
        SliderFloat("Gain", &bloom_gain, 0.1f, 20.0f);
        End();

        Begin("Settings");
        Text("Scene Change Detection");
        SliderFloat("Threshold", &scene_change_thresh, 0.1f, 1.0f);
        SliderFloat("Threshold Diff", &scene_change_thresh_diff, 0.1f, 1.0f);
        End();

		Begin("Window");
		if(Checkbox("Show FPS", &show_fps)) {
			main->setShowFPS(show_fps);
		}
		if(Checkbox("Stretch", &stretch)) {
			main->setStretching(stretch);
		}
#ifndef __EMSCRIPTEN__
		if(Button("Fullscreen")) {
			main->setFullscreen(!main->isFullscreen());
		};

		if(Button("Offscreen")) {
			main->setVisible(!main->isVisible());
		};
#endif
		End();
    });
}

class OptflowPlan : public Plan {
	//BGRA
	cv::UMat background, down;
	//BGR
	cv::UMat result;
	cv::UMat foreground = cv::UMat(cv::Size(WIDTH, HEIGHT), CV_8UC4, cv::Scalar::all(0));
	//GREY
	cv::UMat downPrevGrey, downNextGrey, downMotionMaskGrey;
	vector<cv::Point2f> detectedPoints;
public:
	virtual ~OptflowPlan() override {};
	virtual void infere(cv::Ptr<V4D> window) override {
		auto always = [](){ return true; };

		window->graph(always);
		{
			window->capture([](const cv::UMat& videoFrame, cv::UMat& d, cv::UMat& b) {
				//resize to foreground scale
				cv::resize(videoFrame, d, cv::Size(videoFrame.size().width * fg_scale, videoFrame.size().height * fg_scale));
				//save video background
				videoFrame.copyTo(b);
			}, down, background);

			window->parallel([](const cv::UMat& d, cv::UMat& dng, cv::UMat& dmmg, std::vector<cv::Point2f>& dp){
				cv::cvtColor(d, dng, cv::COLOR_RGBA2GRAY);
				//Subtract the background to create a motion mask
				prepare_motion_mask(dng, dmmg);
				//Detect trackable points in the motion mask
				detect_points(dmmg, dp);
			}, down, downNextGrey, downMotionMaskGrey, detectedPoints);

			window->nvg([](const cv::UMat& dmmg, const cv::UMat& dpg, const cv::UMat& dng, const std::vector<cv::Point2f>& dp) {
				cv::v4d::nvg::clear();
				if (!dpg.empty()) {
					//We don't want the algorithm to get out of hand when there is a scene change, so we suppress it when we detect one.
					if (!detect_scene_change(dmmg, scene_change_thresh, scene_change_thresh_diff)) {
						//Visualize the sparse optical flow using nanovg
						cv::Scalar color = cv::Scalar(effect_color[2] * 255, effect_color[1] * 255, effect_color[0] * 255, effect_color[3] * 255);
						visualize_sparse_optical_flow(dpg, dng, dp, fg_scale, max_stroke, color, max_points, point_loss);
					}
				}
			}, downMotionMaskGrey, downPrevGrey, downNextGrey, detectedPoints);

			window->parallel([](cv::UMat& dpg, const cv::UMat& dng) {
				dpg = dng.clone();
			}, downPrevGrey, downNextGrey);

			window->fb([](cv::UMat& framebuffer, cv::UMat& b, cv::UMat& f, cv::UMat& r) {
				//Put it all together (OpenCL)
				composite_layers(b, f, framebuffer, framebuffer, glow_kernel_size, fg_loss, background_mode, post_proc_mode);
				framebuffer.copyTo(r);
			}, background, foreground, result);

			window->write([](cv::UMat& videoFrame, cv::UMat& r) {
				r.copyTo(videoFrame);
			}, result);
		}
		window->endgraph(always);
	}
};

int main(int argc, char **argv) {
    CV_UNUSED(argc);
    CV_UNUSED(argv);

#ifndef __EMSCRIPTEN__
    if (argc != 2) {
        std::cerr << "Usage: optflow <input-video-file>" << endl;
        exit(1);
    }
#endif

    try {
        using namespace cv::v4d;
        cv::Ptr<V4D> window = V4D::make(WIDTH, HEIGHT, "Sparse Optical Flow Demo", ALL, OFFSCREEN);
        window->printSystemInfo();
        window->setStretching(stretch);
        if (!OFFSCREEN) {
#ifndef __EMSCRIPTEN__
            setup_gui(window);
#else
            setup_gui(window);
#endif
        }

#ifndef __EMSCRIPTEN__
        auto src = makeCaptureSource(window, argv[1]);
        window->setSource(src);
        auto sink = makeWriterSink(window, OUTPUT_FILENAME, src->fps(), cv::Size(WIDTH, HEIGHT));
        window->setSink(sink);
#else
        cv::Ptr<Source> src = makeCaptureSource(WIDTH, HEIGHT, window);
        window->setSource(src);
#endif

        window->run<OptflowPlan>(0);
    } catch (std::exception& ex) {
        cerr << ex.what() << endl;
    }
    return 0;
}
