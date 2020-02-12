/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2010 Jorge Mata <matamax123@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "mousepos.h"

// KConfigSkeleton
#include "mouseposconfig.h"

#include <QAction>
#include <QPainter>
#include <QTime>
#include <QMatrix4x4>

#include <kwinconfig.h>
#include <kwinglutils.h>
#include <kwinxrenderutils.h>

#include <KGlobalAccel>
#include <KLocalizedString>

#include <math.h>

namespace KWin
{

MousePosEffect::MousePosEffect()
    : m_active(false)
    , m_inited(false)
    , m_valid(true)
{
    initConfig<MousePosConfig>();
    m_mousePolling = false;

    m_action = new QAction(this);
    m_action->setObjectName(QStringLiteral("MousePos"));
    m_action->setText(i18n("Paint cursor"));
    //KGlobalAccel::self()->setDefaultShortcut(m_action, QList<QKeySequence>());
    //KGlobalAccel::self()->setShortcut(m_action, QList<QKeySequence>());
    //effects->registerGlobalShortcut(QKeySequence(), m_action);

    connect(m_action, SIGNAL(triggered(bool)), this, SLOT(toggle()));

    connect(effects, SIGNAL(mouseChanged(QPoint,QPoint,Qt::MouseButtons,Qt::MouseButtons,Qt::KeyboardModifiers,Qt::KeyboardModifiers)),
                     SLOT(slotMouseChanged(QPoint,QPoint,Qt::MouseButtons,Qt::MouseButtons,Qt::KeyboardModifiers,Qt::KeyboardModifiers)));
    reconfigure(ReconfigureAll);
    if (!m_mousePolling) {
      effects->startMousePolling();
      m_mousePolling = true;
    }
}

MousePosEffect::~MousePosEffect()
{
    if (m_mousePolling)
        effects->stopMousePolling();
}

void MousePosEffect::reconfigure(ReconfigureFlags)
{
    MousePosConfig::self()->read();
}

void MousePosEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (m_active) {
        QTime t = QTime::currentTime();
        m_lastRect[0].moveCenter(cursorPos());
        m_lastRect[1].moveCenter(cursorPos());
        data.paint |= m_lastRect[0].adjusted(-1,-1,1,1);
    }
    effects->prePaintScreen(data, time);
}

bool MousePosEffect::loadData()
{
    m_inited = true;
    printf("load data!\n");

    m_shader = ShaderManager::instance()->generateShaderFromResources(ShaderTrait::MapTexture, QString("motion.vert"), QStringLiteral("motion.frag"));
    if (!m_shader->isValid()) {
        printf("failure.\n");
        qCCritical(KWINEFFECTS) << "The shader failed to load!";
        return false;
    }
    printf("shader loaded.\n");
    return true;
}

void MousePosEffect::paintScreen(int mask, const QRegion& region, ScreenPaintData& data)
{
    effects->paintScreen(mask, region, data);   // paint normal screen
    if (!m_active)
        return;
    auto c = xcbConnection();

    if (m_valid && !m_inited)
        m_valid = loadData();

    if ( effects->isOpenGLCompositing()) {
        QScopedPointer<xcb_xfixes_get_cursor_image_reply_t, QScopedPointerPodDeleter> cursor(
        xcb_xfixes_get_cursor_image_reply(c,
                                          xcb_xfixes_get_cursor_image_unchecked(c),
                                          nullptr));
    if (cursor.isNull()) {
        return;
    }       
            QImage img = QImage((uchar *) xcb_xfixes_get_cursor_image_cursor_image(cursor.data()), cursor->width, cursor->height,
                      QImage::Format_ARGB32_Premultiplied);
            
            if (img.isNull()) {
                return;
            }
            m_cursorTexture.reset(new GLTexture(img));
    // get cursor position in projection coordinates
    if (!m_cursorTexture) {
      return;
    }
    const QRect cursorRect(0, 0, m_cursorTexture->width(), m_cursorTexture->height());
    QMatrix4x4 mvp = data.projectionMatrix();
    mvp.translate(cursor->x-cursor->xhot, cursor->y-cursor->yhot);
    
    // handle transparence
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // paint texture in cursor offset
    if (MousePosConfig::motionBlur()) {
      m_cursorTexture->bind();
      ShaderBinder binder(m_shader);
      binder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, mvp);
      binder.shader()->setUniform("viewport",QVector2D(3840,2160));
      binder.shader()->setUniform("in_center",QVector2D(0.0f,0.0f));
      binder.shader()->setUniform("in_vel",QVector4D(((float)(cursor->x-cursor->xhot)-m_prevX)/(float)cursorRect.width()
      ,((float)(cursor->y-cursor->yhot)-m_prevY)/(float)cursorRect.height()
      ,0.0f,32.0f));
      //binder.shader()->setUniform("in_pos",QVector2D(cursor->x-cursor->xhot+16, cursor->y-cursor->yhot));
      binder.shader()->setUniform("in_pos",QVector2D(0,0));
      binder.shader()->setUniform("in_sizes",QVector2D(cursorRect.width(),cursorRect.height()));
      m_cursorTexture->render(QRegion(cursorRect), cursorRect);
      m_cursorTexture->unbind();
    } else {
      m_cursorTexture->bind();
      ShaderBinder obinder(ShaderTrait::MapTexture);
      obinder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, mvp);
      m_cursorTexture->render(QRegion(cursorRect), cursorRect);
      m_cursorTexture->unbind();
    }

    glDisable(GL_BLEND);
    m_cursorTexture.reset(NULL);
    
    m_prevX=cursor->x-cursor->xhot;
    m_prevY=cursor->y-cursor->yhot;
    m_lastRect[1]=m_lastRect[0];
    m_lastRect[0]=cursorRect;
    m_lastRect[0].moveTo(m_prevX,m_prevY);
    }
}

void MousePosEffect::postPaintScreen()
{
    if (m_lastRect[0]!=m_lastRect[1]) {
        //effects->addRepaint(m_lastRect[0].adjusted(-1,-1,1,1));
    }
    effects->postPaintScreen();
}

bool MousePosEffect::init()
{
    effects->makeOpenGLContextCurrent();
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
#else
#endif
    m_lastRect[0].moveCenter(cursorPos());
    m_lastRect[1].moveCenter(cursorPos());
    m_active = true;
    printf("init...\n");
    return true;
}

void MousePosEffect::toggle()
{
    m_mousePolling=true;
    m_active=true;
    printf("toggle\n");
/*
    if (m_mousePolling)
        return;

    if (m_active) {
        m_active = false;
    } else if (!init()) {
        return;
    }*/
    effects->addRepaint(m_lastRect[0].adjusted(-1,-1,1,1));
}

void MousePosEffect::slotMouseChanged(const QPoint&, const QPoint&,
                                        Qt::MouseButtons, Qt::MouseButtons,
                                        Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers)
{
    if (!m_mousePolling) // we didn't ask for it but maybe someone else did...
        return;
    if (!m_active && !init()) {
            return;
        }
        effects->addRepaint(m_lastRect[0].adjusted(-1,-1,1,1));
}

void MousePosEffect::loadTexture()
{
}

bool MousePosEffect::isActive() const
{
    return m_active;
}

} // namespace
