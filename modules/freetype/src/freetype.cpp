/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                           License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2000-2008, Intel Corporation, all rights reserved.
// Copyright (C) 2009, Willow Garage Inc., all rights reserved.
// Third party copyrights are property of their respective owners.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of the copyright holders may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the Intel Corporation or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/
//################################################################################
//
//                    Created by Kumataro
//
//################################################################################

#include "precomp.hpp"

namespace cv {
namespace freetype {

using namespace std;

class CV_EXPORTS_W FreeType2Impl : public FreeType2
{
public:
    FreeType2Impl();
    ~FreeType2Impl();
    void loadFontData(String fontFileName, int id) ;
    void setSplitNumber( int num );
    void putText(
        InputOutputArray img, const String& text, Point org,
        int fontHeight, Scalar color,
        int thickness, int line_type, bool bottomLeftOrigin
    );

private:
    FT_Library       mLibrary;
    FT_Face          mFace;
    FT_Outline_Funcs mFn;

    Point            mOrg;
    int              mLine_type;
    int              mThickness;
    int              mHeight;
    Scalar           mColor;
    bool             mIsFaceAvailable;
    String           mText;
    int              mCtoL;
    hb_font_t        *mHb_font;

    void putTextBitmapMono ( InputOutputArray _img);
    void putTextBitmapBlend( InputOutputArray _img);
    void putTextOutline    ( InputOutputArray _img);

    static int mvFn( const FT_Vector *to, void * user);
    static int lnFn( const FT_Vector *to, void * user);
    static int coFn( const FT_Vector *cnt,
                     const FT_Vector *to,
                     void * user);
    static int cuFn( const FT_Vector *cnt1,
                     const FT_Vector *cnt2,
                     const FT_Vector *to,
                     void * user);
    static void readNextCode(FT_Long &c, int &i, const String &text );

    static unsigned int ftd(unsigned int a){
        return (unsigned int)(a + (1 << 5)  ) >> 6;
    }
    class PathUserData{
    private:
    public:
        PathUserData( InputOutputArray _img) : mImg(_img) {};

