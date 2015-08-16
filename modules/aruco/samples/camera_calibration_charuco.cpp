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


#include <opencv2/highgui.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/aruco/charuco.hpp>
#include <vector>
#include <iostream>
#include <ctime>

using namespace std;
using namespace cv;


/**
 */
static void help() {
    cout << "Calibration using a ChArUco board" << endl;
    cout << "How to Use:" << endl;
    cout << "To capture a frame for calibration, press 'c'," << endl;
    cout << "If input comes from video, press any key for next frame" << endl;
    cout << "To finish capturing, press 'ESC' key and calibration starts." << endl;
    cout << "Parameters: " << endl;
    cout << "-w <nmarkers> # Number of markers in X direction" << endl;
    cout << "-h <nsquares> # Number of squares in Y direction" << endl;
    cout << "-sl <squareLength> # Square side lenght (in meters)" << endl;
    cout << "-ml <markerLength> # Marker side lenght (in meters)" << endl;
    cout << "-d <dictionary> # 0: ARUCO, ..." << endl;
    cout << "-o <outputFile> # Output file with calibrated camera parameters" << endl;
    cout << "[-v <videoFile>] # Input from video file, if ommited, input comes from camera"
                 << endl;
    cout << "[-ci <int>] # Camera id if input doesnt come from video (-v). Default is 0"
                 << endl;
    cout << "[-dp <detectorParams>] # File of marker detector parameters" << endl;
    cout << "[-rs] # Apply refind strategy" << endl;
    cout << "[-zt] # Assume zero tangential distortion" << endl;
    cout << "[-a <aspectRatio>] # Fix aspect ratio (fx/fy)" << endl;
    cout << "[-p] # Fix the principal point at the center" << endl;
    cout << "[-sc] # Show detected chessboard corners after calibration" << endl;
}


/**
 */
static bool isParam(string param, int argc, char **argv ) {
    for (int i=0; i<argc; i++)
        if (string(argv[i]) == param )
            return true;
    return false;

}


/**
 */
static string getParam(string param, int argc, char **argv, string defvalue = "") {
    int idx=-1;
    for (int i=0; i<argc && idx==-1; i++)
        if (string(argv[i]) == param)
            idx = i;
    if (idx == -1 || (idx + 1) >= argc)
        return defvalue;
    else
        return argv[idx+1];
}




/**
 */
static void readDetectorParameters(string filename, aruco::DetectorParameters &params) {
    FileStorage fs(filename, FileStorage::READ);
    fs["adaptiveThreshWinSizeMin"] >> params.adaptiveThreshWinSizeMin;
    fs["adaptiveThreshWinSizeMax"] >> params.adaptiveThreshWinSizeMax;
    fs["adaptiveThreshWinSizeStep"] >> params.adaptiveThreshWinSizeStep;
    fs["adaptiveThreshConstant"] >> params.adaptiveThreshConstant;
    fs["minMarkerPerimeterRate"] >> params.minMarkerPerimeterRate;
    fs["maxMarkerPerimeterRate"] >> params.maxMarkerPerimeterRate;
    fs["polygonalApproxAccuracyRate"] >> params.polygonalApproxAccuracyRate;
    fs["minCornerDistance"] >> params.minCornerDistance;
    fs["minDistanceToBorder"] >> params.minDistanceToBorder;
    fs["minMarkerDistance"] >> params.minMarkerDistance;
    fs["doCornerRefinement"] >> params.doCornerRefinement;
    fs["cornerRefinementWinSize"] >> params.cornerRefinementWinSize;
    fs["cornerRefinementMaxIterations"] >> params.cornerRefinementMaxIterations;
    fs["cornerRefinementMinAccuracy"] >> params.cornerRefinementMinAccuracy;
    fs["markerBorderBits"] >> params.markerBorderBits;
    fs["perspectiveRemoveDistortion"] >> params.perspectiveRemoveDistortion;
    fs["perspectiveRemovePixelPerCell"] >> params.perspectiveRemovePixelPerCell;
    fs["perspectiveRemoveIgnoredMarginPerCell"] >> params.perspectiveRemoveIgnoredMarginPerCell;
    fs["maxErroneousBitsInBorderRate"] >> params.maxErroneousBitsInBorderRate;
    fs["minOtsuStdDev"] >> params.minOtsuStdDev;
    fs["errorCorrectionRate"] >> params.errorCorrectionRate;
}



/**
 */
