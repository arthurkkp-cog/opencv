/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                        Intel License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2000, Intel Corporation, all rights reserved.
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
//   * The name of Intel Corporation may not be used to endorse or promote products
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

#include "precomp.hpp"

#ifndef OPENCV_EXCLUDE_C_API

namespace cv
{

void DefaultDeleter<CvMat>::operator ()(CvMat* obj) const
{
    if( obj )
    {
        cvDecRefData( obj );
        cvFree( &obj );
    }
}

void DefaultDeleter<IplImage>::operator ()(IplImage* obj) const
{
    if( obj )
    {
        char* ptr = obj->imageDataOrigin;
        obj->imageData = obj->imageDataOrigin = 0;
        cvFree( &ptr );
        cvFree( &obj->roi );
        cvFree( &obj );
    }
}

void DefaultDeleter<CvMatND>::operator ()(CvMatND* obj) const
{
    if( obj )
    {
        cvDecRefData( obj );
        cvFree( &obj );
    }
}

void DefaultDeleter<CvSparseMat>::operator ()(CvSparseMat* obj) const
{
    if( obj )
    {
        CvMemStorage* storage = obj->heap->storage;
        cvReleaseMemStorage( &storage );
        cvFree( &obj->hashtable );
        cvFree( &obj );
    }
}

void DefaultDeleter<CvMemStorage>::operator ()(CvMemStorage* obj) const { cvReleaseMemStorage(&obj); }

} // cv::


CV_IMPL void
cvRelease( void** struct_ptr )
{
    if( !struct_ptr )
        CV_Error( cv::Error::StsNullPtr, "NULL double pointer" );

    if( *struct_ptr )
    {
        if( CV_IS_MAT(*struct_ptr) )
        {
            CvMat* mat = (CvMat*)*struct_ptr;
            *struct_ptr = 0;
            cvDecRefData( mat );
            cvFree( &mat );
        }
        else if( CV_IS_IMAGE(*struct_ptr))
        {
            IplImage* img = (IplImage*)*struct_ptr;
            *struct_ptr = 0;
            char* ptr = img->imageDataOrigin;
            img->imageData = img->imageDataOrigin = 0;
            cvFree( &ptr );
            cvFree( &img->roi );
            cvFree( &img );
        }
        else
            CV_Error( cv::Error::StsError, "Unknown object type" );
    }
}

void* cvClone( const void* struct_ptr )
{
    void* ptr = 0;
    if( !struct_ptr )
        CV_Error( cv::Error::StsNullPtr, "NULL structure pointer" );

    if( CV_IS_MAT(struct_ptr) )
    {
        const CvMat* src = (const CvMat*)struct_ptr;
        if( !CV_IS_MAT_HDR( src ))
            CV_Error( cv::Error::StsBadArg, "Bad CvMat header" );
        int type = CV_MAT_TYPE(src->type);
        int min_step = CV_ELEM_SIZE(type) * src->cols;
        CvMat* dst = (CvMat*)cvAlloc( sizeof(CvMat) );
        dst->step = min_step;
        dst->type = CV_MAT_MAGIC_VAL | type | CV_MAT_CONT_FLAG;
        dst->rows = src->rows;
        dst->cols = src->cols;
        dst->data.ptr = 0;
        dst->refcount = 0;
        dst->hdr_refcount = 1;
        if( (int64)dst->step*dst->rows > INT_MAX )
            dst->type &= ~CV_MAT_CONT_FLAG;
        if( src->data.ptr )
        {
            size_t step = dst->step;
            if( step == 0 )
                step = CV_ELEM_SIZE(dst->type)*dst->cols;
            int64 _total_size = (int64)step*dst->rows + sizeof(int) + CV_MALLOC_ALIGN;
            size_t total_size = (size_t)_total_size;
            if(_total_size != (int64)total_size)
                CV_Error(cv::Error::StsNoMem, "Too big buffer is allocated" );
            dst->refcount = (int*)cvAlloc( (size_t)total_size );
            dst->data.ptr = (uchar*)cvAlignPtr( dst->refcount + 1, CV_MALLOC_ALIGN );
            *dst->refcount = 1;
            cv::Mat _src = cv::cvarrToMat(src);
            cv::Mat _dst = cv::cvarrToMat(dst);
            _src.copyTo(_dst);
        }
        ptr = dst;
    }
    else if( CV_IS_IMAGE(struct_ptr))
    {
        const IplImage* src = (const IplImage*)struct_ptr;
        if( !CV_IS_IMAGE_HDR( src ))
            CV_Error( cv::Error::StsBadArg, "Bad image header" );
        IplImage* dst = (IplImage*)cvAlloc( sizeof(*dst));
        memcpy( dst, src, sizeof(*src));
        dst->nSize = sizeof(IplImage);
        dst->imageData = dst->imageDataOrigin = 0;
        dst->roi = 0;
        if( src->roi )
        {
            dst->roi = (IplROI*)cvAlloc( sizeof(IplROI));
            dst->roi->coi = src->roi->coi;
            dst->roi->xOffset = src->roi->xOffset;
            dst->roi->yOffset = src->roi->yOffset;
            dst->roi->width = src->roi->width;
            dst->roi->height = src->roi->height;
        }
        if( src->imageData )
        {
            const int64 imageSize_tmp = (int64)dst->widthStep*(int64)dst->height;
            if( (int64)dst->imageSize != imageSize_tmp )
                CV_Error( cv::Error::StsNoMem, "Overflow for imageSize" );
            dst->imageData = dst->imageDataOrigin =
                        (char*)cvAlloc( (size_t)dst->imageSize );
            memcpy( dst->imageData, src->imageData, dst->imageSize );
        }
        ptr = dst;
    }
    else
        CV_Error( cv::Error::StsError, "Unknown object type" );
    return ptr;
}


#endif  // OPENCV_EXCLUDE_C_API
/* End of file. */