        InputOutputArray mImg;
        Scalar mColor;
        int    mThickness;
        int    mLine_type;
        FT_Vector        mOldP;
        int              mCtoL;
        std::vector < Point > mPts;
    };
};

FreeType2Impl::FreeType2Impl()
{
    FT_Init_FreeType(&(this->mLibrary) );

    mCtoL        = 16;
    mFn.shift    = 0;
    mFn.delta    = 0;
    mFn.move_to  = FreeType2Impl::mvFn;
    mFn.line_to  = FreeType2Impl::lnFn;
    mFn.cubic_to = FreeType2Impl::cuFn;
    mFn.conic_to = FreeType2Impl::coFn;

    mIsFaceAvailable = false;
}

FreeType2Impl::~FreeType2Impl()
{
    if( mIsFaceAvailable  == true ){
        hb_font_destroy (mHb_font);
        FT_Done_Face(mFace);
        mIsFaceAvailable = false;
    }
    FT_Done_FreeType(mLibrary);
}

void FreeType2Impl::loadFontData(String fontFileName, int idx)
{
    if( mIsFaceAvailable  == true ){
        hb_font_destroy (mHb_font);
        FT_Done_Face(mFace);
    }
    FT_New_Face( mLibrary, fontFileName.c_str(), idx, &(mFace) );
    mHb_font = hb_ft_font_create (mFace, NULL);
    mIsFaceAvailable = true;
}

void FreeType2Impl::setSplitNumber(int num ){
    CV_Assert( num > 0 );
    mCtoL        = num;
}

void FreeType2Impl::putText(
    InputOutputArray _img, const String& _text, Point _org,
    int _fontHeight, Scalar _color,
    int _thickness, int _line_type, bool bottomLeftOrigin
)
{
    if ( mIsFaceAvailable == false ){
        return; // NEED TO SHOW ERROR
    }
    if ( _text.empty() )
    {
         return;
    }

    FT_Set_Pixel_Sizes( mFace, _fontHeight, _fontHeight );

    if( _line_type == CV_AA && _img.depth() != CV_8U ){
        _line_type = 8;
    }

    mThickness = _thickness;
    mLine_type = _line_type;
    mColor     = _color;
    mHeight    = _fontHeight;
    mText      = _text;
    mOrg       = _org;

    if( !bottomLeftOrigin ) {
        mOrg.y += mHeight;
    }

    if( mThickness < 0 ) // CV_FILLED
    {
        if ( mLine_type == CV_AA ) {
            putTextBitmapBlend(_img);
        }else{
            putTextBitmapMono (_img);
        }
    }else{
        putTextOutline(_img);
    }
}

void FreeType2Impl::putTextOutline(InputOutputArray _img)
{
    hb_buffer_t *hb_buffer;
    hb_buffer = hb_buffer_create ();

    unsigned int textLen;
    hb_buffer_guess_segment_properties (hb_buffer);
    hb_buffer_add_utf8 (hb_buffer, mText.c_str(), -1, 0, -1);
    hb_glyph_info_t *info =
        hb_buffer_get_glyph_infos(hb_buffer,&textLen );
    hb_shape (mHb_font, hb_buffer, NULL, 0);

    mOrg.y -= mHeight;
    PathUserData *userData = new PathUserData( _img );
    userData->mColor     = mColor;
    userData->mCtoL      = mCtoL;
    userData->mThickness = mThickness;
    userData->mLine_type = mLine_type;

    for( unsigned int i = 0 ; i < textLen ; i ++ ){
        FT_Load_Glyph(mFace, info[i].codepoint, 0 );

        FT_GlyphSlot slot  = mFace->glyph;
        FT_Outline outline = slot->outline;

        // Flip
        FT_Matrix mtx = { 1 << 16 , 0 , 0 , -(1 << 16) };
        FT_Outline_Transform(&outline, &mtx);

        // Move
        FT_Outline_Translate(&outline,
                             (FT_Pos)(mOrg.x << 6),
                             (FT_Pos)((mOrg.y + mHeight)  << 6) );

        // Draw
        FT_Outline_Decompose(&outline, &mFn, (void*)userData);

        // Draw (Last Path)
        mvFn( NULL, (void*)userData );

        mOrg.x += ( mFace->glyph->advance.x ) >> 6;
        mOrg.y += ( mFace->glyph->advance.y ) >> 6;
   }
   delete userData;
   hb_buffer_destroy (hb_buffer);
}

void FreeType2Impl::putTextBitmapMono(InputOutputArray _img)
{
    Mat dst = _img.getMat();
    hb_buffer_t *hb_buffer;
    hb_buffer = hb_buffer_create ();

    unsigned int textLen;
    hb_buffer_guess_segment_properties (hb_buffer);
    hb_buffer_add_utf8 (hb_buffer, mText.c_str(), -1, 0, -1);
    hb_glyph_info_t *info =
        hb_buffer_get_glyph_infos(hb_buffer,&textLen );
    hb_shape (mHb_font, hb_buffer, NULL, 0);

    for( unsigned int i = 0 ; i < textLen ; i ++ ){
        FT_Load_Glyph(mFace, info[i].codepoint, 0 );
        FT_Render_Glyph( mFace->glyph, FT_RENDER_MODE_MONO );
        FT_Bitmap    *bmp = &(mFace->glyph->bitmap);

        Point gPos = mOrg;
        gPos.y -= ( mFace->glyph->metrics.horiBearingY >> 6) ;
        gPos.x += ( mFace->glyph->metrics.horiBearingX >> 6) ;

        for (int row = 0; row < (int)bmp->rows; row ++) {
            if( gPos.y + row < 0 ) {
                continue;
            }
            if( gPos.y + row >= dst.rows ) {
                break;
            }

            for (int col = 0; col < bmp->pitch; col ++) {
                int cl = bmp->buffer[ row * bmp->pitch + col ];
                if ( cl == 0 ) {
                    continue;
                }
                for(int bit = 7; bit >= 0; bit -- ){
                    if( gPos.x + col * 8 + (7 - bit) < 0 )
                    {
                        continue;
                    }
                    if( gPos.x + col * 8 + (7 - bit) >= dst.cols )
                    {
                        break;
                    }

                    if ( ( (cl >> bit) & 0x01 ) == 1 ) {
                        cv::Vec3b* ptr = dst.ptr<cv::Vec3b>( gPos.y + row,  gPos.x + col * 8 + (7 - bit) );
                        (*ptr)[0] = mColor[0];
                        (*ptr)[1] = mColor[1];
                        (*ptr)[2] = mColor[2];
                    }
                }
            }
        }

        mOrg.x += ( mFace->glyph->advance.x ) >> 6;
        mOrg.y += ( mFace->glyph->advance.y ) >> 6;
    }
    hb_buffer_destroy (hb_buffer);
}

void FreeType2Impl::putTextBitmapBlend(InputOutputArray _img)
{
    Mat dst = _img.getMat();
    hb_buffer_t *hb_buffer;
    hb_buffer = hb_buffer_create ();

    unsigned int textLen;
    hb_buffer_guess_segment_properties (hb_buffer);
    hb_buffer_add_utf8 (hb_buffer, mText.c_str(), -1, 0, -1);
    hb_glyph_info_t *info =
        hb_buffer_get_glyph_infos(hb_buffer,&textLen );
    hb_shape (mHb_font, hb_buffer, NULL, 0);

    for( unsigned int i = 0 ; i < textLen ; i ++ ){
        FT_Load_Glyph(mFace, info[i].codepoint, 0 );
        FT_Render_Glyph( mFace->glyph, FT_RENDER_MODE_NORMAL );
        FT_Bitmap    *bmp = &(mFace->glyph->bitmap);

        Point gPos = mOrg;
        gPos.y -= ( mFace->glyph->metrics.horiBearingY >> 6) ;
        gPos.x += ( mFace->glyph->metrics.horiBearingX >> 6) ;

        for (int row = 0; row < (int)bmp->rows; row ++) {
            if( gPos.y + row < 0 ) {
                continue;
            }
            if( gPos.y + row >= dst.rows ) {
                break;
            }

            for (int col = 0; col < bmp->pitch; col ++) {
                int cl = bmp->buffer[ row * bmp->pitch + col ];
                if ( cl == 0 ) {
                    continue;
                }
                if( gPos.x + col < 0 )
                {
                    continue;
                }
                if( gPos.x + col >= dst.cols )
                {
                    break;
                }

                cv::Vec3b* ptr = dst.ptr<cv::Vec3b>( gPos.y + row , gPos.x + col);
                double blendAlpha = (double ) cl / 255.0;

                (*ptr)[0] = (double) mColor[0] * blendAlpha + (*ptr)[0] * (1.0 - blendAlpha );
                (*ptr)[1] = (double) mColor[1] * blendAlpha + (*ptr)[1] * (1.0 - blendAlpha );
                (*ptr)[2] = (double) mColor[2] * blendAlpha + (*ptr)[2] * (1.0 - blendAlpha );
            }
        }
        mOrg.x += ( mFace->glyph->advance.x ) >> 6;
        mOrg.y += ( mFace->glyph->advance.y ) >> 6;
    }
    hb_buffer_destroy (hb_buffer);
}

int FreeType2Impl::mvFn( const FT_Vector *to, void * user)
{
    if(user == NULL ) { return 1; }
    PathUserData *p = (PathUserData*)user;

    if( p->mPts.size() > 0 ){
        Mat dst = p->mImg.getMat();
        const Point *ptsList[] = { &(p->mPts[0]) };
        int npt[1]; npt[0] = p->mPts.size();
        polylines(
            dst,
            ptsList,
            npt,
            1,
            false,
            p->mColor,
            p->mThickness,
            p->mLine_type,
            0
        );
    }

    p->mPts.clear();

    if( to == NULL ) { return 1; }

    p->mPts.push_back( Point ( ftd(to->x), ftd(to->y) ) );
    p->mOldP = *to;
    return 0;
}

int FreeType2Impl::lnFn( const FT_Vector *to, void * user)
{
    if(to   == NULL ) { return 1; }
    if(user == NULL ) { return 1; }

    PathUserData *p = (PathUserData *)user;
    p->mPts.push_back( Point ( ftd(to->x), ftd(to->y) ) );
    p->mOldP = *to;
    return 0;
}

int FreeType2Impl::coFn( const FT_Vector *cnt,
                     const FT_Vector *to,
                     void * user)
{
    if(cnt  == NULL ) { return 1; }
    if(to   == NULL ) { return 1; }
    if(user == NULL ) { return 1; }

    PathUserData *p = (PathUserData *)user;

    // Bezier to Line
    for(int i = 0;i <= p->mCtoL; i++){
        double u = (double)i * 1.0 / (p->mCtoL) ;
        double nu = 1.0 - u;
        double p0 =                  nu * nu;
        double p1 = 2.0 * u *        nu;
        double p2 =       u * u;

        double X = (p->mOldP.x) * p0 + cnt->x * p1 + to->x * p2;
        double Y = (p->mOldP.y) * p0 + cnt->y * p1 + to->y * p2;
        p->mPts.push_back( Point ( ftd(X), ftd(Y) ) );
    }
    p->mOldP = *to;
    return 0;
}

int FreeType2Impl::cuFn( const FT_Vector *cnt1,
                     const FT_Vector *cnt2,
                     const FT_Vector *to,
                     void * user)
{
    if(cnt1 == NULL ) { return 1; }
    if(cnt2 == NULL ) { return 1; }
    if(to   == NULL ) { return 1; }
    if(user == NULL ) { return 1; }

    PathUserData *p = (PathUserData *)user;

    // Bezier to Line
    for(int i = 0; i <= p->mCtoL ;i++){
        double u = (double)i * 1.0 / (p->mCtoL) ;
        double nu = 1.0 - u;
        double p0 =                  nu * nu * nu;
        double p1 = 3.0 * u *        nu * nu;
        double p2 = 3.0 * u * u *    nu;
        double p3 =       u * u * u;

        double X = (p->mOldP.x) * p0 + (cnt1->x)    * p1 +
                   (cnt2->x   ) * p2 + (to->x  )    * p3;
        double Y = (p->mOldP.y) * p0 + (cnt1->y)    * p1 +
                   (cnt2->y   ) * p2 + (to->y  )    * p3;

        p->mPts.push_back( Point ( ftd(X), ftd(Y) ) );
    }
    p->mOldP = *to;
    return 0;
}

CV_EXPORTS_W Ptr<FreeType2> createFreeType2()
{
    return Ptr<FreeType2Impl> (new FreeType2Impl () );
}


}} // namespace freetype2
