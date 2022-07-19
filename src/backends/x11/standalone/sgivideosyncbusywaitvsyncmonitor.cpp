/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sgivideosyncbusywaitvsyncmonitor.h"
#include "glxconvenience.h"
#include "logging.h"

#include "options.h"

#include <QX11Info>

namespace KWin
{

SGIVideoSyncBusyWaitVsyncMonitor *SGIVideoSyncBusyWaitVsyncMonitor::create(QObject *parent)
{
    const char *extensions = glXQueryExtensionsString(QX11Info::display(),
                                                      QX11Info::appScreen());
    if (!strstr(extensions, "GLX_SGI_video_sync")) {
        return nullptr; // GLX_SGI_video_sync is unsupported.
    }

    SGIVideoSyncBusyWaitVsyncMonitor *monitor = new SGIVideoSyncBusyWaitVsyncMonitor(parent);
    if (monitor->isValid()) {
        return monitor;
    }
    delete monitor;
    return nullptr;
}

SGIVideoSyncBusyWaitVsyncMonitorHelper::SGIVideoSyncBusyWaitVsyncMonitorHelper(QObject *parent)
    : QObject(parent)
{
    // Establish a new X11 connection to avoid locking up the main X11 connection.
    m_display = XOpenDisplay(DisplayString(QX11Info::display()));
    if (!m_display) {
        qCDebug(KWIN_X11STANDALONE) << "Failed to establish vsync monitor X11 connection";
        return;
    }

    Window rootWindow = DefaultRootWindow(m_display);

    const int attribs[] = {
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        0
    };

    GLXFBConfig config = chooseGlxFbConfig(m_display, attribs);
    if (!config) {
        qCDebug(KWIN_X11STANDALONE) << "Couldn't find any suitable FBConfig for vsync monitor";
        return;
    }

    XVisualInfo *visualInfo = glXGetVisualFromFBConfig(m_display, config);
    if (!visualInfo) {
        return;
    }

    Visual *visual = visualInfo->visual;
    const int depth = visualInfo->depth;
    XFree(visualInfo);

    Colormap colormap = XCreateColormap(m_display, rootWindow, visual, AllocNone);
    XSetWindowAttributes attributes;
    attributes.colormap = colormap;

    m_dummyWindow = XCreateWindow(m_display, rootWindow, options->syncWindowX(), options->syncWindowY(), 1, 1, 0, depth,
                                  InputOutput, visual, CWColormap, &attributes);
    XFreeColormap(m_display, colormap);
    if (!m_dummyWindow) {
        qCDebug(KWIN_X11STANDALONE) << "Failed to create a dummy window for vsync monitor";
        return;
    }

    m_drawable = glXCreateWindow(m_display, config, m_dummyWindow, nullptr);
    if (!m_drawable) {
        qCDebug(KWIN_X11STANDALONE) << "Failed to create GLXWindow for dummy window";
        return;
    }

    m_localContext = glXCreateNewContext(m_display, config, GLX_RGBA_TYPE, 0, true);
    if (!m_localContext) {
        qCDebug(KWIN_X11STANDALONE) << "Failed to create opengl context for vsync monitor";
        return;
    }
}

SGIVideoSyncBusyWaitVsyncMonitorHelper::~SGIVideoSyncBusyWaitVsyncMonitorHelper()
{
    if (m_localContext) {
        glXDestroyContext(m_display, m_localContext);
    }
    if (m_drawable) {
        glXDestroyWindow(m_display, m_drawable);
    }
    if (m_dummyWindow) {
        XDestroyWindow(m_display, m_dummyWindow);
    }
    if (m_display) {
        XCloseDisplay(m_display);
    }
}

bool SGIVideoSyncBusyWaitVsyncMonitorHelper::isValid() const
{
    return m_display && m_localContext && m_drawable;
}

void SGIVideoSyncBusyWaitVsyncMonitorHelper::poll()
{
    if (!glXMakeCurrent(m_display, m_drawable, m_localContext)) {
        qCDebug(KWIN_X11STANDALONE) << "Failed to make vsync monitor OpenGL context current";
        Q_EMIT errorOccurred();
        return;
    }

    uint count;
    uint icount;

    glXGetVideoSyncSGI(&icount);

    while (icount==count) {
      QThread::usleep(1000);
      glXGetVideoSyncSGI(&count);
    }

    // Using monotonic clock is inaccurate, but it's still a pretty good estimate.
    Q_EMIT vblankOccurred(std::chrono::steady_clock::now().time_since_epoch());
}

SGIVideoSyncBusyWaitVsyncMonitor::SGIVideoSyncBusyWaitVsyncMonitor(QObject *parent)
    : VsyncMonitor(parent)
    , m_thread(new QThread)
    , m_helper(new SGIVideoSyncBusyWaitVsyncMonitorHelper)
{
    m_helper->moveToThread(m_thread);

    connect(m_helper, &SGIVideoSyncBusyWaitVsyncMonitorHelper::errorOccurred,
            this, &SGIVideoSyncBusyWaitVsyncMonitor::errorOccurred);
    connect(m_helper, &SGIVideoSyncBusyWaitVsyncMonitorHelper::vblankOccurred,
            this, &SGIVideoSyncBusyWaitVsyncMonitor::vblankOccurred);

    m_thread->setObjectName(QStringLiteral("vsync event monitor"));
    m_thread->start();
}

SGIVideoSyncBusyWaitVsyncMonitor::~SGIVideoSyncBusyWaitVsyncMonitor()
{
    m_thread->quit();
    m_thread->wait();

    delete m_helper;
    delete m_thread;
}

bool SGIVideoSyncBusyWaitVsyncMonitor::isValid() const
{
    return m_helper->isValid();
}

void SGIVideoSyncBusyWaitVsyncMonitor::arm()
{
    QMetaObject::invokeMethod(m_helper, &SGIVideoSyncBusyWaitVsyncMonitorHelper::poll);
}

} // namespace KWin
