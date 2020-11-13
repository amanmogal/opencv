// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.
//
//
//                       License Agreement
//              For Open Source Computer Vision Library
//
// Copyright(C) 2020, Huawei Technologies Co.,Ltd. All rights reserved.
// Third party copyrights are property of their respective owners.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//             http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: Longbu Wang <wanglongbu@huawei.com.com>
//         Jinheng Zhang <zhangjinheng1@huawei.com>
//         Chenqi Shan <shanchenqi@huawei.com>

#ifndef __OPENCV_MCC_CCM_HPP__
#define __OPENCV_MCC_CCM_HPP__

#include <iostream>
#include <cmath>
#include <string>
#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
// #include "opencv2/mcc/linearize.hpp"

namespace cv
{
namespace ccm
{


/** @brief Enum of the possible types of ccm.
*/
enum CCM_TYPE
{
    CCM_3x3,   ///<The CCM with the shape \f$3\times3\f$ performs linear transformation on color values.
    CCM_4x3,   ///<The CCM with the shape \f$4\times3\f$ performs affine transformation.

};

/** @brief Enum of the possible types of initial method.
*/
enum INITIAL_METHOD_TYPE
{
    WHITE_BALANCE,      ///< The white balance method. The initial value is:
                        /// \f$
                        /// M_{CCM}=
                        /// \begin{bmatrix}
                        /// k_R & 0 & 0\\
                        /// 0 & k_G & 0\\
                        /// 0 & 0 & k_B\\
                        /// \end{bmatrix}
                        /// \f$
                        /// where
                        /// \f$
                        /// k_R=mean(R_{li}')/mean(R_{li})\\
                        /// k_R=mean(G_{li}')/mean(G_{li})\\
                        /// k_R=mean(B_{li}')/mean(B_{li})
                        /// \f$
    LEAST_SQUARE,       ///<the least square method is an optimal solution under the linear RGB distance function
};
 /** @brief  Macbeth and Vinyl ColorChecker with 2deg D50 .
    */
enum CONST_COLOR {
    Macbeth,
    Vinyl,
    DigitalSG
};
enum COLOR_SPACE {
    sRGB,                       ///<https://en.wikipedia.org/wiki/SRGB
    sRGBL,                      ///<https://en.wikipedia.org/wiki/SRGB
    AdobeRGB,                   ///<https://en.wikipedia.org/wiki/Adobe_RGB_color_space
    AdobeRGBL,                  ///<https://en.wikipedia.org/wiki/Adobe_RGB_color_space
    WideGamutRGB,               ///<https://en.wikipedia.org/wiki/Wide-gamut_RGB_color_space
    WideGamutRGBL,              ///<https://en.wikipedia.org/wiki/Wide-gamut_RGB_color_space
    ProPhotoRGB,                ///<https://en.wikipedia.org/wiki/ProPhoto_RGB_color_space
    ProPhotoRGBL,               ///<https://en.wikipedia.org/wiki/ProPhoto_RGB_color_space
    DCI_P3_RGB,                 ///<https://en.wikipedia.org/wiki/DCI-P3
    DCI_P3_RGBL,                ///<https://en.wikipedia.org/wiki/DCI-P3
    AppleRGB,                   ///<https://en.wikipedia.org/wiki/RGB_color_space
    AppleRGBL,                  ///<https://en.wikipedia.org/wiki/RGB_color_space
    REC_709_RGB,                ///<https://en.wikipedia.org/wiki/Rec._709
    REC_709_RGBL,               ///<https://en.wikipedia.org/wiki/Rec._709
    REC_2020_RGB,               ///<https://en.wikipedia.org/wiki/Rec._2020
    REC_2020_RGBL,              ///<https://en.wikipedia.org/wiki/Rec._2020
    XYZ_D65_2,                  ///<https://en.wikipedia.org/wiki/CIE_1931_color_space
    XYZ_D65_10,
    XYZ_D50_2,
    XYZ_D50_10,
    XYZ_A_2,
    XYZ_A_10,
    XYZ_D55_2,
    XYZ_D55_10,
    XYZ_D75_2,
    XYZ_D75_10,
    XYZ_E_2,
    XYZ_E_10,
    Lab_D65_2,                  ///<https://en.wikipedia.org/wiki/CIELAB_color_space
    Lab_D65_10,
    Lab_D50_2,
    Lab_D50_10,
    Lab_A_2,
    Lab_A_10,
    Lab_D55_2,
    Lab_D55_10,
    Lab_D75_2,
    Lab_D75_10,
    Lab_E_2,
    Lab_E_10,
};
/** @brief ## Linearization

The first step in color correction is to linearize the detected colors. Because the input color space has not been calibrated, we usually use some empirical methods to linearize. There are several common  linearization methods. The first is identical transformation, the second is gamma correction, and the third is polynomial fitting.

Linearization is generally an elementwise function. The mathematical symbols are as follows:

\f$C\f$: any channel of a color, could be \f$R, G\f$ or \f$B\f$.

\f$R, G,  B\f$:  \f$R, G, B\f$ channels respectively.

\f$G\f$: grayscale;

\f$s,sl\f$: subscript, which represents the detected data and its linearized value, the former is the input and the latter is the output;

\f$d,dl\f$: subscript, which represents the reference data and its linearized value



### Identical Transformation

No change is made during the Identical transformation linearization, usually because the tristimulus values of the input RGB image is already proportional to the luminance. For example, if the input measurement data is in RAW format, the measurement data is already linear, so no linearization is required.

The identity transformation formula is as follows:

\f[
C_{sl}=C_s
\f]

### Gamma Correction

Gamma correction is a means of performing nonlinearity in RGB space, see the Color Space documentation for details. In the linearization part, the value of \f$\gamma\f$ is usually set to 2.2. You can also customize the value.

The formula for gamma correction linearization is as follows:
\f[
C_{sl}=C_s^{\gamma},\qquad C_s\ge0\\
C_{sl}=-(-C_s)^{\gamma},\qquad C_s<0\\\\
\f]

### Polynomial Fitting

Polynomial fitting uses polynomials to linearize. Provided the polynomial is:
\f[
f(x)=a_nx^n+a_{n-1}x^{n-1}+... +a_0
\f]
Then:
\f[
C_{sl}=f(C_s)
\f]
In practice, \f$n\le3\f$ is used to prevent overfitting.

There are many variants of polynomial fitting, the difference lies in the way of generating $f(x)$. It is usually necessary to use linearized reference colors and corresponding detected colors to calculate the polynomial parameters. However, not all colors can participate in the calculation. The saturation detected colors needs to be removed. See the algorithm introduction document for details.

#### Fitting Channels Respectively

Use three polynomials, \f$r(x), g(x), b(x)\f$,  to linearize each channel of the RGB color space[1-3]:
\f[
R_{sl}=r(R_s)\\
G_{sl}=g(G_s)\\
B_{sl}=b(B_s)\\
\f]
The polynomial is generated by minimizing the residual sum of squares between the detected data and the linearized reference data. Take the R-channel as an example:

\f[
R=\arg min_{f}(\Sigma(R_{dl}-f(R_S)^2)
\f]

It's equivalent to finding the least square regression for below equations:
\f[
f(R_{s1})=R_{dl1}\\
f(R_{s2})=R_{dl2}\\
...
\f]

With a polynomial, the above equations becomes:
\f[
\begin{bmatrix}
R_{s1}^{n} & R_{s1}^{n-1} & ... & 1\\
R_{s2}^{n} & R_{s2}^{n-1} & ... & 1\\
... & ... & ... & ...
\end{bmatrix}
\begin{bmatrix}
a_{n}\\
a_{n-1}\\
... \\
a_0
\end{bmatrix}
=
\begin{bmatrix}
R_{dl1}\\
R_{dl2}\\
...
\end{bmatrix}
\f]
It can be expressed as a system of linear equations:

\f[
AX=B
\f]

When the number of reference colors is  not less than the degree of the polynomial, the linear system has a least-squares solution:

\f[
X=(A^TA)^{-1}A^TB
\f]

Once we get the polynomial coefficients, we can get the polynomial r.

This method of finding polynomial coefficients can be implemented by numpy.polyfit in numpy, expressed here as:

\f[
R=polyfit(R_S, R_{dl})
\f]

Note that, in general, the polynomial that we want to obtain is guaranteed to monotonically increase in the interval [0,1] , but this means that nonlinear method is needed to generate the polynomials(see [4] for detail). This would greatly increases the complexity of the program. Considering that the monotonicity does not affect the correct operation of the color correction program, polyfit is still used to implement the program.

Parameters for other channels can also be derived in a similar way.

#### Grayscale Polynomial Fitting

In this method[2], single polynomial is used for all channels. The polynomial is still a polyfit result from the detected colors to the linear reference colors. However, only the gray of the reference colors can participate in the calculation.

Since the detected colors corresponding to the gray of reference colors is not necessarily gray, it needs to be grayed. Grayscale refers to the Y channel of the XYZ color space. The color space of the detected data is not determined and cannot be converted into the XYZ space. Therefore, the sRGB formula is used to approximate[5].
\f[
G_{s}=0.2126R_{s}+0.7152G_{s}+0.0722B_{s}
\f]
Then the polynomial parameters can be obtained by using the polyfit.
\f[
f=polyfit(G_{s}, G_{dl})
\f]
After \f$f\f$ is obtained, linearization can be performed.

#### Logarithmic Polynomial Fitting

For gamma correction formula, we take the logarithm:
\f[
ln(C_{sl})={\gamma}ln(C_s),\qquad C_s\ge0\
\f]
It can be seen that there is a linear relationship between \f$ln(C_s)\f$ and \f$ln(C_{sl})\f$. It can be considered that the formula is an approximation of a polynomial relationship, that is, there exists a polynomial $f$, which makes[2]:
\f[
ln(C_{sl})=f(ln(C_s)), \qquad C_s>0\\
C_{sl}=0, \qquad C_s=0
\f]

Because \f$exp(ln(0))\to\infin\f$, the channel whose component is 0 is directly mapped to 0 in the formula above.

For fitting channels respectively, we have:
\f[
r=polyfit(ln(R_s),ln(R_{dl}))\\
g=polyfit(ln(G_s),ln(G_{dl}))\\
b=polyfit(ln(B_s),ln(B_{dl}))\\
\f]
Note that the parameter of $ln$ cannot be 0. Therefore, we need to delete the channels whose values are 0 from $R_s$ and $R_{dl}$, $G_s$ and $G_{dl}$, $B_s$ and $B_{dl}$.

Therefore:

\f[
ln(R_{sl})=r(ln(R_s)), \qquad R_s>0\\
R_{sl}=0, \qquad R_s=0\\
ln(G_{sl})=g(ln(G_s)),\qquad G_s>0\\
G_{sl}=0, \qquad G_s=0\\
ln(B_{sl})=b(ln(B_s)),\qquad B_s>0\\
B_{sl}=0, \qquad B_s=0\\
\f]

For grayscale polynomials, there are also:
\f[
f=polyfit(ln(G_{sl}),ln(G_{dl}))
\f]
and:
\f[
ln(C_{sl})=f(ln(C_s)), \qquad C_s>0\\
C_sl=0, \qquad C_s=0
\f]
*/
enum LINEAR_TYPE
{
    IDENTITY_,
    GAMMA,
    COLORPOLYFIT,
    COLORLOGPOLYFIT,
    GRAYPOLYFIT,
    GRAYLOGPOLYFIT
};

/** @brief Enum of possibale functions to calculate the distance between
           colors.see https://en.wikipedia.org/wiki/Color_difference for details;*/
enum DISTANCE_TYPE
{
    CIE76,                      ///The 1976 formula is the first formula that related a measured color difference to a known set of CIELAB coordinates.
    CIE94_GRAPHIC_ARTS,        ///The 1976 definition was extended to address perceptual non-uniformities.
    CIE94_TEXTILES,
    CIE2000,
    CMC_1TO1,                   //In 1984, the Colour Measurement Committee of the Society of Dyers and Colourists defined a difference measure, also based on the L*C*h color model.
    CMC_2TO1,
    RGB,
    RGBL
};

/** @brief Core class of ccm model.
           produce a ColorCorrectionModel instance for inference.
*/

class CV_EXPORTS_W ColorCorrectionModel
{
public:
        Mat CV_WRAP ccm;
        /** @brief Color Correction Model
            @param src detected colors of ColorChecker patches;\n
                        the color type is RGB not BGR, and the color values are in [0, 1];
                        type is cv::Mat;
            @param constcolor the Built-in color card;\n
                        Supported list:
                                Macbeth(Macbeth ColorChecker) ;
                                Vinyl(DKK ColorChecker) ;
                                DigitalSG(DigitalSG ColorChecker with 140 squares);\n
                        type: enum CONST_COLOR;\n
        */
    ColorCorrectionModel(Mat src, CONST_COLOR constcolor);
     /** @brief Color Correction Model
            @param src detected colors of ColorChecker patches;\n
                        the color type is RGB not BGR, and the color values are in [0, 1];
                        type is cv::Mat;
            @param colors the reference color values,the color values are in [0, 1].\n
                        type: cv::Mat
            @param ref_cs the corresponding color space
                        NOTICE: For the list of color spaces supported, see the notes above;\n
                        If the color type is some RGB, the format is RGB not BGR;\n
                        type:enum COLOR_SPACE;
        */
    ColorCorrectionModel(Mat src, Mat colors, COLOR_SPACE ref_cs);
    /** @brief Color Correction Model
            @param src detected colors of ColorChecker patches;\n
                        the color type is RGB not BGR, and the color values are in [0, 1];
                        type is cv::Mat;
            @param colors the reference color values,the color values are in [0, 1].\n
                        type: cv::Mat
            @param ref_cs the corresponding color space
                        NOTICE: For the list of color spaces supported, see the notes above;\n
                        If the color type is some RGB, the format is RGB not BGR;\n
                        type:enum COLOR_SPACE;
            @param colored mask of colored color
        */
    ColorCorrectionModel(Mat src, Mat colors, COLOR_SPACE ref_cs, Mat colored);

