/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "effects.h"

#include "abstract_output.h"
#include "effectsadaptor.h"
#include "effectloader.h"
#ifdef KWIN_BUILD_ACTIVITIES
#include "activities.h"
#endif
#include "deleted.h"
#include "x11client.h"
#include "cursor.h"
#include "group.h"
#include "internal_client.h"
#include "osd.h"
#include "pointer_input.h"
#include "renderbackend.h"
#include "unmanaged.h"
#ifdef KWIN_BUILD_TABBOX
#include "tabbox.h"
#endif
#include "screenedge.h"
#include "scripting/scriptedeffect.h"
#include "screens.h"
#include "screenlockerwatcher.h"
#include "virtualdesktops.h"
#include "window_property_notify_x11_filter.h"
#include "workspace.h"
#include "kwinglutils.h"
#include "kwinoffscreenquickview.h"

#include <QDebug>
#include <QMouseEvent>
#include <QWheelEvent>

#include <Plasma/Theme>

#include "composite.h"
#include "platform.h"
#include "utils/xcbutils.h"
#include "waylandclient.h"
#include "wayland_server.h"

#include "decorations/decorationbridge.h"
#include <KDecoration2/DecorationSettings>

namespace KWin
{
//---------------------
// Static

static QByteArray readWindowProperty(xcb_window_t win, xcb_atom_t atom, xcb_atom_t type, int format)
{
    if (win == XCB_WINDOW_NONE) {
        return QByteArray();
    }
    uint32_t len = 32768;
    for (;;) {
        Xcb::Property prop(false, win, atom, XCB_ATOM_ANY, 0, len);
        if (prop.isNull()) {
            // get property failed
            return QByteArray();
        }
        if (prop->bytes_after > 0) {
            len *= 2;
            continue;
        }
        return prop.toByteArray(format, type);
    }
}

static void deleteWindowProperty(xcb_window_t win, long int atom)
{
    if (win == XCB_WINDOW_NONE) {
        return;
    }
    xcb_delete_property(kwinApp()->x11Connection(), win, atom);
}

static xcb_atom_t registerSupportProperty(const QByteArray &propertyName)
{
    auto c = kwinApp()->x11Connection();
    if (!c) {
        return XCB_ATOM_NONE;
    }
    // get the atom for the propertyName
    ScopedCPointer<xcb_intern_atom_reply_t> atomReply(xcb_intern_atom_reply(c,
        xcb_intern_atom_unchecked(c, false, propertyName.size(), propertyName.constData()),
        nullptr));
    if (atomReply.isNull()) {
        return XCB_ATOM_NONE;
    }
    // announce property on root window
    unsigned char dummy = 0;
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, kwinApp()->x11RootWindow(), atomReply->atom, atomReply->atom, 8, 1, &dummy);
    // TODO: add to _NET_SUPPORTED
    return atomReply->atom;
}

//---------------------

EffectsHandlerImpl::EffectsHandlerImpl(Compositor *compositor, Scene *scene)
    : EffectsHandler(Compositor::self()->backend()->compositingType())
    , keyboard_grab_effect(nullptr)
    , fullscreen_effect(nullptr)
    , m_compositor(compositor)
    , m_scene(scene)
    , m_desktopRendering(false)
    , m_currentRenderedDesktop(0)
    , m_effectLoader(new EffectLoader(this))
    , m_trackingCursorChanges(0)
{
    qRegisterMetaType<QVector<KWin::EffectWindow*>>();
    connect(m_effectLoader, &AbstractEffectLoader::effectLoaded, this,
        [this](Effect *effect, const QString &name) {
            effect_order.insert(effect->requestedEffectChainPosition(), EffectPair(name, effect));
            loaded_effects << EffectPair(name, effect);
            effectsChanged();
        }
    );
    m_effectLoader->setConfig(kwinApp()->config());
    new EffectsAdaptor(this);
    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.registerObject(QStringLiteral("/Effects"), this);

    Workspace *ws = Workspace::self();
    VirtualDesktopManager *vds = VirtualDesktopManager::self();
    connect(ws, &Workspace::showingDesktopChanged,
            this, &EffectsHandlerImpl::showingDesktopChanged);
    connect(ws, &Workspace::currentDesktopChanged, this,
        [this](int old, AbstractClient *c) {
            const int newDesktop = VirtualDesktopManager::self()->current();
            if (old != 0 && newDesktop != old) {
                Q_EMIT desktopChanged(old, newDesktop, c ? c->effectWindow() : nullptr);
                // TODO: remove in 4.10
                Q_EMIT desktopChanged(old, newDesktop);
            }
        }
    );
    connect(ws, &Workspace::desktopPresenceChanged, this,
        [this](AbstractClient *c, int old) {
            if (!c->effectWindow()) {
                return;
            }
            Q_EMIT desktopPresenceChanged(c->effectWindow(), old, c->desktop());
        }
    );
    connect(ws, &Workspace::clientAdded, this,
        [this](AbstractClient *c) {
            if (c->readyForPainting())
                slotClientShown(c);
            else
                connect(c, &Toplevel::windowShown, this, &EffectsHandlerImpl::slotClientShown);
        }
    );
    connect(ws, &Workspace::unmanagedAdded, this,
        [this](Unmanaged *u) {
            // it's never initially ready but has synthetic 50ms delay
            connect(u, &Toplevel::windowShown, this, &EffectsHandlerImpl::slotUnmanagedShown);
        }
    );
    connect(ws, &Workspace::internalClientAdded, this,
        [this](InternalClient *client) {
            setupClientConnections(client);
            Q_EMIT windowAdded(client->effectWindow());
        }
    );
    connect(ws, &Workspace::clientActivated, this,
        [this](KWin::AbstractClient *c) {
            Q_EMIT windowActivated(c ? c->effectWindow() : nullptr);
        }
    );
    connect(ws, &Workspace::deletedRemoved, this,
        [this](KWin::Deleted *d) {
            Q_EMIT windowDeleted(d->effectWindow());
            elevated_windows.removeAll(d->effectWindow());
        }
    );
    connect(ws->sessionManager(), &SessionManager::stateChanged, this,
            &KWin::EffectsHandler::sessionStateChanged);
    connect(vds, &VirtualDesktopManager::countChanged, this, &EffectsHandler::numberDesktopsChanged);
    connect(Cursors::self()->mouse(), &Cursor::mouseChanged, this, &EffectsHandler::mouseChanged);
    connect(Screens::self(), &Screens::sizeChanged, this, &EffectsHandler::virtualScreenSizeChanged);
    connect(Screens::self(), &Screens::geometryChanged, this, &EffectsHandler::virtualScreenGeometryChanged);
#ifdef KWIN_BUILD_ACTIVITIES
    if (Activities *activities = Activities::self()) {
        connect(activities, &Activities::added,          this, &EffectsHandler::activityAdded);
        connect(activities, &Activities::removed,        this, &EffectsHandler::activityRemoved);
        connect(activities, &Activities::currentChanged, this, &EffectsHandler::currentActivityChanged);
    }
#endif
    connect(ws, &Workspace::stackingOrderChanged, this, &EffectsHandler::stackingOrderChanged);
#ifdef KWIN_BUILD_TABBOX
    TabBox::TabBox *tabBox = TabBox::TabBox::self();
    connect(tabBox, &TabBox::TabBox::tabBoxAdded,    this, &EffectsHandler::tabBoxAdded);
    connect(tabBox, &TabBox::TabBox::tabBoxUpdated,  this, &EffectsHandler::tabBoxUpdated);
    connect(tabBox, &TabBox::TabBox::tabBoxClosed,   this, &EffectsHandler::tabBoxClosed);
    connect(tabBox, &TabBox::TabBox::tabBoxKeyEvent, this, &EffectsHandler::tabBoxKeyEvent);
#endif
    connect(ScreenEdges::self(), &ScreenEdges::approaching, this, &EffectsHandler::screenEdgeApproaching);
    connect(ScreenLockerWatcher::self(), &ScreenLockerWatcher::locked, this, &EffectsHandler::screenLockingChanged);
    connect(ScreenLockerWatcher::self(), &ScreenLockerWatcher::aboutToLock, this, &EffectsHandler::screenAboutToLock);

    connect(kwinApp(), &Application::x11ConnectionChanged, this,
        [this] {
            registered_atoms.clear();
            for (auto it = m_propertiesForEffects.keyBegin(); it != m_propertiesForEffects.keyEnd(); it++) {
                const auto atom = registerSupportProperty(*it);
                if (atom == XCB_ATOM_NONE) {
                    continue;
                }
                m_compositor->keepSupportProperty(atom);
                m_managedProperties.insert(*it, atom);
                registerPropertyType(atom, true);
            }
            if (kwinApp()->x11Connection()) {
                m_x11WindowPropertyNotify = std::make_unique<WindowPropertyNotifyX11Filter>(this);
            } else {
                m_x11WindowPropertyNotify.reset();
            }
            Q_EMIT xcbConnectionChanged();
        }
    );

    if (kwinApp()->x11Connection()) {
        m_x11WindowPropertyNotify = std::make_unique<WindowPropertyNotifyX11Filter>(this);
    }

    // connect all clients
    for (AbstractClient *client : ws->allClientList()) {
        if (client->readyForPainting()) {
            setupClientConnections(client);
        } else {
            connect(client, &Toplevel::windowShown, this, &EffectsHandlerImpl::slotClientShown);
        }
    }
    for (Unmanaged *u : ws->unmanagedList()) {
        setupUnmanagedConnections(u);
    }
    for (InternalClient *client : ws->internalClients()) {
        setupClientConnections(client);
    }

    connect(kwinApp()->platform(), &Platform::outputEnabled, this, &EffectsHandlerImpl::slotOutputEnabled);
    connect(kwinApp()->platform(), &Platform::outputDisabled, this, &EffectsHandlerImpl::slotOutputDisabled);

    const QVector<AbstractOutput *> outputs = kwinApp()->platform()->enabledOutputs();
    for (AbstractOutput *output : outputs) {
        slotOutputEnabled(output);
    }

    reconfigure();
}

