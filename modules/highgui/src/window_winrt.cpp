// highgui to XAML bridge for OpenCV

// Copyright (c) Microsoft Open Technologies, Inc.
// All rights reserved.
//
// (3 - clause BSD License)
//
// Redistribution and use in source and binary forms, with or without modification, are permitted provided that
// the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the
// following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
// following disclaimer in the documentation and/or other materials provided with the distribution.
// 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or
// promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT(INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include "precomp.hpp"

#include <map>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <opencv2\highgui.hpp>
#include <opencv2\highgui\highgui_winrt.hpp>
#include "window_winrt_bridge.hpp"

#define CV_WINRT_NO_GUI_ERROR( funcname )       \
{                                               \
    cvError( cv::Error::StsNotImplemented, funcname,    \
    "The function is not implemented. ",        \
    __FILE__, __LINE__ );                       \
}

#ifdef CV_ERROR
#undef CV_ERROR
#define CV_ERROR( Code, Msg )                                       \
{                                                                   \
    cvError( (Code), cvFuncName, Msg, __FILE__, __LINE__ );         \
};
#endif

/********************************** WinRT Specific API Implementation ******************************************/

// Initializes or overrides container contents with default XAML markup structure
void cv::winrt_initContainer(::Windows::UI::Xaml::Controls::Panel^ _container)
{
    HighguiBridge::getInstance().setContainer(_container);
}

/********************************** API Implementation *********************************************************/

/********************************** Not YET implemented API ****************************************************/

/********************************** Disabled or not supported API **********************************************/

