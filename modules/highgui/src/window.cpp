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
#include "backend.hpp"

#include "opencv2/core/opengl.hpp"
#include "opencv2/core/utils/logger.hpp"

// in later times, use this file as a dispatcher to implementations like cvcap.cpp


using namespace cv;
using namespace cv::highgui_backend;

namespace cv {

Mutex& getWindowMutex()
{
    static Mutex* g_window_mutex = new Mutex();
    return *g_window_mutex;
}

namespace impl {

typedef std::map<std::string, highgui_backend::UIWindowBase::Ptr> WindowsMap_t;
static WindowsMap_t& getWindowsMap()
{
    static WindowsMap_t g_windowsMap;
    return g_windowsMap;
}

static std::shared_ptr<UIWindow> findWindow_(const std::string& name)
{
    cv::AutoLock lock(cv::getWindowMutex());
    auto& windowsMap = getWindowsMap();
    auto i = windowsMap.find(name);
    if (i != windowsMap.end())
    {
        const auto& ui_base = i->second;
        if (ui_base)
        {
            if (!ui_base->isActive())
            {
                windowsMap.erase(i);
                return std::shared_ptr<UIWindow>();
            }
            auto window = std::dynamic_pointer_cast<UIWindow>(ui_base);
            return window;
        }
    }
    return std::shared_ptr<UIWindow>();
}

static void cleanupTrackbarCallbacksWithData_();  // forward declaration

static void cleanupClosedWindows_()
{
    cv::AutoLock lock(cv::getWindowMutex());
    auto& windowsMap = getWindowsMap();
    for (auto it = windowsMap.begin(); it != windowsMap.end();)
    {
        const auto& ui_base = it->second;
        bool erase = (!ui_base || !ui_base->isActive());
        if (erase)
        {
            it = windowsMap.erase(it);
        }
        else
        {
            ++it;
        }
    }

    cleanupTrackbarCallbacksWithData_();
}

// Just to support deprecated API, to be removed
struct TrackbarCallbackWithData
{
    std::weak_ptr<UITrackbar> trackbar_;
    int* data_;
    TrackbarCallback callback_;
    void* userdata_;

    TrackbarCallbackWithData(int* data, TrackbarCallback callback, void* userdata)
        : data_(data)
        , callback_(callback), userdata_(userdata)
    {
        // trackbar_ is initialized separatelly
    }

    ~TrackbarCallbackWithData()
    {
        CV_LOG_DEBUG(NULL, "UI/Trackbar: Cleanup deprecated TrackbarCallbackWithData");
    }

    void onChange(int pos)
    {
        if (data_)
            *data_ = pos;
        if (callback_)
            callback_(pos, userdata_);
    }