EffectsHandlerImpl::~EffectsHandlerImpl()
{
    unloadAllEffects();
}

void EffectsHandlerImpl::unloadAllEffects()
{
    for (const EffectPair &pair : qAsConst(loaded_effects)) {
        destroyEffect(pair.second);
    }

    effect_order.clear();
    m_effectLoader->clear();

    effectsChanged();
}

void EffectsHandlerImpl::setupClientConnections(AbstractClient* c)
{
    connect(c, &AbstractClient::windowClosed, this, &EffectsHandlerImpl::slotWindowClosed);
    connect(c, static_cast<void (AbstractClient::*)(KWin::AbstractClient*, MaximizeMode)>(&AbstractClient::clientMaximizedStateChanged),
            this, &EffectsHandlerImpl::slotClientMaximized);
    connect(c, &AbstractClient::clientStartUserMovedResized, this,
        [this](AbstractClient *c) {
            Q_EMIT windowStartUserMovedResized(c->effectWindow());
        }
    );
    connect(c, &AbstractClient::clientStepUserMovedResized, this,
        [this](AbstractClient *c, const QRect &geometry) {
            Q_EMIT windowStepUserMovedResized(c->effectWindow(), geometry);
        }
    );
    connect(c, &AbstractClient::clientFinishUserMovedResized, this,
        [this](AbstractClient *c) {
            Q_EMIT windowFinishUserMovedResized(c->effectWindow());
        }
    );
    connect(c, &AbstractClient::opacityChanged, this, &EffectsHandlerImpl::slotOpacityChanged);
    connect(c, &AbstractClient::clientMinimized, this,
        [this](AbstractClient *c, bool animate) {
            // TODO: notify effects even if it should not animate?
            if (animate) {
                Q_EMIT windowMinimized(c->effectWindow());
            }
        }
    );
    connect(c, &AbstractClient::clientUnminimized, this,
        [this](AbstractClient* c, bool animate) {
            // TODO: notify effects even if it should not animate?
            if (animate) {
                Q_EMIT windowUnminimized(c->effectWindow());
            }
        }
    );
    connect(c, &AbstractClient::modalChanged,         this, &EffectsHandlerImpl::slotClientModalityChanged);
    connect(c, &AbstractClient::geometryShapeChanged, this, &EffectsHandlerImpl::slotGeometryShapeChanged);
    connect(c, &AbstractClient::frameGeometryChanged, this, &EffectsHandlerImpl::slotFrameGeometryChanged);
    connect(c, &AbstractClient::damaged,              this, &EffectsHandlerImpl::slotWindowDamaged);
    connect(c, &AbstractClient::unresponsiveChanged, this,
        [this, c](bool unresponsive) {
            Q_EMIT windowUnresponsiveChanged(c->effectWindow(), unresponsive);
        }
    );
    connect(c, &AbstractClient::windowShown, this,
        [this](Toplevel *c) {
            Q_EMIT windowShown(c->effectWindow());
        }
    );
    connect(c, &AbstractClient::windowHidden, this,
        [this](Toplevel *c) {
            Q_EMIT windowHidden(c->effectWindow());
        }
    );
    connect(c, &AbstractClient::keepAboveChanged, this,
        [this, c](bool above) {
            Q_UNUSED(above)
            Q_EMIT windowKeepAboveChanged(c->effectWindow());
        }
    );
    connect(c, &AbstractClient::keepBelowChanged, this,
        [this, c](bool below) {
            Q_UNUSED(below)
            Q_EMIT windowKeepBelowChanged(c->effectWindow());
        }
    );
    connect(c, &AbstractClient::fullScreenChanged, this,
        [this, c]() {
            Q_EMIT windowFullScreenChanged(c->effectWindow());
        }
    );
    connect(c, &AbstractClient::visibleGeometryChanged, this, [this, c]() {
        Q_EMIT windowExpandedGeometryChanged(c->effectWindow());
    });
}

void EffectsHandlerImpl::setupUnmanagedConnections(Unmanaged* u)
{
    connect(u, &Unmanaged::windowClosed,         this, &EffectsHandlerImpl::slotWindowClosed);
    connect(u, &Unmanaged::opacityChanged,       this, &EffectsHandlerImpl::slotOpacityChanged);
    connect(u, &Unmanaged::geometryShapeChanged, this, &EffectsHandlerImpl::slotGeometryShapeChanged);
    connect(u, &Unmanaged::frameGeometryChanged, this, &EffectsHandlerImpl::slotFrameGeometryChanged);
    connect(u, &Unmanaged::damaged,              this, &EffectsHandlerImpl::slotWindowDamaged);
    connect(u, &Unmanaged::visibleGeometryChanged, this, [this, u]() {
        Q_EMIT windowExpandedGeometryChanged(u->effectWindow());
    });
}

void EffectsHandlerImpl::reconfigure()
{
    m_effectLoader->queryAndLoadAll();
}

// the idea is that effects call this function again which calls the next one
void EffectsHandlerImpl::prePaintScreen(ScreenPrePaintData& data, std::chrono::milliseconds presentTime)
{
    if (m_currentPaintScreenIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintScreenIterator++)->prePaintScreen(data, presentTime);
        --m_currentPaintScreenIterator;
    }
    // no special final code
}

void EffectsHandlerImpl::paintScreen(int mask, const QRegion &region, ScreenPaintData& data)
{
    if (m_currentPaintScreenIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintScreenIterator++)->paintScreen(mask, region, data);
        --m_currentPaintScreenIterator;
    } else
        m_scene->finalPaintScreen(mask, region, data);
}

void EffectsHandlerImpl::paintDesktop(int desktop, int mask, QRegion region, ScreenPaintData &data)
{
    if (desktop < 1 || desktop > numberOfDesktops()) {
        return;
    }
    m_currentRenderedDesktop = desktop;
    m_desktopRendering = true;
    // save the paint screen iterator
    EffectsIterator savedIterator = m_currentPaintScreenIterator;
    m_currentPaintScreenIterator = m_activeEffects.constBegin();
    effects->paintScreen(mask, region, data);
    // restore the saved iterator
    m_currentPaintScreenIterator = savedIterator;
    m_desktopRendering = false;
}

void EffectsHandlerImpl::postPaintScreen()
{
    if (m_currentPaintScreenIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintScreenIterator++)->postPaintScreen();
        --m_currentPaintScreenIterator;
    }
    // no special final code
}

void EffectsHandlerImpl::prePaintWindow(EffectWindow* w, WindowPrePaintData& data, std::chrono::milliseconds presentTime)
{
    if (m_currentPaintWindowIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintWindowIterator++)->prePaintWindow(w, data, presentTime);
        --m_currentPaintWindowIterator;
    }
    // no special final code
}

void EffectsHandlerImpl::paintWindow(EffectWindow* w, int mask, const QRegion &region, WindowPaintData& data)
{
    if (m_currentPaintWindowIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintWindowIterator++)->paintWindow(w, mask, region, data);
        --m_currentPaintWindowIterator;
    } else
        m_scene->finalPaintWindow(static_cast<EffectWindowImpl*>(w), mask, region, data);
}

void EffectsHandlerImpl::paintEffectFrame(EffectFrame* frame, const QRegion &region, double opacity, double frameOpacity)
{
    if (m_currentPaintEffectFrameIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintEffectFrameIterator++)->paintEffectFrame(frame, region, opacity, frameOpacity);
        --m_currentPaintEffectFrameIterator;
    } else {
        const EffectFrameImpl* frameImpl = static_cast<const EffectFrameImpl*>(frame);
        frameImpl->finalRender(region, opacity, frameOpacity);
    }
}

void EffectsHandlerImpl::postPaintWindow(EffectWindow* w)
{
    if (m_currentPaintWindowIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintWindowIterator++)->postPaintWindow(w);
        --m_currentPaintWindowIterator;
    }
    // no special final code
}

Effect *EffectsHandlerImpl::provides(Effect::Feature ef)
{
    for (int i = 0; i < loaded_effects.size(); ++i)
        if (loaded_effects.at(i).second->provides(ef))
            return loaded_effects.at(i).second;
    return nullptr;
}

void EffectsHandlerImpl::drawWindow(EffectWindow* w, int mask, const QRegion &region, WindowPaintData& data)
{
    if (m_currentDrawWindowIterator != m_activeEffects.constEnd()) {
        (*m_currentDrawWindowIterator++)->drawWindow(w, mask, region, data);
        --m_currentDrawWindowIterator;
    } else
        m_scene->finalDrawWindow(static_cast<EffectWindowImpl*>(w), mask, region, data);
}

bool EffectsHandlerImpl::hasDecorationShadows() const
{
    return false;
}

bool EffectsHandlerImpl::decorationsHaveAlpha() const
{
    return true;
}

bool EffectsHandlerImpl::decorationSupportsBlurBehind() const
{
    return Decoration::DecorationBridge::self()->needsBlur();
}

// start another painting pass
void EffectsHandlerImpl::startPaint()
{
    m_activeEffects.clear();
    m_activeEffects.reserve(loaded_effects.count());
    for(QVector< KWin::EffectPair >::const_iterator it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if (it->second->isActive()) {
            m_activeEffects << it->second;
        }
    }
    m_currentDrawWindowIterator = m_activeEffects.constBegin();
    m_currentPaintWindowIterator = m_activeEffects.constBegin();
    m_currentPaintScreenIterator = m_activeEffects.constBegin();
    m_currentPaintEffectFrameIterator = m_activeEffects.constBegin();
}

void EffectsHandlerImpl::slotClientMaximized(KWin::AbstractClient *c, MaximizeMode maxMode)
{
    bool horizontal = false;
    bool vertical = false;
    switch (maxMode) {
    case MaximizeHorizontal:
        horizontal = true;
        break;
    case MaximizeVertical:
        vertical = true;
        break;
    case MaximizeFull:
        horizontal = true;
        vertical = true;
        break;
    case MaximizeRestore: // fall through
    default:
        // default - nothing to do
        break;
    }
    if (EffectWindowImpl *w = c->effectWindow()) {
        Q_EMIT windowMaximizedStateChanged(w, horizontal, vertical);
    }
}

