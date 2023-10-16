// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.
// Copyright Amir Hassan (kallaballa) <amir@viel-zu.org>

#include <opencv2/v4d/v4d.hpp>
//adapted from https://gitlab.com/wikibooks-opengl/modern-tutorials/-/blob/master/tut05_cube/cube.cpp

/* Demo Parameters */

#ifndef __EMSCRIPTEN__
constexpr long unsigned int WIDTH = 1280;
constexpr long unsigned int HEIGHT = 720;
#else
constexpr long unsigned int WIDTH = 960;
constexpr long unsigned int HEIGHT = 960;
#endif
constexpr bool OFFSCREEN = false;
#ifndef __EMSCRIPTEN__
constexpr double FPS = 60;
constexpr const char* OUTPUT_FILENAME = "many_cubes-demo.mkv";
#endif
const unsigned long DIAG = hypot(double(WIDTH), double(HEIGHT));
const int glow_kernel_size = std::max(int(DIAG / 138 % 2 == 0 ? DIAG / 138 + 1 : DIAG / 138), 1);

using std::cerr;
using std::endl;

/* OpenGL constants */
const GLuint triangles = 12;
const GLuint vertices_index = 0;
const GLuint colors_index = 1;

//Cube vertices, colors and indices
const float vertices[] = {
		// Front face
        0.5, 0.5, 0.5, -0.5, 0.5, 0.5, -0.5, -0.5, 0.5, 0.5, -0.5, 0.5,
        // Back face
        0.5, 0.5, -0.5, -0.5, 0.5, -0.5, -0.5, -0.5, -0.5, 0.5, -0.5, -0.5
};

const float vertex_colors[] = {
		1.0, 0.4, 0.6, 1.0, 0.9, 0.2, 0.7, 0.3, 0.8, 0.5, 0.3, 1.0,
		0.2, 0.6, 1.0, 0.6, 1.0, 0.4, 0.6, 0.8, 0.8, 0.4, 0.8, 0.8
};

const unsigned short triangle_indices[] = {
		// Front
        0, 1, 2, 2, 3, 0,

        // Right
        0, 3, 7, 7, 4, 0,

        // Bottom
        2, 6, 7, 7, 3, 2,

        // Left
        1, 5, 6, 6, 2, 1,

        // Back
        4, 7, 6, 6, 5, 4,

        // Top
        5, 1, 0, 0, 4, 5
};
//Simple transform & pass-through shaders
static GLuint load_shader() {
	//Shader versions "330" and "300 es" are very similar.
	//If you are careful you can write the same code for both versions.
#if !defined(__EMSCRIPTEN__) && !defined(OPENCV_V4D_USE_ES3)
    const string shaderVersion = "330";
#else
    const string shaderVersion = "300 es";
#endif

    const string vert =
            "    #version " + shaderVersion
                    + R"(
    precision lowp float;
    layout(location = 0) in vec3 pos;
    layout(location = 1) in vec3 vertex_color;
    
    uniform mat4 transform;
    
    out vec3 color;
    void main() {
      gl_Position = transform * vec4(pos, 1.0);
      color = vertex_color;
    }
)";

    const string frag =
            "    #version " + shaderVersion
                    + R"(
    precision lowp float;
    in vec3 color;
    
    out vec4 frag_color;
    
    void main() {
      frag_color = vec4(color, 1.0);
    }
)";

    //Initialize the shaders and returns the program
    return cv::v4d::initShader(vert.c_str(), frag.c_str(), "fragColor");
}

