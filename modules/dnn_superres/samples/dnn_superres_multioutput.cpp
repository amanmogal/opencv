#include <iostream>
#include <sstream>
#include <opencv2/dnn_superres.hpp>

using namespace std;
using namespace cv;
using namespace dnn;
using namespace dnn_superres;

int main(int argc, char *argv[])
{
    // Check for valid command line arguments, print usage
    // if insufficient arguments were given.
    if (argc < 4) {
        cout << "usage:   Arg 1: image     | Path to image" << endl;
        cout << "\t Arg 2: scales in a format of 2,4,8\n";
        cout << "\t Arg 3: output node names in a format of nchw_output_0,nchw_output_1\n";
        cout << "\t Arg 4: path to model file \n";
        return -1;
    }

    string img_path = string(argv[1]);
    string scales_str = string(argv[2]);
    string output_names_str = string(argv[3]);
    std::string path = string(argv[4]);

    std::stringstream ss(scales_str);
    std::vector<int> scales;
    std::string token;
    char delim = ',';
    while (std::getline(ss, token, delim)) {
        scales.push_back(atoi(token.c_str()));
    }

    ss = std::stringstream(output_names_str);
    std::vector<String> node_names;
    while (std::getline(ss, token, delim)) {
        node_names.push_back(token);
    }

    // Load the image
    Mat img = cv::imread(img_path);
    Mat original_img(img);
    if (img.empty())
    {
        std::cerr << "Couldn't load image: " << img << "\n";
        return -2;
    }

    //Make dnn super resolution instance
    DnnSuperResImpl sr;
    int scale = *max_element(scales.begin(), scales.end());
    std::vector<Mat> outputs;
    sr.readModel(path);
    sr.setModel("lapsrn", scale);

    sr.upsample_multioutput(img, outputs, scales, node_names);

    for(unsigned int i = 0; i < outputs.size(); i++)
    {
        cv::namedWindow("Upsampled image", WINDOW_AUTOSIZE);
        cv::imshow("Upsampled image", outputs[i]);
        //cv::imwrite("./saved.jpg", img_new);
        cv::waitKey(0);
    }

    return 0;
}