void EffectsHandlerImpl::slotOpacityChanged(Toplevel *t, qreal oldOpacity)
{
    if (t->opacity() == oldOpacity || !t->effectWindow()) {
        return;
    }
    Q_EMIT windowOpacityChanged(t->effectWindow(), oldOpacity, (qreal)t->opacity());
}

void EffectsHandlerImpl::slotClientShown(KWin::Toplevel *t)
{
    Q_ASSERT(qobject_cast<AbstractClient *>(t));
    AbstractClient *c = static_cast<AbstractClient *>(t);
    disconnect(c, &Toplevel::windowShown, this, &EffectsHandlerImpl::slotClientShown);
    setupClientConnections(c);
    Q_EMIT windowAdded(c->effectWindow());
}

void EffectsHandlerImpl::slotUnmanagedShown(KWin::Toplevel *t)
{   // regardless, unmanaged windows are -yet?- not synced anyway
    Q_ASSERT(qobject_cast<Unmanaged *>(t));
    Unmanaged *u = static_cast<Unmanaged*>(t);
    setupUnmanagedConnections(u);
    Q_EMIT windowAdded(u->effectWindow());
}

void EffectsHandlerImpl::slotWindowClosed(KWin::Toplevel *c, KWin::Deleted *d)
{
    c->disconnect(this);
    if (d) {
        Q_EMIT windowClosed(c->effectWindow());
    }
}

void EffectsHandlerImpl::slotClientModalityChanged()
{
    Q_EMIT windowModalityChanged(static_cast<X11Client *>(sender())->effectWindow());
}

void EffectsHandlerImpl::slotCurrentTabAboutToChange(EffectWindow *from, EffectWindow *to)
{
    Q_EMIT currentTabAboutToChange(from, to);
}

void EffectsHandlerImpl::slotTabAdded(EffectWindow* w, EffectWindow* to)
{
    Q_EMIT tabAdded(w, to);
}

void EffectsHandlerImpl::slotTabRemoved(EffectWindow *w, EffectWindow* leaderOfFormerGroup)
{
    Q_EMIT tabRemoved(w, leaderOfFormerGroup);
}

void EffectsHandlerImpl::slotWindowDamaged(Toplevel* t, const QRegion& r)
{
    if (!t->effectWindow()) {
        // can happen during tear down of window
        return;
    }
    Q_EMIT windowDamaged(t->effectWindow(), r);
}

void EffectsHandlerImpl::slotGeometryShapeChanged(Toplevel* t, const QRect& old)
{
    // during late cleanup effectWindow() may be already NULL
    // in some functions that may still call this
    if (t == nullptr || t->effectWindow() == nullptr)
        return;
    Q_EMIT windowGeometryShapeChanged(t->effectWindow(), old);
}

void EffectsHandlerImpl::slotFrameGeometryChanged(Toplevel *toplevel, const QRect &oldGeometry)
{
    // effectWindow() might be nullptr during tear down of the client.
    if (toplevel->effectWindow()) {
        Q_EMIT windowFrameGeometryChanged(toplevel->effectWindow(), oldGeometry);
    }
}

void EffectsHandlerImpl::setActiveFullScreenEffect(Effect* e)
{
    if (fullscreen_effect == e) {
        return;
    }
    const bool activeChanged = (e == nullptr || fullscreen_effect == nullptr);
    fullscreen_effect = e;
    Q_EMIT activeFullScreenEffectChanged();
    if (activeChanged) {
        Q_EMIT hasActiveFullScreenEffectChanged();
    }
}

Effect* EffectsHandlerImpl::activeFullScreenEffect() const
{
    return fullscreen_effect;
}

bool EffectsHandlerImpl::hasActiveFullScreenEffect() const
{
    return fullscreen_effect;
}

bool EffectsHandlerImpl::grabKeyboard(Effect* effect)
{
    if (keyboard_grab_effect != nullptr)
        return false;
    if (!doGrabKeyboard()) {
        return false;
    }
    keyboard_grab_effect = effect;
    return true;
}

bool EffectsHandlerImpl::doGrabKeyboard()
{
    return true;
}

void EffectsHandlerImpl::ungrabKeyboard()
{
    Q_ASSERT(keyboard_grab_effect != nullptr);
    doUngrabKeyboard();
    keyboard_grab_effect = nullptr;
}

void EffectsHandlerImpl::doUngrabKeyboard()
{
}

void EffectsHandlerImpl::grabbedKeyboardEvent(QKeyEvent* e)
{
    if (keyboard_grab_effect != nullptr)
        keyboard_grab_effect->grabbedKeyboardEvent(e);
}

void EffectsHandlerImpl::startMouseInterception(Effect *effect, Qt::CursorShape shape)
{
    if (m_grabbedMouseEffects.contains(effect)) {
        return;
    }
    m_grabbedMouseEffects.append(effect);
    if (m_grabbedMouseEffects.size() != 1) {
        return;
    }
    doStartMouseInterception(shape);
}

void EffectsHandlerImpl::doStartMouseInterception(Qt::CursorShape shape)
{
    input()->pointer()->setEffectsOverrideCursor(shape);
}

void EffectsHandlerImpl::stopMouseInterception(Effect *effect)
{
    if (!m_grabbedMouseEffects.contains(effect)) {
        return;
    }
    m_grabbedMouseEffects.removeAll(effect);
    if (m_grabbedMouseEffects.isEmpty()) {
        doStopMouseInterception();
    }
}

void EffectsHandlerImpl::doStopMouseInterception()
{
    input()->pointer()->removeEffectsOverrideCursor();
}

bool EffectsHandlerImpl::isMouseInterception() const
{
    return m_grabbedMouseEffects.count() > 0;
}


bool EffectsHandlerImpl::touchDown(qint32 id, const QPointF &pos, quint32 time)
{
    // TODO: reverse call order?
    for (auto it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if (it->second->touchDown(id, pos, time)) {
            return true;
        }
    }
    return false;
}

bool EffectsHandlerImpl::touchMotion(qint32 id, const QPointF &pos, quint32 time)
{
    // TODO: reverse call order?
    for (auto it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if (it->second->touchMotion(id, pos, time)) {
            return true;
        }
    }
    return false;
}

bool EffectsHandlerImpl::touchUp(qint32 id, quint32 time)
{
    // TODO: reverse call order?
    for (auto it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if (it->second->touchUp(id, time)) {
            return true;
        }
    }
    return false;
}

void EffectsHandlerImpl::registerGlobalShortcut(const QKeySequence &shortcut, QAction *action)
{
    input()->registerShortcut(shortcut, action);
}

void EffectsHandlerImpl::registerPointerShortcut(Qt::KeyboardModifiers modifiers, Qt::MouseButton pointerButtons, QAction *action)
{
    input()->registerPointerShortcut(modifiers, pointerButtons, action);
}

void EffectsHandlerImpl::registerAxisShortcut(Qt::KeyboardModifiers modifiers, PointerAxisDirection axis, QAction *action)
{
    input()->registerAxisShortcut(modifiers, axis, action);
}

void EffectsHandlerImpl::registerRealtimeTouchpadSwipeShortcut(SwipeDirection dir, QAction* onUp, std::function<void(qreal)> progressCallback)
{
    input()->registerRealtimeTouchpadSwipeShortcut(dir, onUp, progressCallback);
}

void EffectsHandlerImpl::registerTouchpadSwipeShortcut(SwipeDirection direction, QAction *action)
{
    input()->registerTouchpadSwipeShortcut(direction, action);
}

void* EffectsHandlerImpl::getProxy(QString name)
{
    for (QVector< EffectPair >::const_iterator it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it)
        if ((*it).first == name)
            return (*it).second->proxy();

    return nullptr;
}

void EffectsHandlerImpl::startMousePolling()
{
    if (Cursors::self()->mouse())
        Cursors::self()->mouse()->startMousePolling();
}

void EffectsHandlerImpl::stopMousePolling()
{
    if (Cursors::self()->mouse())
        Cursors::self()->mouse()->stopMousePolling();
}

bool EffectsHandlerImpl::hasKeyboardGrab() const
{
    return keyboard_grab_effect != nullptr;
}

void EffectsHandlerImpl::registerPropertyType(long atom, bool reg)
{
    if (reg)
        ++registered_atoms[ atom ]; // initialized to 0 if not present yet
    else {
        if (--registered_atoms[ atom ] == 0)
            registered_atoms.remove(atom);
    }
}

xcb_atom_t EffectsHandlerImpl::announceSupportProperty(const QByteArray &propertyName, Effect *effect)
{
    PropertyEffectMap::iterator it = m_propertiesForEffects.find(propertyName);
    if (it != m_propertiesForEffects.end()) {
        // property has already been registered for an effect
        // just append Effect and return the atom stored in m_managedProperties
        if (!it.value().contains(effect)) {
            it.value().append(effect);
        }
        return m_managedProperties.value(propertyName, XCB_ATOM_NONE);
    }
    m_propertiesForEffects.insert(propertyName, QList<Effect*>() << effect);
    const auto atom = registerSupportProperty(propertyName);
    if (atom == XCB_ATOM_NONE) {
        return atom;
    }
    m_compositor->keepSupportProperty(atom);
    m_managedProperties.insert(propertyName, atom);
    registerPropertyType(atom, true);
    return atom;
}

void EffectsHandlerImpl::removeSupportProperty(const QByteArray &propertyName, Effect *effect)
{
    PropertyEffectMap::iterator it = m_propertiesForEffects.find(propertyName);
    if (it == m_propertiesForEffects.end()) {
        // property is not registered - nothing to do
        return;
    }
    if (!it.value().contains(effect)) {
        // property is not registered for given effect - nothing to do
        return;
    }
    it.value().removeAll(effect);
    if (!it.value().isEmpty()) {
        // property still registered for another effect - nothing further to do
        return;
    }
    const xcb_atom_t atom = m_managedProperties.take(propertyName);
    registerPropertyType(atom, false);
    m_propertiesForEffects.remove(propertyName);
    m_compositor->removeSupportProperty(atom); // delayed removal
}