//Initializes objects, buffers, shaders and uniforms
static void init_scene(const cv::Size& sz, GLuint& vao, GLuint& shaderProgram, GLuint& uniformTransform) {
    glEnable (GL_DEPTH_TEST);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    unsigned int triangles_ebo;
    glGenBuffers(1, &triangles_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, triangles_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof triangle_indices, triangle_indices,
            GL_STATIC_DRAW);

    unsigned int verticies_vbo;
    glGenBuffers(1, &verticies_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, verticies_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof vertices, vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(vertices_index, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(vertices_index);

    unsigned int colors_vbo;
    glGenBuffers(1, &colors_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, colors_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof vertex_colors, vertex_colors, GL_STATIC_DRAW);

    glVertexAttribPointer(colors_index, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(colors_index);

    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    shaderProgram = load_shader();
    uniformTransform = glGetUniformLocation(shaderProgram, "transform");
    glViewport(0,0, sz.width, sz.height);
}

//Renders a rotating rainbow-colored cube on a blueish background
static void render_scene(GLuint &vao, GLuint &shaderProgram,
		GLuint &uniformTransform) {
	//Clear the background
	glClearColor(0.2, 0.24, 0.4, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	//Use the prepared shader program
	glUseProgram(shaderProgram);

	//Scale and rotate the cube depending on the current time.
	float angle = fmod(
			double(cv::getTickCount()) / double(cv::getTickFrequency()),
			2 * M_PI);
	float scale = 0.25;

	cv::Matx44f scaleMat(scale, 0.0, 0.0, 0.0, 0.0, scale, 0.0, 0.0, 0.0, 0.0,
			scale, 0.0, 0.0, 0.0, 0.0, 1.0);

	cv::Matx44f rotXMat(1.0, 0.0, 0.0, 0.0, 0.0, cos(angle), -sin(angle), 0.0,
			0.0, sin(angle), cos(angle), 0.0, 0.0, 0.0, 0.0, 1.0);

	cv::Matx44f rotYMat(cos(angle), 0.0, sin(angle), 0.0, 0.0, 1.0, 0.0, 0.0,
			-sin(angle), 0.0, cos(angle), 0.0, 0.0, 0.0, 0.0, 1.0);

	cv::Matx44f rotZMat(cos(angle), -sin(angle), 0.0, 0.0, sin(angle),
			cos(angle), 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0);

	//calculate the transform
	cv::Matx44f transform = scaleMat * rotXMat * rotYMat * rotZMat;
	//set the corresponding uniform
	glUniformMatrix4fv(uniformTransform, 1, GL_FALSE, transform.val);
	//Bind the prepared vertex array object
	glBindVertexArray(vao);
	//Draw
	glDrawElements(GL_TRIANGLES, triangles * 3, GL_UNSIGNED_SHORT, NULL);
}

//applies a glow effect to an image
static void glow_effect(const cv::UMat& src, cv::UMat& dst, const int ksize) {
    thread_local cv::UMat resize;
    thread_local cv::UMat blur;
    thread_local cv::UMat dst16;

    cv::bitwise_not(src, dst);

    //Resize for some extra performance
    cv::resize(dst, resize, cv::Size(), 0.5, 0.5);
    //Cheap blur
    cv::boxFilter(resize, resize, -1, cv::Size(ksize, ksize), cv::Point(-1, -1), true,
            cv::BORDER_REPLICATE);
    //Back to original size
    cv::resize(resize, blur, src.size());

    //Multiply the src image with a blurred version of itself
    cv::multiply(dst, blur, dst16, 1, CV_16U);
    //Normalize and convert back to CV_8U
    cv::divide(dst16, cv::Scalar::all(255.0), dst, 1, CV_8U);

    cv::bitwise_not(dst, dst);
}

using namespace cv::v4d;
class CubeDemoPlan : public Plan {
	cv::UMat frame_;
	GLuint vao_;
	GLuint shaderProgram_;
	GLuint uniformTransform_;
public:
	void setup(cv::Ptr<V4D> window) {
		window->gl([](const cv::Size& sz, GLuint& v, GLuint& sp, GLuint& ut){
			init_scene(sz, v, sp, ut);
		}, window->fbSize(), vao_, shaderProgram_, uniformTransform_);
	}
	void infer(cv::Ptr<V4D> window) {
		window->gl([](){
			//Clear the background
			glClearColor(0.2f, 0.24f, 0.4f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);
		});

		//Render using multiple OpenGL contexts
		window->gl([](GLuint& v, GLuint& sp, GLuint& ut){
			render_scene(v, sp, ut);
		}, vao_, shaderProgram_, uniformTransform_);

		//Aquire the frame buffer for use by OpenCV
		window->fb([](cv::UMat& framebuffer, cv::UMat& f) {
#ifndef __EMSCRIPTEN__
			glow_effect(framebuffer, framebuffer, glow_kernel_size);
#endif
			framebuffer.copyTo(f);
		}, frame_);

		window->write([](cv::UMat& outputFrame, const cv::UMat& f){
			f.copyTo(outputFrame);
		}, frame_);
	}
};

int main() {
    cv::Ptr<V4D> window = V4D::make(WIDTH, HEIGHT, "Cube Demo", ALL, OFFSCREEN);
#ifndef __EMSCRIPTEN__
    //Creates a writer sink (which might be hardware accelerated)
    auto sink = makeWriterSink(window, OUTPUT_FILENAME, FPS, cv::Size(WIDTH, HEIGHT));
    window->setSink(sink);
#endif
    window->run<CubeDemoPlan>(0);

    return 0;
}
