/*
By downloading, copying, installing or using the software you agree to this
license. If you do not agree to this license, do not download, install,
copy or use the software.

                          License Agreement
               For Open Source Computer Vision Library
                       (3-clause BSD License)

Copyright (C) 2013, OpenCV Foundation, all rights reserved.
Third party copyrights are property of their respective owners.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

  * Neither the names of the copyright holders nor the names of the contributors
    may be used to endorse or promote products derived from this software
    without specific prior written permission.

This software is provided by the copyright holders and contributors "as is" and
any express or implied warranties, including, but not limited to, the implied
warranties of merchantability and fitness for a particular purpose are
disclaimed. In no event shall copyright holders or contributors be liable for
any direct, indirect, incidental, special, exemplary, or consequential damages
(including, but not limited to, procurement of substitute goods or services;
loss of use, data, or profits; or business interruption) however caused
and on any theory of liability, whether in contract, strict liability,
or tort (including negligence or otherwise) arising in any way out of
the use of this software, even if advised of the possibility of such damage.
*/

#include <opencv2/opencv_modules.hpp>

#include <opencv2/highgui.hpp>
#include <opencv2/mcc.hpp>
#include <opencv2/dnn.hpp>
#include <iostream>


using namespace std;
using namespace cv;
using namespace mcc;

const char *about = "Basic chart detection";
const char *keys = {
    "{t              |         | chartType: 0-Standard, 1-DigitalSG, 2-Vinyl}"
    "{v        |       | Input from video file, if ommited, input comes from camera }"
    // "{i        |       | File path of image  }"
    "{m        |       | File path of model  }"
    "{pb        |       | File path of pbtxt file  }"
    "{ci       | 0     | Camera id if input doesnt come from video (-v) }"};

int main(int argc, char *argv[])
{
    CommandLineParser parser(argc, argv, keys);
    parser.about(about);

    if (argc < 2)
    {
        parser.printMessage();
        return 0;
    }

    int chartType = parser.get<int>("t");

    // string imagepath = parser.get<string> ("i");
    string model_path = parser.get<string> ("m");
    string pbtxt_path = parser.get<string> ("pb");
    int camId = parser.get<int>("ci");

    String video;
    if (parser.has("v"))
    {
        video = parser.get<String>("v");
    }

    if (!parser.check())
    {
        parser.printErrors();
        return 0;
    }

    VideoCapture inputVideo;
    int waitTime;
    if (!video.empty())
    {
        inputVideo.open(video);
        waitTime = 0;
    }
    else
    {
        inputVideo.open(camId);
        waitTime = 10;
    }

    //load the network

	cv::dnn::Net net = cv::dnn::readNetFromTensorflow(model_path, pbtxt_path);

    while (inputVideo.grab())
    {
        Mat image, imageCopy;
        inputVideo.retrieve(image);

        // image = imread(imagepath);

        imageCopy=image.clone();
        Ptr<CCheckerDetector> detector = CCheckerDetector::create(2, 5);
        if(!detector->setNet(net))
        {
            cout<<"Loading Model failed: Falling back to standard techniques"<<endl;
        }
        Ptr<CChecker> det = CChecker::create();
        cv::Rect region = Rect(Point2f(0,0), image.size());
        vector<cv::Rect> regions(1, region);
        // Marker type to detect
        if (!detector->process(image, chartType, regions, true ))
        {
            printf("ChartColor not detected \n");
        }
        else
        {

            // get checker
            std::vector<Ptr<mcc::CChecker>> checkers;
            detector->getListColorChecker(checkers);
            Ptr<mcc::CChecker> checker;

            for (size_t ck = 0; ck < checkers.size(); ck++)
            {
                // current checker
                checker = checkers[ck];

                Ptr<CCheckerDraw> cdraw = CCheckerDraw::create(checker);
                cdraw->draw(image, chartType);
            }

        }

         imshow("image result | q or esc to quit", image);
        imshow("original", imageCopy);
        char key = (char)waitKey(waitTime);
        if (key == 27)
            break;
    }

    return 0;
}