        /** @brief set ColorSpace
            @param cs_ the absolute color space that detected colors convert to;\n
            NOTICE: it should be some RGB color space;\n
                    For the list of RGB color spaces supported, see the notes above;
            type: enum COLOR_SPACE;\n
            default: sRGB;
        */
    CV_WRAP void setColorSpace(COLOR_SPACE cs_);
        /** @brief set ccm_type
            @param ccm_type :the shape of color correction matrix(CCM);\n
            Supported list: "CCM_3x3"(3x3 matrix);"CCM_4x3"( 4x3 matrix);\n
            type: enum CCM_TYPE;\n
            default: CCM_3x3;\n
        */
    CV_WRAP void setCCM(CCM_TYPE ccm_type);
    /** @brief set Distance
        @param distance the type of color distance;\n
            Supported list:"CIE2000"; "CIE94_GRAPHIC_ARTS";"CIE94_TEXTILES";
                "CIE76";
                "CMC_1TO1";
                "CMC_2TO1";
                "RGB" (Euclidean distance of rgb color space);
                "RGBL" () Euclidean distance of rgbl color space);\n
            type: enum DISTANCE_TYPE;\n
            default: CIE2000;\n
            */
    CV_WRAP void setDistance(DISTANCE_TYPE distance);
     /** @brief set Linear
        @param linear_type the method of linearization;
            NOTICE: see Linearization.pdf for details;\n
            Supported list:
                "IDENTITY_" (no change is made);
                "GAMMA"( gamma correction;
                        Need assign a value to gamma simultaneously);
                "COLORPOLYFIT" (polynomial fitting channels respectively;
                                Need assign a value to deg simultaneously);
                "GRAYPOLYFIT"( grayscale polynomial fitting;
                                Need assign a value to deg and dst_whites simultaneously);
                "COLORLOGPOLYFIT"(logarithmic polynomial fitting channels respectively;
                                Need assign a value to deg simultaneously);
                "GRAYLOGPOLYFIT" (grayscale Logarithmic polynomial fitting;
                                Need assign a value to deg and dst_whites simultaneously);\n
            type: enum LINEAR_TYPE;\n
            default: GAMMA;\n
            */
    CV_WRAP void setLinear(LINEAR_TYPE linear_type);