    static void onChangeCallback(int pos, void* userdata)
    {
        TrackbarCallbackWithData* thiz = (TrackbarCallbackWithData*)userdata;
        CV_Assert(thiz);
        return thiz->onChange(pos);
    }
};

typedef std::vector< std::shared_ptr<TrackbarCallbackWithData> > TrackbarCallbacksWithData_t;
static TrackbarCallbacksWithData_t& getTrackbarCallbacksWithData()
{
    static TrackbarCallbacksWithData_t g_trackbarCallbacksWithData;
    return g_trackbarCallbacksWithData;
}

static void cleanupTrackbarCallbacksWithData_()
{
    cv::AutoLock lock(cv::getWindowMutex());
    auto& callbacks = getTrackbarCallbacksWithData();
    for (auto it = callbacks.begin(); it != callbacks.end();)
    {
        const auto& cb = *it;
        bool erase = (!cb || cb->trackbar_.expired());
        if (erase)
        {
            it = callbacks.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

}}  // namespace cv::impl

using namespace cv::impl;

static void deprecateNotFoundNoOpBehavior()
{
    CV_LOG_ONCE_WARNING(NULL, "This no-op behavior is deprecated. Future versions of OpenCV will trigger exception in this case");
}
#define CV_NOT_FOUND_DEPRECATION deprecateNotFoundNoOpBehavior()

cv::Rect cv::getWindowImageRect(const String& winname)
{
    CV_TRACE_FUNCTION();
    CV_Assert(!winname.empty());

    {
        auto window = findWindow_(winname);
        if (window)
        {
            return window->getImageRect();
        }
    }

    auto backend = getCurrentUIBackend();
    if (backend)
    {
        CV_LOG_WARNING(NULL, "Can't find window with name: '" << winname << "'. Do nothing");
        CV_NOT_FOUND_DEPRECATION;
    }
    else
    {
        CV_LOG_WARNING(NULL, "No UI backends available. Use OPENCV_LOG_LEVEL=DEBUG for investigation");
    }
    return Rect(-1, -1, -1, -1);
}

void cv::namedWindow( const String& winname, int flags )
{
    CV_TRACE_FUNCTION();
    CV_Assert(!winname.empty());

    {
        cv::AutoLock lock(cv::getWindowMutex());
        cleanupClosedWindows_();
        auto& windowsMap = getWindowsMap();
        auto i = windowsMap.find(winname);
        if (i != windowsMap.end())
        {
            auto ui_base = i->second;
            if (ui_base)
            {
                auto window = std::dynamic_pointer_cast<UIWindow>(ui_base);
                if (!window)
                {
                    CV_LOG_ERROR(NULL, "OpenCV/UI: Can't create window: '" << winname << "'");
                }
                return;
            }
        }
        auto backend = getCurrentUIBackend();
        if (backend)
        {
            auto window = backend->createWindow(winname, flags);
            if (!window)
            {
                CV_LOG_ERROR(NULL, "OpenCV/UI: Can't create window: '" << winname << "'");
                return;
            }
            windowsMap.emplace(winname, window);
            return;
        }
    }

    CV_Error(Error::StsError,
        "The function is not implemented. "
        "Rebuild the library with Windows, GTK+ 2.x or Cocoa support. "
        "If you are on Ubuntu or Debian, install libgtk2.0-dev and pkg-config, then re-run cmake or configure script");
}

void cv::destroyWindow( const String& winname )
{
    CV_TRACE_FUNCTION();

    {
        auto window = findWindow_(winname);
        if (window)
        {
            window->destroy();
            cleanupClosedWindows_();
            return;
        }
    }

    CV_LOG_WARNING(NULL, "Can't find window with name: '" << winname << "'. Do nothing");
}

void cv::destroyAllWindows()
{
    CV_TRACE_FUNCTION();

    {
        cv::AutoLock lock(cv::getWindowMutex());
        auto backend = getCurrentUIBackend();
        if (backend)
        {
            backend->destroyAllWindows();
            cleanupClosedWindows_();
            return;
        }
    }

    CV_LOG_WARNING(NULL, "No UI backends available. Use OPENCV_LOG_LEVEL=DEBUG for investigation");
}

void cv::resizeWindow( const String& winname, int width, int height )
{
    CV_TRACE_FUNCTION();

    {
        auto window = findWindow_(winname);
        if (window)
        {
            return window->resize(width, height);
        }
    }

    auto backend = getCurrentUIBackend();
    if (backend)
    {
        CV_LOG_WARNING(NULL, "Can't find window with name: '" << winname << "'. Do nothing");
        CV_NOT_FOUND_DEPRECATION;
    }
    else
    {
        CV_LOG_WARNING(NULL, "No UI backends available. Use OPENCV_LOG_LEVEL=DEBUG for investigation");
    }
    return;
}

void cv::resizeWindow(const String& winname, const cv::Size& size)
{
   CV_TRACE_FUNCTION();
   resizeWindow(winname, size.width, size.height);
}

void cv::moveWindow( const String& winname, int x, int y )
{
    CV_TRACE_FUNCTION();

    {
        auto window = findWindow_(winname);
        if (window)
        {
            return window->move(x, y);
        }
    }

    auto backend = getCurrentUIBackend();
    if (backend)
    {
        CV_LOG_WARNING(NULL, "Can't find window with name: '" << winname << "'. Do nothing");
        CV_NOT_FOUND_DEPRECATION;
    }
    else
    {
        CV_LOG_WARNING(NULL, "No UI backends available. Use OPENCV_LOG_LEVEL=DEBUG for investigation");
    }
    return;
}

void cv::setWindowTitle(const String& winname, const String& title)
{
    CV_TRACE_FUNCTION();

    {
        cv::AutoLock lock(cv::getWindowMutex());
        auto window = findWindow_(winname);
        if (window)
        {
            return window->setTitle(title);
        }
    }

    auto backend = getCurrentUIBackend();
    if (backend)
    {
        CV_LOG_WARNING(NULL, "Can't find window with name: '" << winname << "'. Do nothing");
        CV_NOT_FOUND_DEPRECATION;
    }
    else
    {
        CV_LOG_WARNING(NULL, "No UI backends available. Use OPENCV_LOG_LEVEL=DEBUG for investigation");
    }
    return;
}

void cv::setWindowProperty(const String& winname, int prop_id, double prop_value)
{
    CV_TRACE_FUNCTION();
    CV_Assert(!winname.empty());

    {
        auto window = findWindow_(winname);
        if (window)
        {
            window->setProperty(prop_id, prop_value);
            return;
        }
    }

    auto backend = getCurrentUIBackend();
    if (backend)
    {
        CV_LOG_WARNING(NULL, "Can't find window with name: '" << winname << "'. Do nothing");
        CV_NOT_FOUND_DEPRECATION;
    }
    else
    {
        CV_LOG_WARNING(NULL, "No UI backends available. Use OPENCV_LOG_LEVEL=DEBUG for investigation");
    }
}

double cv::getWindowProperty(const String& winname, int prop_id)
{
    CV_TRACE_FUNCTION();
    CV_Assert(!winname.empty());

    {
        auto window = findWindow_(winname);
        if (window)
        {
            double v = window->getProperty(prop_id);
            if (cvIsNaN(v))
                return -1;
            return v;
        }
    }

    auto backend = getCurrentUIBackend();
    if (backend)
    {
        CV_LOG_WARNING(NULL, "Can't find window with name: '" << winname << "'. Do nothing");
        CV_NOT_FOUND_DEPRECATION;
    }
    else
    {
        CV_LOG_WARNING(NULL, "No UI backends available. Use OPENCV_LOG_LEVEL=DEBUG for investigation");
    }
    return -1;
}

int cv::waitKeyEx(int delay)
{
    CV_TRACE_FUNCTION();

    {
        cv::AutoLock lock(cv::getWindowMutex());
        auto backend = getCurrentUIBackend();
        if (backend)
        {
            return backend->waitKeyEx(delay);
        }
    }

    CV_LOG_WARNING(NULL, "No UI backends available. Use OPENCV_LOG_LEVEL=DEBUG for investigation");
    return -1;
}

int cv::waitKey(int delay)
{
    CV_TRACE_FUNCTION();
    int code = waitKeyEx(delay);
#ifndef WINRT
    static int use_legacy = -1;
    if (use_legacy < 0)
    {
        use_legacy = utils::getConfigurationParameterBool("OPENCV_LEGACY_WAITKEY");
    }
    if (use_legacy > 0)
        return code;
#endif
    return (code != -1) ? (code & 0xff) : -1;
}

/*
 * process until queue is empty but don't wait.
 */
int cv::pollKey()
{
    CV_TRACE_FUNCTION();

    {
        cv::AutoLock lock(cv::getWindowMutex());
        auto backend = getCurrentUIBackend();
        if (backend)
        {
            return backend->pollKey();
        }
    }

    CV_LOG_WARNING(NULL, "No UI backends available. Use OPENCV_LOG_LEVEL=DEBUG for investigation");
    return -1;
}

int cv::createTrackbar(const String& trackbarName, const String& winName,
                   int* value, int count, TrackbarCallback callback,
                   void* userdata)
{
    CV_TRACE_FUNCTION();

    CV_LOG_IF_WARNING(NULL, value, "UI/Trackbar(" << trackbarName << "@" << winName << "): Using 'value' pointer is unsafe and deprecated. Use NULL as value pointer. "
            "To fetch trackbar value setup callback.");

    {
        cv::AutoLock lock(cv::getWindowMutex());
        auto window = findWindow_(winName);
        if (window)
        {
            if (value)
            {
                auto cb = std::make_shared<TrackbarCallbackWithData>(value, callback, userdata);
                auto trackbar = window->createTrackbar(trackbarName, count, TrackbarCallbackWithData::onChangeCallback, cb.get());
                if (!trackbar)
                {
                    CV_LOG_ERROR(NULL, "OpenCV/UI: Can't create trackbar: '" << trackbarName << "'@'" << winName << "'");
                    return 0;
                }
                cb->trackbar_ = trackbar;
                getTrackbarCallbacksWithData().emplace_back(cb);
                getWindowsMap().emplace(trackbar->getID(), trackbar);
                trackbar->setPos(*value);
                return 1;
            }
            else
            {
                auto trackbar = window->createTrackbar(trackbarName, count, callback, userdata);
                if (!trackbar)
                {
                    CV_LOG_ERROR(NULL, "OpenCV/UI: Can't create trackbar: '" << trackbarName << "'@'" << winName << "'");
                    return 0;
                }
                getWindowsMap().emplace(trackbar->getID(), trackbar);
                return 1;
            }
        }
    }

    auto backend = getCurrentUIBackend();
    if (backend)
    {
        CV_LOG_WARNING(NULL, "Can't find window with name: '" << winName << "'. Do nothing");
        CV_NOT_FOUND_DEPRECATION;
    }
    else
    {
        CV_LOG_WARNING(NULL, "No UI backends available. Use OPENCV_LOG_LEVEL=DEBUG for investigation");
    }
    return 0;
}

void cv::setTrackbarPos( const String& trackbarName, const String& winName, int value )
{
    CV_TRACE_FUNCTION();

    {
        cv::AutoLock lock(cv::getWindowMutex());
        auto window = findWindow_(winName);
        if (window)
        {
            auto trackbar = window->findTrackbar(trackbarName);
            CV_Assert(trackbar);
            return trackbar->setPos(value);
        }
    }

    auto backend = getCurrentUIBackend();
    if (backend)
    {
        CV_LOG_WARNING(NULL, "Can't find window with name: '" << winName << "'. Do nothing");
        CV_NOT_FOUND_DEPRECATION;
    }
    else
    {
        CV_LOG_WARNING(NULL, "No UI backends available. Use OPENCV_LOG_LEVEL=DEBUG for investigation");
    }
    return;
}

void cv::setTrackbarMax(const String& trackbarName, const String& winName, int maxval)
{
    CV_TRACE_FUNCTION();

    {
        cv::AutoLock lock(cv::getWindowMutex());
        auto window = findWindow_(winName);
        if (window)
        {
            auto trackbar = window->findTrackbar(trackbarName);
            CV_Assert(trackbar);
            Range old_range = trackbar->getRange();
            Range range(std::min(old_range.start, maxval), maxval);
            return trackbar->setRange(range);
        }
    }

    auto backend = getCurrentUIBackend();
    if (backend)
    {
        CV_LOG_WARNING(NULL, "Can't find window with name: '" << winName << "'. Do nothing");
        CV_NOT_FOUND_DEPRECATION;
    }
    else
    {
        CV_LOG_WARNING(NULL, "No UI backends available. Use OPENCV_LOG_LEVEL=DEBUG for investigation");
    }
    return;
}

void cv::setTrackbarMin(const String& trackbarName, const String& winName, int minval)
{
    CV_TRACE_FUNCTION();

    {
        cv::AutoLock lock(cv::getWindowMutex());
        auto window = findWindow_(winName);
        if (window)
        {
            auto trackbar = window->findTrackbar(trackbarName);
            CV_Assert(trackbar);
            Range old_range = trackbar->getRange();
            Range range(minval, std::max(minval, old_range.end));
            return trackbar->setRange(range);
        }
    }

    auto backend = getCurrentUIBackend();
    if (backend)
    {
        CV_LOG_WARNING(NULL, "Can't find window with name: '" << winName << "'. Do nothing");
        CV_NOT_FOUND_DEPRECATION;
    }
    else
    {
        CV_LOG_WARNING(NULL, "No UI backends available. Use OPENCV_LOG_LEVEL=DEBUG for investigation");
    }
    return;
}

int cv::getTrackbarPos( const String& trackbarName, const String& winName )
{
    CV_TRACE_FUNCTION();

    {
        cv::AutoLock lock(cv::getWindowMutex());
        auto window = findWindow_(winName);
        if (window)
        {
            auto trackbar = window->findTrackbar(trackbarName);
            CV_Assert(trackbar);
            return trackbar->getPos();
        }
    }

    auto backend = getCurrentUIBackend();
    if (backend)
    {
        CV_LOG_WARNING(NULL, "Can't find window with name: '" << winName << "'. Do nothing");
        CV_NOT_FOUND_DEPRECATION;
    }
    else
    {
        CV_LOG_WARNING(NULL, "No UI backends available. Use OPENCV_LOG_LEVEL=DEBUG for investigation");
    }
    return -1;
}

void cv::setMouseCallback( const String& windowName, MouseCallback onMouse, void* param)
{
    CV_TRACE_FUNCTION();

    {
        cv::AutoLock lock(cv::getWindowMutex());
        auto window = findWindow_(windowName);
        if (window)
        {
            return window->setMouseCallback(onMouse, param);
        }
    }

    auto backend = getCurrentUIBackend();
    if (backend)
    {
        CV_LOG_WARNING(NULL, "Can't find window with name: '" << windowName << "'. Do nothing");
        CV_NOT_FOUND_DEPRECATION;
    }
    else
    {
        CV_LOG_WARNING(NULL, "No UI backends available. Use OPENCV_LOG_LEVEL=DEBUG for investigation");
    }
    return;
}

int cv::getMouseWheelDelta( int flags )
{
    CV_TRACE_FUNCTION();
    return CV_GET_WHEEL_DELTA(flags);
}

int cv::startWindowThread()
{
    CV_TRACE_FUNCTION();
    return 0;
}

// OpenGL support

void cv::setOpenGlDrawCallback(const String& name, OpenGlDrawCallback callback, void* userdata)
{
    CV_TRACE_FUNCTION();
#ifndef HAVE_OPENGL
    CV_UNUSED(name);
    CV_UNUSED(callback);
    CV_UNUSED(userdata);
    CV_Error(cv::Error::OpenGlNotSupported, "The library is compiled without OpenGL support");
#else
    CV_UNUSED(name);
    CV_UNUSED(callback);
    CV_UNUSED(userdata);
    CV_Error(cv::Error::StsNotImplemented, "The function is not implemented");
#endif
}

void cv::setOpenGlContext(const String& windowName)
{
    CV_TRACE_FUNCTION();
#ifndef HAVE_OPENGL
    CV_UNUSED(windowName);
    CV_Error(cv::Error::OpenGlNotSupported, "The library is compiled without OpenGL support");
#else
    CV_UNUSED(windowName);
    CV_Error(cv::Error::StsNotImplemented, "The function is not implemented");
#endif
}

void cv::updateWindow(const String& windowName)
{
    CV_TRACE_FUNCTION();
#ifndef HAVE_OPENGL
    CV_UNUSED(windowName);
    CV_Error(cv::Error::OpenGlNotSupported, "The library is compiled without OpenGL support");
#else
    CV_UNUSED(windowName);
    CV_Error(cv::Error::StsNotImplemented, "The function is not implemented");
#endif
}

#ifdef HAVE_OPENGL
namespace
{
    std::map<cv::String, cv::ogl::Texture2D> wndTexs;
    std::map<cv::String, cv::ogl::Texture2D> ownWndTexs;
    std::map<cv::String, cv::ogl::Buffer> ownWndBufs;

    void glDrawTextureCallback(void* userdata)
    {
        cv::ogl::Texture2D* texObj = static_cast<cv::ogl::Texture2D*>(userdata);

        cv::ogl::render(*texObj);
    }
}
#endif // HAVE_OPENGL

void cv::imshow( const String& winname, InputArray _img )
{
    CV_TRACE_FUNCTION();

    const Size size = _img.size();
    CV_Assert(size.width>0 && size.height>0);
    {
        cv::AutoLock lock(cv::getWindowMutex());
        cleanupClosedWindows_();
        auto& windowsMap = getWindowsMap();
        auto i = windowsMap.find(winname);
        if (i != windowsMap.end())
        {
            auto ui_base = i->second;
            if (ui_base)
            {
                auto window = std::dynamic_pointer_cast<UIWindow>(ui_base);
                if (!window)
                {
                    CV_LOG_ERROR(NULL, "OpenCV/UI: invalid window name: '" << winname << "'");
                }
                return window->imshow(_img);
            }
        }
        auto backend = getCurrentUIBackend();
        if (backend)
        {
            auto window = backend->createWindow(winname, WINDOW_AUTOSIZE);
            if (!window)
            {
                CV_LOG_ERROR(NULL, "OpenCV/UI: Can't create window: '" << winname << "'");
                return;
            }
            windowsMap.emplace(winname, window);
            return window->imshow(_img);
        }
    }

    CV_Error(Error::StsError,
        "The function is not implemented. "
        "Rebuild the library with Windows, GTK+ 2.x or Cocoa support. "
        "If you are on Ubuntu or Debian, install libgtk2.0-dev and pkg-config, then re-run cmake or configure script");
}

void cv::imshow(const String& winname, const ogl::Texture2D& _tex)
{
    CV_TRACE_FUNCTION();
#ifndef HAVE_OPENGL
    CV_UNUSED(winname);
    CV_UNUSED(_tex);
    CV_Error(cv::Error::OpenGlNotSupported, "The library is compiled without OpenGL support");
#else
    const double useGl = getWindowProperty(winname, WND_PROP_OPENGL);

    if (useGl <= 0)
    {
        CV_Error(cv::Error::OpenGlNotSupported, "The window was created without OpenGL context");
    }
    else
    {
        const double autoSize = getWindowProperty(winname, WND_PROP_AUTOSIZE);

        if (autoSize > 0)
        {
            Size size = _tex.size();
            resizeWindow(winname, size.width, size.height);
        }

        setOpenGlContext(winname);

        cv::ogl::Texture2D& tex = wndTexs[winname];

        tex = _tex;

        tex.setAutoRelease(false);

        setOpenGlDrawCallback(winname, glDrawTextureCallback, &tex);

        updateWindow(winname);
    }
#endif
}

const std::string cv::currentUIFramework()
{
    CV_TRACE_FUNCTION();

    // plugin and backend-compatible implementations
    auto backend = getCurrentUIBackend();
    if (backend)
    {
        return backend->getName();
    }

    // builtin backends
#if defined(HAVE_WIN32UI)
    CV_Assert(false); // backend-compatible
#elif defined (HAVE_GTK)
    CV_Assert(false); // backend-compatible
#elif defined (HAVE_QT)
    return std::string("QT");
#elif defined (HAVE_COCOA)
    return std::string("COCOA");
#elif defined (HAVE_WAYLAND)
    return std::string("WAYLAND");
#endif

    return std::string();
}


static const char* NO_QT_ERR_MSG = "The library is compiled without QT support";

cv::QtFont cv::fontQt(const String&, int, Scalar, int,  int, int)
{
    CV_Error(cv::Error::StsNotImplemented, NO_QT_ERR_MSG);
}

void cv::addText( const Mat&, const String&, Point, const QtFont&)
{
    CV_Error(cv::Error::StsNotImplemented, NO_QT_ERR_MSG);
}

void cv::addText(const Mat&, const String&, Point, const String&, int, Scalar, int, int, int)
{
    CV_Error(cv::Error::StsNotImplemented, NO_QT_ERR_MSG);
}

void cv::displayStatusBar(const String&,  const String&, int)
{
    CV_Error(cv::Error::StsNotImplemented, NO_QT_ERR_MSG);
}

void cv::displayOverlay(const String&,  const String&, int )
{
    CV_Error(cv::Error::StsNotImplemented, NO_QT_ERR_MSG);
}

int cv::startLoop(int (*)(int argc, char *argv[]), int , char**)
{
    CV_Error(cv::Error::StsNotImplemented, NO_QT_ERR_MSG);
}

void cv::stopLoop()
{
    CV_Error(cv::Error::StsNotImplemented, NO_QT_ERR_MSG);
}

void cv::saveWindowParameters(const String&)
{
    CV_Error(cv::Error::StsNotImplemented, NO_QT_ERR_MSG);
}

void cv::loadWindowParameters(const String&)
{
    CV_Error(cv::Error::StsNotImplemented, NO_QT_ERR_MSG);
}

int cv::createButton(const String&, ButtonCallback, void*, int , bool )
{
    CV_Error(cv::Error::StsNotImplemented, NO_QT_ERR_MSG);
}

/* End of file. */
