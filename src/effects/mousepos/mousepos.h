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

#ifndef KWIN_MOUSEPOS_H
#define KWIN_MOUSEPOS_H

#include <kwineffects.h>

class QAction;

namespace KWin
{
class GLTexture;

class MousePosEffect
    : public Effect
{
    Q_OBJECT
    Q_PROPERTY(bool mousePolling READ isMousePolling)
public:
    MousePosEffect();
    virtual ~MousePosEffect();
    virtual void prePaintScreen(ScreenPrePaintData& data, std::chrono::milliseconds time);
    virtual void paintScreen(int mask, const QRegion& region, ScreenPaintData& data);
    virtual void postPaintScreen();
    virtual void reconfigure(ReconfigureFlags);
    virtual bool isActive() const;

    bool blocksDirectScanout() const override;

    // for properties
    bool isMousePolling() const {
        return m_mousePolling;
    }
protected:
    bool loadData();
private Q_SLOTS:
    void toggle();
    void slotMouseChanged(const QPoint& pos, const QPoint& old,
                              Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons,
                              Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers);
private:
    bool init();
    void loadTexture();
    QRect m_lastRect[2];
    QRect m_union;
    bool m_active, m_mousePolling;
    float m_angle;
    float m_angleBase;
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    QSize m_size[2];
#endif
    QAction* m_action;
    QScopedPointer<GLTexture> m_cursorTexture;
    Qt::KeyboardModifiers m_modifiers;
    bool m_inited;
    bool m_valid;
    GLShader* m_shader;
    float m_prevX, m_prevY;
};

} // namespace

#endif