static void saveCameraParams(const string& filename,
                             Size imageSize, float aspectRatio, int flags,
                             const Mat& cameraMatrix, const Mat& distCoeffs,
                             double totalAvgErr ) {
    FileStorage fs( filename, FileStorage::WRITE );

    time_t tt;
    time( &tt );
    struct tm *t2 = localtime( &tt );
    char buf[1024];
    strftime( buf, sizeof(buf)-1, "%c", t2 );

    fs << "calibration_time" << buf;

    fs << "image_width" << imageSize.width;
    fs << "image_height" << imageSize.height;

    if ( flags & CALIB_FIX_ASPECT_RATIO )
        fs << "aspectRatio" << aspectRatio;

    if ( flags != 0 ) {
        sprintf( buf, "flags: %s%s%s%s",
        flags & CALIB_USE_INTRINSIC_GUESS ? "+use_intrinsic_guess" : "",
        flags & CALIB_FIX_ASPECT_RATIO ? "+fix_aspectRatio" : "",
        flags & CALIB_FIX_PRINCIPAL_POINT ? "+fix_principal_point" : "",
        flags & CALIB_ZERO_TANGENT_DIST ? "+zero_tangent_dist" : "" );
    }

    fs << "flags" << flags;

    fs << "camera_matrix" << cameraMatrix;
    fs << "distortion_coefficients" << distCoeffs;

    fs << "avg_reprojection_error" << totalAvgErr;
}



/**
 */