     /** @brief set Gamma
        @param gamma the gamma value of gamma correction;
            NOTICE: only valid when linear is set to "gamma";\n
            type: double;\n
            default: 2.2;\n
            */
    CV_WRAP void setLinearGamma(double gamma);

    /** @brief set degree
        @param deg the degree of linearization polynomial;\n
            NOTICE: only valid when linear is set to "COLORPOLYFIT", "GRAYPOLYFIT",
                    "COLORLOGPOLYFIT" and "GRAYLOGPOLYFIT";\n
            type: int;\n
            default: 3;\n
            */
    CV_WRAP void setLinearDegree(int deg);
        /** @brief set SaturatedThreshold
        @param lower the lower threshold to determine saturation;\n
                default: 0;
        @param upper the upper threshold to determine saturation;
            NOTICE: it is a tuple of [lower, upper];
                    The colors in the closed interval [lower, upper] are reserved to participate
                    in the calculation of the loss function and initialization parameters\n
                    default: 0;
            */
    CV_WRAP void setSaturatedThreshold(double lower, double upper);
     /** @brief set WeightsList
        @param weights_list the list of weight of each color;\n
            type: cv::Mat;\n
            default: empty array;
            */
    CV_WRAP void setWeightsList(Mat weights_list);
    /** @brief set WeightCoeff
        @param weights_coeff the exponent number of L* component of the reference color in CIE Lab color space;\n
            type: double;\n
            default: 0;\n
            */
    CV_WRAP void setWeightCoeff(double weights_coeff);
    /** @brief set InitialMethod
        @param initial_method_type the method of calculating CCM initial value;\n
            Supported list:
                'LEAST_SQUARE' (least-squre method);
                'WHITE_BALANCE' (white balance method);\n
            type: enum INITIAL_METHOD_TYPE;
            */
    CV_WRAP void setInitialMethod(INITIAL_METHOD_TYPE initial_method_type);
    /** @brief set MaxCount
        @param max_count used in MinProblemSolver-DownhillSolver;\n
            Terminal criteria to the algorithm;\n
            type: int;
            default: 5000;
            */
    CV_WRAP void setMaxCount(int max_count);
     /** @brief set Epsilon
        @param epsilon used in MinProblemSolver-DownhillSolver;\n
            Terminal criteria to the algorithm;\n
            type: double;
            default: 1e-4;
            */
    CV_WRAP void setEpsilon(double epsilon);
    /** @brief make color correction*/
    CV_WRAP void run();

    // /** @brief Infer using fitting ccm.
    //     @param img the input image, type of cv::Mat.
    //     @param islinear default false.
    //     @return the output array, type of cv::Mat.
    // */
    CV_WRAP Mat infer(const Mat& img, bool islinear = false);
private:
    CV_WRAP class Impl;
    CV_WRAP Ptr<Impl> p;

};

} // namespace ccm
} // namespace cv

#endif