QByteArray EffectsHandlerImpl::readRootProperty(long atom, long type, int format) const
{
    if (!kwinApp()->x11Connection()) {
        return QByteArray();
    }
    return readWindowProperty(kwinApp()->x11RootWindow(), atom, type, format);
}

void EffectsHandlerImpl::activateWindow(EffectWindow* c)
{
    if (auto cl = qobject_cast<AbstractClient *>(static_cast<EffectWindowImpl *>(c)->window())) {
        Workspace::self()->activateClient(cl, true);
    }
}

EffectWindow* EffectsHandlerImpl::activeWindow() const
{
    return Workspace::self()->activeClient() ? Workspace::self()->activeClient()->effectWindow() : nullptr;
}

void EffectsHandlerImpl::moveWindow(EffectWindow* w, const QPoint& pos, bool snap, double snapAdjust)
{
    auto cl = qobject_cast<AbstractClient *>(static_cast<EffectWindowImpl *>(w)->window());
    if (!cl || !cl->isMovable())
        return;

    if (snap)
        cl->move(Workspace::self()->adjustClientPosition(cl, pos, true, snapAdjust));
    else
        cl->move(pos);
}

void EffectsHandlerImpl::windowToDesktop(EffectWindow* w, int desktop)
{
    auto cl = qobject_cast<AbstractClient *>(static_cast<EffectWindowImpl *>(w)->window());
    if (cl && !cl->isDesktop() && !cl->isDock()) {
        Workspace::self()->sendClientToDesktop(cl, desktop, true);
    }
}

void EffectsHandlerImpl::windowToDesktops(EffectWindow *w, const QVector<uint> &desktopIds)
{
    AbstractClient* cl = qobject_cast< AbstractClient* >(static_cast<EffectWindowImpl*>(w)->window());
    if (!cl || cl->isDesktop() || cl->isDock()) {
        return;
    }
    QVector<VirtualDesktop*> desktops;
    desktops.reserve(desktopIds.count());
    for (uint x11Id: desktopIds) {
        if (x11Id > VirtualDesktopManager::self()->count()) {
            continue;
        }
        VirtualDesktop *d = VirtualDesktopManager::self()->desktopForX11Id(x11Id);
        Q_ASSERT(d);
        if (desktops.contains(d)) {
            continue;
        }
        desktops << d;
    }
    cl->setDesktops(desktops);
}

void EffectsHandlerImpl::windowToScreen(EffectWindow* w, EffectScreen *screen)
{
    auto cl = qobject_cast<AbstractClient *>(static_cast<EffectWindowImpl *>(w)->window());
    if (cl && !cl->isDesktop() && !cl->isDock()) {
        EffectScreenImpl *screenImpl = static_cast<EffectScreenImpl *>(screen);
        Workspace::self()->sendClientToOutput(cl, screenImpl->platformOutput());
    }
}

void EffectsHandlerImpl::setShowingDesktop(bool showing)
{
    Workspace::self()->setShowingDesktop(showing);
}

QString EffectsHandlerImpl::currentActivity() const
{
#ifdef KWIN_BUILD_ACTIVITIES
    if (!Activities::self()) {
        return QString();
    }
    return Activities::self()->current();
#else
    return QString();
#endif
}

int EffectsHandlerImpl::currentDesktop() const
{
    return VirtualDesktopManager::self()->current();
}

int EffectsHandlerImpl::numberOfDesktops() const
{
    return VirtualDesktopManager::self()->count();
}

void EffectsHandlerImpl::setCurrentDesktop(int desktop)
{
    VirtualDesktopManager::self()->setCurrent(desktop);
}

void EffectsHandlerImpl::setNumberOfDesktops(int desktops)
{
    VirtualDesktopManager::self()->setCount(desktops);
}

QSize EffectsHandlerImpl::desktopGridSize() const
{
    return VirtualDesktopManager::self()->grid().size();
}

int EffectsHandlerImpl::desktopGridWidth() const
{
    return desktopGridSize().width();
}

int EffectsHandlerImpl::desktopGridHeight() const
{
    return desktopGridSize().height();
}

int EffectsHandlerImpl::workspaceWidth() const
{
    return desktopGridWidth() * Screens::self()->size().width();
}

int EffectsHandlerImpl::workspaceHeight() const
{
    return desktopGridHeight() * Screens::self()->size().height();
}

int EffectsHandlerImpl::desktopAtCoords(QPoint coords) const
{
    if (auto vd = VirtualDesktopManager::self()->grid().at(coords)) {
        return vd->x11DesktopNumber();
    }
    return 0;
}

QPoint EffectsHandlerImpl::desktopGridCoords(int id) const
{
    return VirtualDesktopManager::self()->grid().gridCoords(id);
}

QPoint EffectsHandlerImpl::desktopCoords(int id) const
{
    QPoint coords = VirtualDesktopManager::self()->grid().gridCoords(id);
    if (coords.x() == -1)
        return QPoint(-1, -1);
    const QSize displaySize = Screens::self()->size();
    return QPoint(coords.x() * displaySize.width(), coords.y() * displaySize.height());
}

int EffectsHandlerImpl::desktopAbove(int desktop, bool wrap) const
{
    return getDesktop<DesktopAbove>(desktop, wrap);
}

int EffectsHandlerImpl::desktopToRight(int desktop, bool wrap) const
{
    return getDesktop<DesktopRight>(desktop, wrap);
}

int EffectsHandlerImpl::desktopBelow(int desktop, bool wrap) const
{
    return getDesktop<DesktopBelow>(desktop, wrap);
}

int EffectsHandlerImpl::desktopToLeft(int desktop, bool wrap) const
{
    return getDesktop<DesktopLeft>(desktop, wrap);
}

QString EffectsHandlerImpl::desktopName(int desktop) const
{
    const VirtualDesktop *vd = VirtualDesktopManager::self()->desktopForX11Id(desktop);
    return vd ? vd->name() : QString();
}

bool EffectsHandlerImpl::optionRollOverDesktops() const
{
    return options->isRollOverDesktops();
}

double EffectsHandlerImpl::animationTimeFactor() const
{
    return options->animationTimeFactor();
}

EffectWindow* EffectsHandlerImpl::findWindow(WId id) const
{
    if (X11Client *w = Workspace::self()->findClient(Predicate::WindowMatch, id))
        return w->effectWindow();
    if (Unmanaged* w = Workspace::self()->findUnmanaged(id))
        return w->effectWindow();
    return nullptr;
}

EffectWindow* EffectsHandlerImpl::findWindow(KWaylandServer::SurfaceInterface *surf) const
{
    if (waylandServer()) {
        if (AbstractClient *w = waylandServer()->findClient(surf)) {
            return w->effectWindow();
        }
    }
    return nullptr;
}

EffectWindow *EffectsHandlerImpl::findWindow(QWindow *w) const
{
    if (Toplevel *toplevel = workspace()->findInternal(w)) {
        return toplevel->effectWindow();
    }
    return nullptr;
}

EffectWindow *EffectsHandlerImpl::findWindow(const QUuid &id) const
{
    if (const auto client = workspace()->findAbstractClient([&id] (const AbstractClient *c) { return c->internalId() == id; })) {
        return client->effectWindow();
    }
    if (const auto unmanaged = workspace()->findUnmanaged([&id] (const Unmanaged *c) { return c->internalId() == id; })) {
        return unmanaged->effectWindow();
    }
    return nullptr;
}

EffectWindowList EffectsHandlerImpl::stackingOrder() const
{
    QList<Toplevel *> list = Workspace::self()->xStackingOrder();
    EffectWindowList ret;
    for (Toplevel *t : list) {
        if (EffectWindow *w = effectWindow(t))
            ret.append(w);
    }
    return ret;
}

void EffectsHandlerImpl::setElevatedWindow(KWin::EffectWindow* w, bool set)
{
    elevated_windows.removeAll(w);
    if (set)
        elevated_windows.append(w);
}

void EffectsHandlerImpl::setTabBoxWindow(EffectWindow* w)
{
#ifdef KWIN_BUILD_TABBOX
    if (auto c = qobject_cast<AbstractClient *>(static_cast<EffectWindowImpl *>(w)->window())) {
        TabBox::TabBox::self()->setCurrentClient(c);
    }
#else
    Q_UNUSED(w)
#endif
}

void EffectsHandlerImpl::setTabBoxDesktop(int desktop)
{
#ifdef KWIN_BUILD_TABBOX
    TabBox::TabBox::self()->setCurrentDesktop(desktop);
#else
    Q_UNUSED(desktop)
#endif
}

EffectWindowList EffectsHandlerImpl::currentTabBoxWindowList() const
{
#ifdef KWIN_BUILD_TABBOX
    const auto clients = TabBox::TabBox::self()->currentClientList();
    EffectWindowList ret;
    ret.reserve(clients.size());
    std::transform(std::cbegin(clients), std::cend(clients),
        std::back_inserter(ret),
        [](auto client) { return client->effectWindow(); });
    return ret;
#else
    return EffectWindowList();
#endif
}

void EffectsHandlerImpl::refTabBox()
{
#ifdef KWIN_BUILD_TABBOX
    TabBox::TabBox::self()->reference();
#endif
}

void EffectsHandlerImpl::unrefTabBox()
{
#ifdef KWIN_BUILD_TABBOX
    TabBox::TabBox::self()->unreference();
#endif
}

void EffectsHandlerImpl::closeTabBox()
{
#ifdef KWIN_BUILD_TABBOX
    TabBox::TabBox::self()->close();
#endif
}

QList< int > EffectsHandlerImpl::currentTabBoxDesktopList() const
{
#ifdef KWIN_BUILD_TABBOX
    return TabBox::TabBox::self()->currentDesktopList();
#else
    return QList< int >();
#endif
}

