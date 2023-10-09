#include <opencv2/v4d/v4d.hpp>

using namespace cv;
using namespace cv::v4d;

int main() {
    Ptr<V4D> window = V4D::make(960, 960, "Font Rendering with GUI");

	//The font size
	float size = 40.0f;
	//The text hue
	std::vector<float> color = {1.0f, 0.0f, 0.0f};
	//The text
	string hw = "hello world";
	//Setup the GUI
	window->imgui([&](ImGuiContext* ctx) {
	    using namespace ImGui;
	    SetCurrentContext(ctx);
	    Begin("Settings");
	    SliderFloat("Font Size", &size, 1.0f, 100.0f);
		ColorPicker3("Text Color", color.data());
		End();
	});

	window->run([&](Ptr<V4D> win) {
		//Render the text at the center of the screen using parameters from the GUI.
		win->nvg([](const Size& sz, const string& str, const float& s, const std::vector<float>& c) {
			using namespace cv::v4d::nvg;
			clear();
			fontSize(s);
			fontFace("sans-bold");
			fillColor(Scalar(c[2] * 255, c[1] * 255, c[0] * 255, 255));
			textAlign(NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
			text(sz.width / 2.0, sz.height / 2.0, str.c_str(), str.c_str() + str.size());
		}, win->fbSize(), hw, size, color);

		return win->display();
	});
}

