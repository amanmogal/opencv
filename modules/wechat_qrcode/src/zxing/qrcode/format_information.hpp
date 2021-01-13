// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.
//
// Tencent is pleased to support the open source community by making WeChat QRCode available.
// Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.

/*
 *  format_information.hpp
 *  zxing
 *
 *  Copyright 2010 ZXing authors All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __ZXING_QRCODE_FORMAT_INFORMATION_HPP__
#define __ZXING_QRCODE_FORMAT_INFORMATION_HPP__

#include "zxing/common/counted.hpp"
#include "zxing/errorhandler.hpp"
#include "zxing/qrcode/error_correction_level.hpp"

namespace zxing {
namespace qrcode {

class FormatInformation : public Counted {
private:
    static int FORMAT_INFO_MASK_QR;
    static int FORMAT_INFO_DECODE_LOOKUP[][2];
    static int N_FORMAT_INFO_DECODE_LOOKUPS;
    static int BITS_SET_IN_HALF_BYTE[];

    ErrorCorrectionLevel &errorCorrectionLevel_;
    char dataMask_;
    float possiableFix_;

    FormatInformation(int formatInfo, float possiableFix, ErrorHandler &err_handler);

public:
    static int numBitsDiffering(int a, int b);
    static Ref<FormatInformation> decodeFormatInformation(int maskedFormatInfo1,
                                                          int maskedFormatInfo2);
    static Ref<FormatInformation> doDecodeFormatInformation(int maskedFormatInfo1,
                                                            int maskedFormatInfo2);
    ErrorCorrectionLevel &getErrorCorrectionLevel();
    char getDataMask();
    float getPossiableFix();
    friend bool operator==(const FormatInformation &a, const FormatInformation &b);
    friend std::ostream &operator<<(std::ostream &out, const FormatInformation &fi);
};
}  // namespace qrcode
}  // namespace zxing

#endif  // __ZXING_QRCODE_FORMAT_INFORMATION_HPP__