int EffectsHandlerImpl::currentTabBoxDesktop() const
{
#ifdef KWIN_BUILD_TABBOX
    return TabBox::TabBox::self()->currentDesktop();
#else
    return -1;
#endif
}

EffectWindow* EffectsHandlerImpl::currentTabBoxWindow() const
{
#ifdef KWIN_BUILD_TABBOX
    if (auto c = TabBox::TabBox::self()->currentClient())
        return c->effectWindow();
#endif
    return nullptr;
}

void EffectsHandlerImpl::addRepaintFull()
{
    m_compositor->scene()->addRepaintFull();
}

void EffectsHandlerImpl::addRepaint(const QRect& r)
{
    m_compositor->scene()->addRepaint(r);
}

void EffectsHandlerImpl::addRepaint(const QRegion& r)
{
    m_compositor->scene()->addRepaint(r);
}

void EffectsHandlerImpl::addRepaint(int x, int y, int w, int h)
{
    m_compositor->scene()->addRepaint(x, y, w, h);
}

EffectScreen *EffectsHandlerImpl::activeScreen() const
{
    return EffectScreenImpl::get(workspace()->activeOutput());
}

QRect EffectsHandlerImpl::clientArea(clientAreaOption opt, const EffectScreen *screen, int desktop) const
{
    const VirtualDesktop *virtualDesktop;
    if (desktop == 0 || desktop == -1) {
        virtualDesktop = VirtualDesktopManager::self()->currentDesktop();
    } else {
        virtualDesktop = VirtualDesktopManager::self()->desktopForX11Id(desktop);
    }

    const EffectScreenImpl *screenImpl = static_cast<const EffectScreenImpl *>(screen);
    return Workspace::self()->clientArea(opt, screenImpl->platformOutput(), virtualDesktop);
}

QRect EffectsHandlerImpl::clientArea(clientAreaOption opt, const EffectWindow* c) const
{
    const Toplevel* t = static_cast< const EffectWindowImpl* >(c)->window();
    return Workspace::self()->clientArea(opt, t);
}

QRect EffectsHandlerImpl::clientArea(clientAreaOption opt, const QPoint& p, int desktop) const
{
    return Workspace::self()->clientArea(opt, p, desktop);
}

QRect EffectsHandlerImpl::virtualScreenGeometry() const
{
    return Screens::self()->geometry();
}

QSize EffectsHandlerImpl::virtualScreenSize() const
{
    return Screens::self()->size();
}

void EffectsHandlerImpl::defineCursor(Qt::CursorShape shape)
{
    input()->pointer()->setEffectsOverrideCursor(shape);
}

bool EffectsHandlerImpl::checkInputWindowEvent(QMouseEvent *e)
{
    Q_ASSERT(this!=NULL);
    if (m_grabbedMouseEffects.isEmpty()) {
        return false;
    }
    for (Effect *effect : qAsConst(m_grabbedMouseEffects)) {
        effect->windowInputMouseEvent(e);
    }
    return true;
}

bool EffectsHandlerImpl::checkInputWindowEvent(QWheelEvent *e)
{
    if (m_grabbedMouseEffects.isEmpty()) {
        return false;
    }
    for (Effect *effect : qAsConst(m_grabbedMouseEffects)) {
        effect->windowInputMouseEvent(e);
    }
    return true;
}

void EffectsHandlerImpl::connectNotify(const QMetaMethod &signal)
{
    if (signal == QMetaMethod::fromSignal(&EffectsHandler::cursorShapeChanged)) {
        if (!m_trackingCursorChanges) {
            connect(Cursors::self()->mouse(), &Cursor::cursorChanged, this, &EffectsHandler::cursorShapeChanged);
            Cursors::self()->mouse()->startCursorTracking();
        }
        ++m_trackingCursorChanges;
    }
    EffectsHandler::connectNotify(signal);
}

void EffectsHandlerImpl::disconnectNotify(const QMetaMethod &signal)
{
    if (signal == QMetaMethod::fromSignal(&EffectsHandler::cursorShapeChanged)) {
        Q_ASSERT(m_trackingCursorChanges > 0);
        if (!--m_trackingCursorChanges) {
            Cursors::self()->mouse()->stopCursorTracking();
            disconnect(Cursors::self()->mouse(), &Cursor::cursorChanged, this, &EffectsHandler::cursorShapeChanged);
        }
    }
    EffectsHandler::disconnectNotify(signal);
}


void EffectsHandlerImpl::checkInputWindowStacking()
{
    if (m_grabbedMouseEffects.isEmpty()) {
        return;
    }
    doCheckInputWindowStacking();
}

void EffectsHandlerImpl::doCheckInputWindowStacking()
{
}

QPoint EffectsHandlerImpl::cursorPos() const
{
    return Cursors::self()->mouse()->pos();
}

void EffectsHandlerImpl::reserveElectricBorder(ElectricBorder border, Effect *effect)
{
    ScreenEdges::self()->reserve(border, effect, "borderActivated");
}

void EffectsHandlerImpl::unreserveElectricBorder(ElectricBorder border, Effect *effect)
{
    ScreenEdges::self()->unreserve(border, effect);
}

void EffectsHandlerImpl::registerTouchBorder(ElectricBorder border, QAction *action)
{
    ScreenEdges::self()->reserveTouch(border, action);
}

void EffectsHandlerImpl::unregisterTouchBorder(ElectricBorder border, QAction *action)
{
    ScreenEdges::self()->unreserveTouch(border, action);
}

QPainter *EffectsHandlerImpl::scenePainter()
{
    return m_scene->scenePainter();
}

void EffectsHandlerImpl::toggleEffect(const QString& name)
{
    if (isEffectLoaded(name))
        unloadEffect(name);
    else
        loadEffect(name);
}

QStringList EffectsHandlerImpl::loadedEffects() const
{
    QStringList listModules;
    listModules.reserve(loaded_effects.count());
    std::transform(loaded_effects.constBegin(), loaded_effects.constEnd(),
        std::back_inserter(listModules),
        [](const EffectPair &pair) { return pair.first; });
    return listModules;
}

QStringList EffectsHandlerImpl::listOfEffects() const
{
    return m_effectLoader->listOfKnownEffects();
}

bool EffectsHandlerImpl::loadEffect(const QString& name)
{
    makeOpenGLContextCurrent();
    m_compositor->scene()->addRepaintFull();

    return m_effectLoader->loadEffect(name);
}

void EffectsHandlerImpl::unloadEffect(const QString& name)
{
    auto it = std::find_if(effect_order.begin(), effect_order.end(),
        [name](EffectPair &pair) {
            return pair.first == name;
        }
    );
    if (it == effect_order.end()) {
        qCDebug(KWIN_CORE) << "EffectsHandler::unloadEffect : Effect not loaded :" << name;
        return;
    }

    qCDebug(KWIN_CORE) << "EffectsHandler::unloadEffect : Unloading Effect :" << name;
    destroyEffect((*it).second);
    effect_order.erase(it);
    effectsChanged();

    m_compositor->scene()->addRepaintFull();
}

void EffectsHandlerImpl::destroyEffect(Effect *effect)
{
    makeOpenGLContextCurrent();

    if (fullscreen_effect == effect) {
        setActiveFullScreenEffect(nullptr);
    }

    if (keyboard_grab_effect == effect) {
        ungrabKeyboard();
    }

    stopMouseInterception(effect);

    const QList<QByteArray> properties = m_propertiesForEffects.keys();
    for (const QByteArray &property : properties) {
        removeSupportProperty(property, effect);
    }

    delete effect;
}

void EffectsHandlerImpl::reconfigureEffect(const QString& name)
{
    for (QVector< EffectPair >::const_iterator it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it)
        if ((*it).first == name) {
            kwinApp()->config()->reparseConfiguration();
            makeOpenGLContextCurrent();
            (*it).second->reconfigure(Effect::ReconfigureAll);
            return;
        }
}

bool EffectsHandlerImpl::isEffectLoaded(const QString& name) const
{
    auto it = std::find_if(loaded_effects.constBegin(), loaded_effects.constEnd(),
        [&name](const EffectPair &pair) { return pair.first == name; });
    return it != loaded_effects.constEnd();
}

bool EffectsHandlerImpl::isEffectSupported(const QString &name)
{
    // If the effect is loaded, it is obviously supported.
    if (isEffectLoaded(name)) {
        return true;
    }

    // next checks might require a context
    makeOpenGLContextCurrent();

    return m_effectLoader->isEffectSupported(name);

}

QList<bool> EffectsHandlerImpl::areEffectsSupported(const QStringList &names)
{
    QList<bool> retList;
    retList.reserve(names.count());
    std::transform(names.constBegin(), names.constEnd(),
        std::back_inserter(retList),
        [this](const QString &name) {
            return isEffectSupported(name);
        });
    return retList;
}

void EffectsHandlerImpl::reloadEffect(Effect *effect)
{
    QString effectName;
    for (QVector< EffectPair >::const_iterator it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if ((*it).second == effect) {
            effectName = (*it).first;
            break;
        }
    }
    if (!effectName.isNull()) {
        unloadEffect(effectName);
        m_effectLoader->loadEffect(effectName);
    }
}

void EffectsHandlerImpl::effectsChanged()
{
    loaded_effects.clear();
    m_activeEffects.clear(); // it's possible to have a reconfigure and a quad rebuild between two paint cycles - bug #308201

    loaded_effects.reserve(effect_order.count());
    std::copy(effect_order.constBegin(), effect_order.constEnd(),
        std::back_inserter(loaded_effects));

    m_activeEffects.reserve(loaded_effects.count());
}

QStringList EffectsHandlerImpl::activeEffects() const
{
    QStringList ret;
    for(QVector< KWin::EffectPair >::const_iterator it = loaded_effects.constBegin(),
                                                    end = loaded_effects.constEnd(); it != end; ++it) {
            if (it->second->isActive()) {
                ret << it->first;
            }
        }
    return ret;
}