int main(int argc, char *argv[]) {

    if (!isParam("-w", argc, argv) || !isParam("-h", argc, argv) || !isParam("-sl", argc, argv) ||
        !isParam("-ml", argc, argv) || !isParam("-d", argc, argv) || !isParam("-o", argc, argv) ) {
        help();
        return 0;
    }

    int squaresX = atoi( getParam("-w", argc, argv).c_str() );
    int squaresY = atoi( getParam("-h", argc, argv).c_str() );
    float squareLength = (float)atof( getParam("-sl", argc, argv).c_str() );
    float markerLength = (float)atof( getParam("-ml", argc, argv).c_str() );
    int dictionaryId = atoi( getParam("-d", argc, argv).c_str() );
    aruco::Dictionary dictionary = aruco::getPredefinedDictionary(
                                   aruco::PREDEFINED_DICTIONARY_NAME(dictionaryId));
    string outputFile = getParam("-o", argc, argv);

    bool showChessboardCorners = isParam("-sc", argc, argv);

    int calibrationFlags = 0;
    float aspectRatio = 1;
    if (isParam("-a", argc, argv)) {
        calibrationFlags |= CALIB_FIX_ASPECT_RATIO;
        aspectRatio = (float)atof( getParam("-a", argc, argv).c_str() );
    }
    if (isParam("-zt", argc, argv))
        calibrationFlags |= CALIB_ZERO_TANGENT_DIST;
    if (isParam("-p", argc, argv))
        calibrationFlags |= CALIB_FIX_PRINCIPAL_POINT;

    aruco::DetectorParameters detectorParams;
    if (isParam("-dp", argc, argv)) {
      readDetectorParameters(getParam("-dp", argc, argv), detectorParams);
    }
    detectorParams.doCornerRefinement=false; // no corner refinement in markers

    bool refindStrategy = false;
    if (isParam("-rs", argc, argv))
        refindStrategy = true;

    VideoCapture inputVideo;
    int waitTime;
    if (isParam("-v", argc, argv)) {
        inputVideo.open(getParam("-v", argc, argv));
        waitTime = 0;
    }
    else {
        int camId = 0;
        if (isParam("-ci", argc, argv))
            camId = atoi( getParam("-ci", argc, argv).c_str() );
        inputVideo.open(camId);
        waitTime = 10;
    }

    aruco::CharucoBoard board = aruco::CharucoBoard::create(squaresX, squaresY,
                                                                    squareLength, markerLength,
                                                                    dictionary);

    vector<vector<vector<Point2f> > > allCorners;
    vector<vector<int> > allIds;
    vector<Mat> allImgs;
    Size imgSize;

    while (inputVideo.grab()) {
        Mat image, imageCopy;
        inputVideo.retrieve(image);

        vector<int> ids;
        vector<vector<Point2f> > corners, rejected;

        // detect markers and estimate pose
        aruco::detectMarkers(image, dictionary, corners, ids, detectorParams, rejected);

        // refind strategy to detect more markers
        if (refindStrategy)
            aruco::refineDetectedMarkers(image, board, corners, ids, rejected);

        Mat currentCharucoCorners, currentCharucoIds;
        if (ids.size() > 0)
            aruco::interpolateCornersCharuco(corners, ids, image, board, currentCharucoCorners,
                                                 currentCharucoIds);

        // draw results
        image.copyTo(imageCopy);
        if (ids.size() > 0)
            aruco::drawDetectedMarkers(imageCopy, imageCopy, corners);

        if (currentCharucoCorners.total() > 0) {
            aruco::drawDetectedCornersCharuco(imageCopy, imageCopy, currentCharucoCorners,
                                                  currentCharucoIds);
        }

        imshow("out", imageCopy);
        char key = (char) waitKey(waitTime);
        if (key == 27)
            break;
        if (key == 'c' && ids.size() > 0) {
            cout << "Frame captured" << endl;
            allCorners.push_back(corners);
            allIds.push_back(ids);
            allImgs.push_back(image);
            imgSize = image.size();
        }
    }

    Mat cameraMatrix, distCoeffs;
    vector<Mat> rvecs, tvecs;
    double repError;

    if( calibrationFlags & CALIB_FIX_ASPECT_RATIO ) {
        cameraMatrix = Mat::eye(3, 3, CV_64F);
        cameraMatrix.at<double>(0,0) = aspectRatio;
    }


    vector<vector<Point2f> > allCornersConcatenated;
    vector<int> allIdsConcatenated;
    vector<int> markerCounterPerFrame;
    markerCounterPerFrame.reserve(allCorners.size());
    for(unsigned int i=0; i<allCorners.size(); i++) {
        markerCounterPerFrame.push_back((int)allCorners[i].size());
        for(unsigned int j=0; j<allCorners[i].size(); j++) {
            allCornersConcatenated.push_back(allCorners[i][j]);
            allIdsConcatenated.push_back(allIds[i][j]);
        }
    }
    double arucoRepErr;
    arucoRepErr = aruco::calibrateCameraAruco(allCornersConcatenated, allIdsConcatenated,
                                                  markerCounterPerFrame, board, imgSize,
                                                  cameraMatrix, distCoeffs, noArray(),
                                                  noArray(), calibrationFlags);

    int nFrames = (int)allCorners.size();
    vector<Mat> allCharucoCorners;
    vector<Mat> allCharucoIds;
    vector<Mat> filteredImages;
    allCharucoCorners.reserve(nFrames);
    allCharucoIds.reserve(nFrames);

    for (int i=0; i<nFrames; i++) {
        Mat currentCharucoCorners, currentCharucoIds;
        aruco::interpolateCornersCharuco(allCorners[i], allIds[i], allImgs[i], board,
                                             currentCharucoCorners, currentCharucoIds,
                                             cameraMatrix, distCoeffs);
        bool validPose;
        Mat currentRvec, currentTvec;
        validPose = aruco::estimatePoseCharucoBoard(currentCharucoCorners, currentCharucoIds,
                                                        board, cameraMatrix, distCoeffs,
                                                        currentRvec, currentTvec);
        if(validPose) {
            allCharucoCorners.push_back(currentCharucoCorners);
            allCharucoIds.push_back(currentCharucoIds);
            filteredImages.push_back(allImgs[i]);
        }
    }

    repError = aruco::calibrateCameraCharuco(allCharucoCorners, allCharucoIds, board, imgSize,
                                                 cameraMatrix, distCoeffs, rvecs, tvecs,
                                                 calibrationFlags);

    saveCameraParams(outputFile, imgSize, aspectRatio, calibrationFlags, cameraMatrix, distCoeffs,
                     repError );

    cout << "Rep Error: " << repError << endl;
    cout << "Rep Error Aruco: " << arucoRepErr << endl;
    cout << "Calibration saved to " << outputFile << endl;

    if (showChessboardCorners) {
        for (unsigned int frame = 0; frame < filteredImages.size(); frame++) {
            Mat imageCopy = filteredImages[frame].clone();
            if (allIds[frame].size() > 0) {

                if (allCharucoCorners[frame].total() > 0) {
                    aruco::drawDetectedCornersCharuco(imageCopy, imageCopy,
                                                          allCharucoCorners[frame],
                                                          allCharucoIds[frame]);
                }
            }

            imshow("out", imageCopy);
            char key = (char) waitKey(0);
            if (key == 27)
                break;
        }
    }

    return 0;
}
