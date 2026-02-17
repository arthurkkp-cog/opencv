// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html

#include "precomp.hpp"
#include "opencv2/core/mat.hpp"
#include "opencv2/core/types_c.h"

#ifndef OPENCV_EXCLUDE_C_API
// glue

CvMatND cvMatND(const cv::Mat& m)
{
    CvMatND self;
    cvInitMatNDHeader(&self, m.dims, m.size, m.type(), m.data );
    int i, d = m.dims;
    for( i = 0; i < d; i++ )
        self.dim[i].step = (int)m.step[i];
    self.type |= m.flags & cv::Mat::CONTINUOUS_FLAG;
    return self;
}

_IplImage cvIplImage(const cv::Mat& m)
{
    _IplImage self;
    CV_Assert( m.dims <= 2 );
    cvInitImageHeader(&self, cvSize(m.size()), cvIplDepth(m.flags), m.channels());
    cvSetData(&self, m.data, (int)m.step[0]);
    return self;
}

namespace cv {

static Mat cvMatToMat(const CvMat* m, bool copyData)
{
    Mat thiz;

    if( !m )
        return thiz;

    if( !copyData )
    {
        thiz.flags = Mat::MAGIC_VAL + (m->type & (CV_MAT_TYPE_MASK|CV_MAT_CONT_FLAG));
        thiz.dims = 2;
        thiz.rows = m->rows;
        thiz.cols = m->cols;
        thiz.datastart = thiz.data = m->data.ptr;
        size_t esz = CV_ELEM_SIZE(m->type), minstep = thiz.cols*esz, _step = m->step;
        if( _step == 0 )
            _step = minstep;
        thiz.datalimit = thiz.datastart + _step*thiz.rows;
        thiz.dataend = thiz.datalimit - _step + minstep;
        thiz.step[0] = _step; thiz.step[1] = esz;
    }
    else
    {
        thiz.datastart = thiz.dataend = thiz.data = 0;
        Mat(m->rows, m->cols, m->type, m->data.ptr, m->step).copyTo(thiz);
    }

    return thiz;
}

static Mat cvMatNDToMat(const CvMatND* m, bool copyData)
{
    Mat thiz;

    if( !m )
        return thiz;
    thiz.datastart = thiz.data = m->data.ptr;
    thiz.flags |= CV_MAT_TYPE(m->type);
    int _sizes[CV_MAX_DIM];
    size_t _steps[CV_MAX_DIM];

    int d = m->dims;
    for( int i = 0; i < d; i++ )
    {
        _sizes[i] = m->dim[i].size;
        _steps[i] = m->dim[i].step;
    }

    setSize(thiz, d, _sizes, _steps);
    finalizeHdr(thiz);

    if( copyData )
    {
        Mat temp(thiz);
        thiz.release();
        temp.copyTo(thiz);
    }

    return thiz;
}

static Mat iplImageToMat(const IplImage* img, bool copyData)
{
    Mat m;

    if( !img )
        return m;

    m.dims = 2;
    CV_DbgAssert(CV_IS_IMAGE(img) && img->imageData != 0);

    int imgdepth = IPL2CV_DEPTH(img->depth);
    size_t esz;
    m.step[0] = img->widthStep;

    if(!img->roi)
    {
        CV_Assert(img->dataOrder == IPL_DATA_ORDER_PIXEL);
        m.flags = Mat::MAGIC_VAL + CV_MAKETYPE(imgdepth, img->nChannels);
        m.rows = img->height;
        m.cols = img->width;
        m.datastart = m.data = (uchar*)img->imageData;
        esz = CV_ELEM_SIZE(m.flags);
    }
    else
    {
        CV_Assert(img->dataOrder == IPL_DATA_ORDER_PIXEL || img->roi->coi != 0);
        bool selectedPlane = img->roi->coi && img->dataOrder == IPL_DATA_ORDER_PLANE;
        m.flags = Mat::MAGIC_VAL + CV_MAKETYPE(imgdepth, selectedPlane ? 1 : img->nChannels);
        m.rows = img->roi->height;
        m.cols = img->roi->width;
        esz = CV_ELEM_SIZE(m.flags);
        m.datastart = m.data = (uchar*)img->imageData +
            (selectedPlane ? (img->roi->coi - 1)*m.step*img->height : 0) +
            img->roi->yOffset*m.step[0] + img->roi->xOffset*esz;
    }
    m.datalimit = m.datastart + m.step.p[0]*m.rows;
    m.dataend = m.datastart + m.step.p[0]*(m.rows-1) + esz*m.cols;
    m.step[1] = esz;
    m.updateContinuityFlag();

    if( copyData )
    {
        Mat m2 = m;
        m.release();
        if( !img->roi || !img->roi->coi ||
            img->dataOrder == IPL_DATA_ORDER_PLANE)
            m2.copyTo(m);
        else
        {
            int ch[] = {img->roi->coi - 1, 0};
            m.create(m2.rows, m2.cols, m2.type());
            mixChannels(&m2, 1, &m, 1, ch, 1);
        }
    }

    return m;
}

Mat cvarrToMat(const CvArr* arr, bool copyData,
               bool /*allowND*/, int coiMode, AutoBuffer<double>* abuf )
{
    if( !arr )
        return Mat();
    if( CV_IS_MAT_HDR_Z(arr) )
        return cvMatToMat((const CvMat*)arr, copyData);
    if( CV_IS_MATND(arr) )
        return cvMatNDToMat((const CvMatND*)arr, copyData );
    if( CV_IS_IMAGE(arr) )
    {
        const IplImage* iplimg = (const IplImage*)arr;
        if( coiMode == 0 && iplimg->roi && iplimg->roi->coi > 0 )
            CV_Error(cv::Error::BadCOI, "COI is not supported by the function");
        return iplImageToMat(iplimg, copyData);
    }
    if( CV_IS_SEQ(arr) )
    {
        CvSeq* seq = (CvSeq*)arr;
        int total = seq->total, type = CV_MAT_TYPE(seq->flags), esz = seq->elem_size;
        if( total == 0 )
            return Mat();
        CV_Assert(total > 0 && CV_ELEM_SIZE(seq->flags) == esz);
        if(!copyData && seq->first->next == seq->first)
            return Mat(total, 1, type, seq->first->data);
        if( abuf )
        {
            abuf->allocate(((size_t)total*esz + sizeof(double)-1)/sizeof(double));
            double* bufdata = abuf->data();
            cvCvtSeqToArray(seq, bufdata, CV_WHOLE_SEQ);
            return Mat(total, 1, type, bufdata);
        }

        Mat buf(total, 1, type);
        cvCvtSeqToArray(seq, buf.ptr(), CV_WHOLE_SEQ);
        return buf;
    }
    CV_Error(cv::Error::StsBadArg, "Unknown array type");
}

void extractImageCOI(const CvArr* arr, OutputArray _ch, int coi)
{
    Mat mat = cvarrToMat(arr, false, true, 1);
    _ch.create(mat.dims, mat.size, mat.depth());
    Mat ch = _ch.getMat();
    if(coi < 0)
    {
        CV_Assert( CV_IS_IMAGE(arr) );
        coi = cvGetImageCOI((const IplImage*)arr)-1;
    }
    CV_Assert(0 <= coi && coi < mat.channels());
    int _pairs[] = { coi, 0 };
    mixChannels( &mat, 1, &ch, 1, _pairs, 1 );
}

void insertImageCOI(InputArray _ch, CvArr* arr, int coi)
{
    Mat ch = _ch.getMat(), mat = cvarrToMat(arr, false, true, 1);
    if(coi < 0)
    {
        CV_Assert( CV_IS_IMAGE(arr) );
        coi = cvGetImageCOI((const IplImage*)arr)-1;
    }
    CV_Assert(ch.size == mat.size && ch.depth() == mat.depth() && 0 <= coi && coi < mat.channels());
    int _pairs[] = { 0, coi };
    mixChannels( &ch, 1, &mat, 1, _pairs, 1 );
}

} // cv::

#endif  // OPENCV_EXCLUDE_C_API