bool EffectsHandlerImpl::blocksDirectScanout() const
{
    if (fullscreen_effect != NULL) return true;
    for(QVector< KWin::EffectPair >::const_iterator it = loaded_effects.constBegin(),
                                                    end = loaded_effects.constEnd(); it != end; ++it) {
        if (it->second->isActive() && it->second->blocksDirectScanout()) {
            return true;
        }
    }
    return false;
}

KWaylandServer::Display *EffectsHandlerImpl::waylandDisplay() const
{
    if (waylandServer()) {
        return waylandServer()->display();
    }
    return nullptr;
}

EffectFrame* EffectsHandlerImpl::effectFrame(EffectFrameStyle style, bool staticSize, const QPoint& position, Qt::Alignment alignment) const
{
    return new EffectFrameImpl(style, staticSize, position, alignment);
}


QVariant EffectsHandlerImpl::kwinOption(KWinOption kwopt)
{
    switch (kwopt) {
    case CloseButtonCorner: {
        // TODO: this could become per window and be derived from the actual position in the deco
        const auto settings = Decoration::DecorationBridge::self()->settings();
        return settings && settings->decorationButtonsLeft().contains(KDecoration2::DecorationButtonType::Close) ? Qt::TopLeftCorner : Qt::TopRightCorner;
    }
    case SwitchDesktopOnScreenEdge:
        return ScreenEdges::self()->isDesktopSwitching();
    case SwitchDesktopOnScreenEdgeMovingWindows:
        return ScreenEdges::self()->isDesktopSwitchingMovingClients();
    default:
        return QVariant(); // an invalid one
    }
}

QString EffectsHandlerImpl::supportInformation(const QString &name) const
{
    auto it = std::find_if(loaded_effects.constBegin(), loaded_effects.constEnd(),
        [name](const EffectPair &pair) { return pair.first == name; });
    if (it == loaded_effects.constEnd()) {
        return QString();
    }

    QString support((*it).first + QLatin1String(":\n"));
    const QMetaObject *metaOptions = (*it).second->metaObject();
    for (int i=0; i<metaOptions->propertyCount(); ++i) {
        const QMetaProperty property = metaOptions->property(i);
        if (qstrcmp(property.name(), "objectName") == 0) {
            continue;
        }
        support += QString::fromUtf8(property.name()) + QLatin1String(": ") + (*it).second->property(property.name()).toString() + QLatin1Char('\n');
    }

    return support;
}


bool EffectsHandlerImpl::isScreenLocked() const
{
    return ScreenLockerWatcher::self()->isLocked();
}

QString EffectsHandlerImpl::debug(const QString& name, const QString& parameter) const
{
    QString internalName = name.toLower();;
    for (QVector< EffectPair >::const_iterator it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if ((*it).first == internalName) {
            return it->second->debug(parameter);
        }
    }
    return QString();
}

bool EffectsHandlerImpl::makeOpenGLContextCurrent()
{
    return m_scene->makeOpenGLContextCurrent();
}

void EffectsHandlerImpl::doneOpenGLContextCurrent()
{
    m_scene->doneOpenGLContextCurrent();
}

bool EffectsHandlerImpl::animationsSupported() const
{
    static const QByteArray forceEnvVar = qgetenv("KWIN_EFFECTS_FORCE_ANIMATIONS");
    if (!forceEnvVar.isEmpty()) {
        static const int forceValue = forceEnvVar.toInt();
        return forceValue == 1;
    }
    return m_scene->animationsSupported();
}

void EffectsHandlerImpl::highlightWindows(const QVector<EffectWindow *> &windows)
{
    Effect *e = provides(Effect::HighlightWindows);
    if (!e) {
        return;
    }
    e->perform(Effect::HighlightWindows, QVariantList{QVariant::fromValue(windows)});
}

PlatformCursorImage EffectsHandlerImpl::cursorImage() const
{
    return kwinApp()->platform()->cursorImage();
}

void EffectsHandlerImpl::hideCursor()
{
    Cursors::self()->hideCursor();
}

void EffectsHandlerImpl::showCursor()
{
    Cursors::self()->showCursor();
}

void EffectsHandlerImpl::startInteractiveWindowSelection(std::function<void(KWin::EffectWindow*)> callback)
{
    kwinApp()->platform()->startInteractiveWindowSelection(
        [callback] (KWin::Toplevel *t) {
            if (t && t->effectWindow()) {
                callback(t->effectWindow());
            } else {
                callback(nullptr);
            }
        }
    );
}

void EffectsHandlerImpl::startInteractivePositionSelection(std::function<void(const QPoint&)> callback)
{
    kwinApp()->platform()->startInteractivePositionSelection(callback);
}

void EffectsHandlerImpl::showOnScreenMessage(const QString &message, const QString &iconName)
{
    OSD::show(message, iconName);
}

void EffectsHandlerImpl::hideOnScreenMessage(OnScreenMessageHideFlags flags)
{
    OSD::HideFlags osdFlags;
    if (flags.testFlag(OnScreenMessageHideFlag::SkipsCloseAnimation)) {
        osdFlags |= OSD::HideFlag::SkipCloseAnimation;
    }
    OSD::hide(osdFlags);
}

KSharedConfigPtr EffectsHandlerImpl::config() const
{
    return kwinApp()->config();
}

KSharedConfigPtr EffectsHandlerImpl::inputConfig() const
{
    return InputConfig::self()->inputConfig();
}

Effect *EffectsHandlerImpl::findEffect(const QString &name) const
{
    auto it = std::find_if(loaded_effects.constBegin(), loaded_effects.constEnd(),
        [name] (const EffectPair &pair) {
            return pair.first == name;
        }
    );
    if (it == loaded_effects.constEnd()) {
        return nullptr;
    }
    return (*it).second;
}

void EffectsHandlerImpl::renderOffscreenQuickView(OffscreenQuickView *w) const
{
    if (!w->isVisible()) {
        return;
    }
    scene()->paintOffscreenQuickView(w);
}

SessionState EffectsHandlerImpl::sessionState() const
{
    return Workspace::self()->sessionManager()->state();
}

QList<EffectScreen *> EffectsHandlerImpl::screens() const
{
    return m_effectScreens;
}

EffectScreen *EffectsHandlerImpl::screenAt(const QPoint &point) const
{
    return EffectScreenImpl::get(kwinApp()->platform()->outputAt(point));
}

EffectScreen *EffectsHandlerImpl::findScreen(const QString &name) const
{
    for (EffectScreen *screen : qAsConst(m_effectScreens)) {
        if (screen->name() == name) {
            return screen;
        }
    }
    return nullptr;
}

EffectScreen *EffectsHandlerImpl::findScreen(int screenId) const
{
    return m_effectScreens.value(screenId);
}

void EffectsHandlerImpl::slotOutputEnabled(AbstractOutput *output)
{
    EffectScreen *screen = new EffectScreenImpl(output, this);
    m_effectScreens.append(screen);
    Q_EMIT screenAdded(screen);
}

void EffectsHandlerImpl::slotOutputDisabled(AbstractOutput *output)
{
    EffectScreen *screen = EffectScreenImpl::get(output);
    m_effectScreens.removeOne(screen);
    Q_EMIT screenRemoved(screen);
    delete screen;
}

void EffectsHandlerImpl::renderScreen(EffectScreen *screen)
{
    auto output = static_cast<EffectScreenImpl *>(screen)->platformOutput();
    scene()->paintScreen(output, Compositor::self()->windowsToRender());
}

bool EffectsHandlerImpl::isCursorHidden() const
{
    return Cursors::self()->isCursorHidden();
}

//****************************************
// EffectScreenImpl
//****************************************

EffectScreenImpl::EffectScreenImpl(AbstractOutput *output, QObject *parent)
    : EffectScreen(parent)
    , m_platformOutput(output)
{
    m_platformOutput->m_effectScreen = this;

    connect(output, &AbstractOutput::aboutToChange, this, &EffectScreen::aboutToChange);
    connect(output, &AbstractOutput::changed, this, &EffectScreen::changed);
    connect(output, &AbstractOutput::wakeUp, this, &EffectScreen::wakeUp);
    connect(output, &AbstractOutput::aboutToTurnOff, this, &EffectScreen::aboutToTurnOff);
    connect(output, &AbstractOutput::scaleChanged, this, &EffectScreen::devicePixelRatioChanged);
    connect(output, &AbstractOutput::geometryChanged, this, &EffectScreen::geometryChanged);
}

EffectScreenImpl::~EffectScreenImpl()
{
    if (m_platformOutput) {
        m_platformOutput->m_effectScreen = nullptr;
    }
}

EffectScreenImpl *EffectScreenImpl::get(AbstractOutput *output)
{
    return output->m_effectScreen;
}

AbstractOutput *EffectScreenImpl::platformOutput() const
{
    return m_platformOutput;
}

QString EffectScreenImpl::name() const
{
    return m_platformOutput->name();
}

qreal EffectScreenImpl::devicePixelRatio() const
{
    return m_platformOutput->scale();
}

QRect EffectScreenImpl::geometry() const
{
    return m_platformOutput->geometry();
}

EffectScreen::Transform EffectScreenImpl::transform() const
{
    return EffectScreen::Transform(m_platformOutput->transform());
}

//****************************************
// EffectWindowImpl
//****************************************

EffectWindowImpl::EffectWindowImpl(Toplevel *toplevel)
    : EffectWindow(toplevel)
    , toplevel(toplevel)
    , sw(nullptr)
{
    // Deleted windows are not managed. So, when windowClosed signal is
    // emitted, effects can't distinguish managed windows from unmanaged
    // windows(e.g. combo box popups, popup menus, etc). Save value of the
    // managed property during construction of EffectWindow. At that time,
    // parent can be Client, XdgShellClient, or Unmanaged. So, later on, when
    // an instance of Deleted becomes parent of the EffectWindow, effects
    // can still figure out whether it is/was a managed window.
    managed = toplevel->isClient();

    waylandClient = qobject_cast<KWin::WaylandClient *>(toplevel) != nullptr;
    x11Client = qobject_cast<KWin::X11Client *>(toplevel) != nullptr ||
        qobject_cast<KWin::Unmanaged *>(toplevel) != nullptr;
}

EffectWindowImpl::~EffectWindowImpl()
{
    QVariant cachedTextureVariant = data(LanczosCacheRole);
    if (cachedTextureVariant.isValid()) {
        GLTexture *cachedTexture = static_cast< GLTexture*>(cachedTextureVariant.value<void*>());
        delete cachedTexture;
    }
}

bool EffectWindowImpl::isPaintingEnabled()
{
    return sceneWindow()->isPaintingEnabled();
}

void EffectWindowImpl::enablePainting(int reason)
{
    sceneWindow()->enablePainting(reason);
}

void EffectWindowImpl::disablePainting(int reason)
{
    sceneWindow()->disablePainting(reason);
}

void EffectWindowImpl::addRepaint(const QRect &r)
{
    toplevel->addRepaint(r);
}

void EffectWindowImpl::addRepaint(int x, int y, int w, int h)
{
    toplevel->addRepaint(x, y, w, h);
}

void EffectWindowImpl::addRepaintFull()
{
    toplevel->addRepaintFull();
}

void EffectWindowImpl::addLayerRepaint(const QRect &r)
{
    toplevel->addLayerRepaint(r);
}

void EffectWindowImpl::addLayerRepaint(int x, int y, int w, int h)
{
    toplevel->addLayerRepaint(x, y, w, h);
}

const EffectWindowGroup* EffectWindowImpl::group() const
{
    if (auto c = qobject_cast<X11Client *>(toplevel)) {
        return c->group()->effectGroup();
    }
    return nullptr; // TODO
}

void EffectWindowImpl::refWindow()
{
    if (auto d = qobject_cast<Deleted *>(toplevel)) {
        return d->refWindow();
    }
    Q_UNREACHABLE(); // TODO
}

void EffectWindowImpl::unrefWindow()
{
    if (auto d = qobject_cast<Deleted *>(toplevel)) {
        return d->unrefWindow();   // delays deletion in case
    }
    Q_UNREACHABLE(); // TODO
}

EffectScreen *EffectWindowImpl::screen() const
{
    return EffectScreenImpl::get(toplevel->output());
}

#define TOPLEVEL_HELPER( rettype, prototype, toplevelPrototype) \
    rettype EffectWindowImpl::prototype ( ) const \
    { \
        return toplevel->toplevelPrototype(); \
    }

TOPLEVEL_HELPER(double, opacity, opacity)
TOPLEVEL_HELPER(bool, hasAlpha, hasAlpha)
TOPLEVEL_HELPER(int, x, x)
TOPLEVEL_HELPER(int, y, y)
TOPLEVEL_HELPER(int, width, width)
TOPLEVEL_HELPER(int, height, height)
TOPLEVEL_HELPER(QPoint, pos, pos)
TOPLEVEL_HELPER(QSize, size, size)
TOPLEVEL_HELPER(QRect, geometry, frameGeometry)
TOPLEVEL_HELPER(QRect, frameGeometry, frameGeometry)
TOPLEVEL_HELPER(QRect, bufferGeometry, bufferGeometry)
TOPLEVEL_HELPER(QRect, clientGeometry, clientGeometry)
TOPLEVEL_HELPER(QRect, expandedGeometry, visibleGeometry)
TOPLEVEL_HELPER(QRect, rect, rect)
TOPLEVEL_HELPER(int, desktop, desktop)
TOPLEVEL_HELPER(bool, isDesktop, isDesktop)
TOPLEVEL_HELPER(bool, isDock, isDock)
TOPLEVEL_HELPER(bool, isToolbar, isToolbar)
TOPLEVEL_HELPER(bool, isMenu, isMenu)
TOPLEVEL_HELPER(bool, isNormalWindow, isNormalWindow)
TOPLEVEL_HELPER(bool, isDialog, isDialog)
TOPLEVEL_HELPER(bool, isSplash, isSplash)
TOPLEVEL_HELPER(bool, isUtility, isUtility)
TOPLEVEL_HELPER(bool, isDropdownMenu, isDropdownMenu)
TOPLEVEL_HELPER(bool, isPopupMenu, isPopupMenu)
TOPLEVEL_HELPER(bool, isTooltip, isTooltip)
TOPLEVEL_HELPER(bool, isNotification, isNotification)
TOPLEVEL_HELPER(bool, isCriticalNotification, isCriticalNotification)
TOPLEVEL_HELPER(bool, isOnScreenDisplay, isOnScreenDisplay)
TOPLEVEL_HELPER(bool, isComboBox, isComboBox)
TOPLEVEL_HELPER(bool, isDNDIcon, isDNDIcon)
TOPLEVEL_HELPER(bool, isDeleted, isDeleted)
TOPLEVEL_HELPER(QString, windowRole, windowRole)
TOPLEVEL_HELPER(QStringList, activities, activities)
TOPLEVEL_HELPER(bool, skipsCloseAnimation, skipsCloseAnimation)
TOPLEVEL_HELPER(KWaylandServer::SurfaceInterface *, surface, surface)
TOPLEVEL_HELPER(bool, isPopupWindow, isPopupWindow)
TOPLEVEL_HELPER(bool, isOutline, isOutline)
TOPLEVEL_HELPER(bool, isLockScreen, isLockScreen)
TOPLEVEL_HELPER(pid_t, pid, pid)
TOPLEVEL_HELPER(qlonglong, windowId, window)

#undef TOPLEVEL_HELPER

#define CLIENT_HELPER_WITH_DELETED( rettype, prototype, propertyname, defaultValue ) \
    rettype EffectWindowImpl::prototype ( ) const \
    { \
        auto client = qobject_cast<AbstractClient *>(toplevel); \
        if (client) { \
            return client->propertyname(); \
        } \
        auto deleted = qobject_cast<Deleted *>(toplevel); \
        if (deleted) { \
            return deleted->propertyname(); \
        } \
        return defaultValue; \
    }

CLIENT_HELPER_WITH_DELETED(bool, isMinimized, isMinimized, false)
CLIENT_HELPER_WITH_DELETED(bool, isModal, isModal, false)
CLIENT_HELPER_WITH_DELETED(bool, isFullScreen, isFullScreen, false)
CLIENT_HELPER_WITH_DELETED(bool, keepAbove, keepAbove, false)
CLIENT_HELPER_WITH_DELETED(bool, keepBelow, keepBelow, false)
CLIENT_HELPER_WITH_DELETED(QString, caption, caption, QString());
CLIENT_HELPER_WITH_DELETED(QVector<uint>, desktops, x11DesktopIds, QVector<uint>());

#undef CLIENT_HELPER_WITH_DELETED

// legacy from tab groups, can be removed when no effects use this any more.
bool EffectWindowImpl::isCurrentTab() const
{
    return true;
}

QString EffectWindowImpl::windowClass() const
{
    return toplevel->resourceName() + QLatin1Char(' ') + toplevel->resourceClass();
}

QRect EffectWindowImpl::contentsRect() const
{
    return QRect(toplevel->clientPos(), toplevel->clientSize());
}

NET::WindowType EffectWindowImpl::windowType() const
{
    return toplevel->windowType();
}

#define CLIENT_HELPER( rettype, prototype, propertyname, defaultValue ) \
    rettype EffectWindowImpl::prototype ( ) const \
    { \
        auto client = qobject_cast<AbstractClient *>(toplevel); \
        if (client) { \
            return client->propertyname(); \
        } \
        return defaultValue; \
    }

CLIENT_HELPER(bool, isMovable, isMovable, false)
CLIENT_HELPER(bool, isMovableAcrossScreens, isMovableAcrossScreens, false)
CLIENT_HELPER(bool, isUserMove, isInteractiveMove, false)
CLIENT_HELPER(bool, isUserResize, isInteractiveResize, false)
CLIENT_HELPER(QRect, iconGeometry, iconGeometry, QRect())
CLIENT_HELPER(bool, isSpecialWindow, isSpecialWindow, true)
CLIENT_HELPER(bool, acceptsFocus, wantsInput, true) // We don't actually know...
CLIENT_HELPER(QIcon, icon, icon, QIcon())
CLIENT_HELPER(bool, isSkipSwitcher, skipSwitcher, false)
CLIENT_HELPER(bool, decorationHasAlpha, decorationHasAlpha, false)
CLIENT_HELPER(bool, isUnresponsive, unresponsive, false)

#undef CLIENT_HELPER

QSize EffectWindowImpl::basicUnit() const
{
    if (auto client = qobject_cast<X11Client *>(toplevel)){
        return client->basicUnit();
    }
    return QSize(1,1);
}

void EffectWindowImpl::setWindow(Toplevel* w)
{
    toplevel = w;
    setParent(w);
}

void EffectWindowImpl::setSceneWindow(Scene::Window* w)
{
    sw = w;
}

QRect EffectWindowImpl::decorationInnerRect() const
{
    return toplevel->rect() - toplevel->frameMargins();
}

QByteArray EffectWindowImpl::readProperty(long atom, long type, int format) const
{
    if (!kwinApp()->x11Connection()) {
        return QByteArray();
    }
    return readWindowProperty(window()->window(), atom, type, format);
}

void EffectWindowImpl::deleteProperty(long int atom) const
{
    if (kwinApp()->x11Connection()) {
        deleteWindowProperty(window()->window(), atom);
    }
}

EffectWindow* EffectWindowImpl::findModal()
{
    auto client = qobject_cast<AbstractClient *>(toplevel);
    if (!client) {
        return nullptr;
    }

    AbstractClient *modal = client->findModal();
    if (modal) {
        return modal->effectWindow();
    }

    return nullptr;
}

EffectWindow* EffectWindowImpl::transientFor()
{
    auto client = qobject_cast<AbstractClient *>(toplevel);
    if (!client) {
        return nullptr;
    }

    AbstractClient *transientFor = client->transientFor();
    if (transientFor) {
        return transientFor->effectWindow();
    }

    return nullptr;
}

QWindow *EffectWindowImpl::internalWindow() const
{
    auto client = qobject_cast<InternalClient *>(toplevel);
    if (!client) {
        return nullptr;
    }
    return client->internalWindow();
}

template <typename T>
EffectWindowList getMainWindows(T *c)
{
    const auto mainclients = c->mainClients();
    EffectWindowList ret;
    ret.reserve(mainclients.size());
    std::transform(std::cbegin(mainclients), std::cend(mainclients),
        std::back_inserter(ret),
        [](auto client) { return client->effectWindow(); });
    return ret;
}

EffectWindowList EffectWindowImpl::mainWindows() const
{
    if (auto client = qobject_cast<AbstractClient *>(toplevel)) {
        return getMainWindows(client);
    }
    if (auto deleted = qobject_cast<Deleted *>(toplevel)) {
        return getMainWindows(deleted);
    }
    return {};
}

void EffectWindowImpl::setData(int role, const QVariant &data)
{
    if (!data.isNull())
        dataMap[ role ] = data;
    else
        dataMap.remove(role);
    Q_EMIT effects->windowDataChanged(this, role);
}

QVariant EffectWindowImpl::data(int role) const
{
    return dataMap.value(role);
}

EffectWindow* effectWindow(Toplevel* w)
{
    EffectWindowImpl* ret = w->effectWindow();
    return ret;
}

EffectWindow* effectWindow(Scene::Window* w)
{
    EffectWindowImpl* ret = w->window()->effectWindow();
    ret->setSceneWindow(w);
    return ret;
}

void EffectWindowImpl::elevate(bool elevate)
{
    effects->setElevatedWindow(this, elevate);
}

void EffectWindowImpl::minimize()
{
    if (auto client = qobject_cast<AbstractClient *>(toplevel)) {
        client->minimize();
    }
}

void EffectWindowImpl::unminimize()
{
    if (auto client = qobject_cast<AbstractClient *>(toplevel)) {
        client->unminimize();
    }
}

void EffectWindowImpl::closeWindow()
{
    if (auto client = qobject_cast<AbstractClient *>(toplevel)) {
        client->closeWindow();
    }
}

void EffectWindowImpl::referencePreviousWindowPixmap()
{
    if (sw) {
        sw->referencePreviousPixmap();
    }
}

void EffectWindowImpl::unreferencePreviousWindowPixmap()
{
    if (sw) {
        sw->unreferencePreviousPixmap();
    }
}

bool EffectWindowImpl::isManaged() const
{
    return managed;
}

bool EffectWindowImpl::isWaylandClient() const
{
    return waylandClient;
}

bool EffectWindowImpl::isX11Client() const
{
    return x11Client;
}


//****************************************
// EffectWindowGroupImpl
//****************************************


EffectWindowList EffectWindowGroupImpl::members() const
{
    const auto memberList = group->members();
    EffectWindowList ret;
    ret.reserve(memberList.size());
    std::transform(std::cbegin(memberList), std::cend(memberList),
        std::back_inserter(ret),
        [](auto toplevel) { return toplevel->effectWindow(); });
    return ret;
}

//****************************************
// EffectFrameImpl
//****************************************

EffectFrameImpl::EffectFrameImpl(EffectFrameStyle style, bool staticSize, QPoint position, Qt::Alignment alignment)
    : QObject(nullptr)
    , EffectFrame()
    , m_style(style)
    , m_static(staticSize)
    , m_point(position)
    , m_alignment(alignment)
    , m_shader(nullptr)
    , m_theme(new Plasma::Theme(this))
{
    if (m_style == EffectFrameStyled) {
        m_frame.setImagePath(QStringLiteral("widgets/background"));
        m_frame.setCacheAllRenderedFrames(true);
        connect(m_theme, &Plasma::Theme::themeChanged, this, &EffectFrameImpl::plasmaThemeChanged);
    }
    m_selection.setImagePath(QStringLiteral("widgets/viewitem"));
    m_selection.setElementPrefix(QStringLiteral("hover"));
    m_selection.setCacheAllRenderedFrames(true);
    m_selection.setEnabledBorders(Plasma::FrameSvg::AllBorders);

    m_sceneFrame = Compositor::self()->scene()->createEffectFrame(this);
}

EffectFrameImpl::~EffectFrameImpl()
{
    delete m_sceneFrame;
}

const QFont& EffectFrameImpl::font() const
{
    return m_font;
}

void EffectFrameImpl::setFont(const QFont& font)
{
    if (m_font == font) {
        return;
    }
    m_font = font;
    QRect oldGeom = m_geometry;
    if (!m_text.isEmpty()) {
        autoResize();
    }
    if (oldGeom == m_geometry) {
        // Wasn't updated in autoResize()
        m_sceneFrame->freeTextFrame();
    }
}

void EffectFrameImpl::free()
{
    m_sceneFrame->free();
}

const QRect& EffectFrameImpl::geometry() const
{
    return m_geometry;
}

void EffectFrameImpl::setGeometry(const QRect& geometry, bool force)
{
    QRect oldGeom = m_geometry;
    m_geometry = geometry;
    if (m_geometry == oldGeom && !force) {
        return;
    }
    effects->addRepaint(oldGeom);
    effects->addRepaint(m_geometry);
    if (m_geometry.size() == oldGeom.size() && !force) {
        return;
    }

    if (m_style == EffectFrameStyled) {
        qreal left, top, right, bottom;
        m_frame.getMargins(left, top, right, bottom);   // m_geometry is the inner geometry
        m_frame.resizeFrame(m_geometry.adjusted(-left, -top, right, bottom).size());
    }

    free();
}

const QIcon& EffectFrameImpl::icon() const
{
    return m_icon;
}

void EffectFrameImpl::setIcon(const QIcon& icon)
{
    m_icon = icon;
    if (isCrossFade()) {
        m_sceneFrame->crossFadeIcon();
    }
    if (m_iconSize.isEmpty() && !m_icon.availableSizes().isEmpty()) { // Set a size if we don't already have one
        setIconSize(m_icon.availableSizes().constFirst());
    }
    m_sceneFrame->freeIconFrame();
}

const QSize& EffectFrameImpl::iconSize() const
{
    return m_iconSize;
}

void EffectFrameImpl::setIconSize(const QSize& size)
{
    if (m_iconSize == size) {
        return;
    }
    m_iconSize = size;
    autoResize();
    m_sceneFrame->freeIconFrame();
}

void EffectFrameImpl::plasmaThemeChanged()
{
    free();
}

void EffectFrameImpl::render(const QRegion &region, double opacity, double frameOpacity)
{
    if (m_geometry.isEmpty()) {
        return; // Nothing to display
    }
    m_shader = nullptr;
    setScreenProjectionMatrix(static_cast<EffectsHandlerImpl*>(effects)->scene()->screenProjectionMatrix());
    effects->paintEffectFrame(this, region, opacity, frameOpacity);
}

void EffectFrameImpl::finalRender(QRegion region, double opacity, double frameOpacity) const
{
    region = infiniteRegion(); // TODO: Old region doesn't seem to work with OpenGL

    m_sceneFrame->render(region, opacity, frameOpacity);
}

Qt::Alignment EffectFrameImpl::alignment() const
{
    return m_alignment;
}


void
EffectFrameImpl::align(QRect &geometry)
{
    if (m_alignment & Qt::AlignLeft)
        geometry.moveLeft(m_point.x());
    else if (m_alignment & Qt::AlignRight)
        geometry.moveLeft(m_point.x() - geometry.width());
    else
        geometry.moveLeft(m_point.x() - geometry.width() / 2);
    if (m_alignment & Qt::AlignTop)
        geometry.moveTop(m_point.y());
    else if (m_alignment & Qt::AlignBottom)
        geometry.moveTop(m_point.y() - geometry.height());
    else
        geometry.moveTop(m_point.y() - geometry.height() / 2);
}


void EffectFrameImpl::setAlignment(Qt::Alignment alignment)
{
    m_alignment = alignment;
    align(m_geometry);
    setGeometry(m_geometry);
}

void EffectFrameImpl::setPosition(const QPoint& point)
{
    m_point = point;
    QRect geometry = m_geometry; // this is important, setGeometry need call repaint for old & new geometry
    align(geometry);
    setGeometry(geometry);
}

const QString& EffectFrameImpl::text() const
{
    return m_text;
}

void EffectFrameImpl::setText(const QString& text)
{
    if (m_text == text) {
        return;
    }
    if (isCrossFade()) {
        m_sceneFrame->crossFadeText();
    }
    m_text = text;
    QRect oldGeom = m_geometry;
    autoResize();
    if (oldGeom == m_geometry) {
        // Wasn't updated in autoResize()
        m_sceneFrame->freeTextFrame();
    }
}

void EffectFrameImpl::setSelection(const QRect& selection)
{
    if (selection == m_selectionGeometry) {
        return;
    }
    m_selectionGeometry = selection;
    if (m_selectionGeometry.size() != m_selection.frameSize().toSize()) {
        m_selection.resizeFrame(m_selectionGeometry.size());
    }
    // TODO; optimize to only recreate when resizing
    m_sceneFrame->freeSelection();
}

void EffectFrameImpl::autoResize()
{
    if (m_static)
        return; // Not automatically resizing

    QRect geometry;
    // Set size
    if (!m_text.isEmpty()) {
        QFontMetrics metrics(m_font);
        geometry.setSize(metrics.size(0, m_text));
    }
    if (!m_icon.isNull() && !m_iconSize.isEmpty()) {
        geometry.setLeft(-m_iconSize.width());
        if (m_iconSize.height() > geometry.height())
            geometry.setHeight(m_iconSize.height());
    }

    align(geometry);
    setGeometry(geometry);
}

QColor EffectFrameImpl::styledTextColor()
{
    return m_theme->color(Plasma::Theme::TextColor);
}

} // namespace
