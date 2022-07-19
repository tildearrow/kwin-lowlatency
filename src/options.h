/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <m.graesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_OPTIONS_H
#define KWIN_OPTIONS_H

#include "main.h"
#include "placement.h"

#include <KConfigWatcher>

namespace KWin
{

// Whether to keep all windows mapped when compositing (i.e. whether to have
// actively updated window pixmaps).
enum HiddenPreviews {
    // The normal mode with regard to mapped windows. Hidden (minimized, etc.)
    // and windows on inactive virtual desktops are not mapped, their pixmaps
    // are only their icons.
    HiddenPreviewsNever,
    // Like normal mode, but shown windows (i.e. on inactive virtual desktops)
    // are kept mapped, only hidden windows are unmapped.
    HiddenPreviewsShown,
    // All windows are kept mapped regardless of their state.
    HiddenPreviewsAlways
};

/**
 * This enum type specifies whether the Xwayland server must be restarted after a crash.
 */
enum XwaylandCrashPolicy {
    Stop,
    Restart,
};

/**
 * This enum type specifies the latency level configured by the user.
 */
enum LatencyPolicy {
    LatencyExtremelyLow,
    LatencyLow,
    LatencyMedium,
    LatencyHigh,
    LatencyExtremelyHigh,
};

/**
 * This enum type specifies the method for estimating the expected render time.
 */
enum RenderTimeEstimator {
    RenderTimeEstimatorMinimum,
    RenderTimeEstimatorMaximum,
    RenderTimeEstimatorAverage,
};

/**
 * This enum type specifies the VSync mechanism employed.
 */
enum VSyncMechanism {
    VSyncMechanismAuto,
    VSyncMechanismNone,
    VSyncMechanismIntelSwap,
    VSyncMechanismGLFinish,
    VSyncMechanismSGI,
    VSyncMechanismOML,
    VSyncMechanismSGIHack
};

/**
 * This enum type specifies the action to perform in the event of a crash.
 */
enum CrashAction {
    CrashActionRestart,
    CrashActionRestartNoDisable,
    CrashActionQuit,
    CrashActionOpenbox,
    CrashActionRestartDisable,
    CrashActionAAAAAAA
};

class Settings;

class KWIN_EXPORT Options : public QObject
{
    Q_OBJECT
    Q_ENUM(XwaylandCrashPolicy)
    Q_ENUM(LatencyPolicy)
    Q_ENUM(RenderTimeEstimator)
    Q_ENUM(VSyncMechanism)
    Q_ENUM(CrashAction)
    Q_PROPERTY(FocusPolicy focusPolicy READ focusPolicy WRITE setFocusPolicy NOTIFY focusPolicyChanged)
    Q_PROPERTY(XwaylandCrashPolicy xwaylandCrashPolicy READ xwaylandCrashPolicy WRITE setXwaylandCrashPolicy NOTIFY xwaylandCrashPolicyChanged)
    Q_PROPERTY(int xwaylandMaxCrashCount READ xwaylandMaxCrashCount WRITE setXwaylandMaxCrashCount NOTIFY xwaylandMaxCrashCountChanged)
    Q_PROPERTY(bool nextFocusPrefersMouse READ isNextFocusPrefersMouse WRITE setNextFocusPrefersMouse NOTIFY nextFocusPrefersMouseChanged)
    /**
     * Whether clicking on a window raises it in FocusFollowsMouse
     * mode or not.
     */
    Q_PROPERTY(bool clickRaise READ isClickRaise WRITE setClickRaise NOTIFY clickRaiseChanged)
    /**
     * Whether autoraise is enabled FocusFollowsMouse mode or not.
     */
    Q_PROPERTY(bool autoRaise READ isAutoRaise WRITE setAutoRaise NOTIFY autoRaiseChanged)
    /**
     * Autoraise interval.
     */
    Q_PROPERTY(int autoRaiseInterval READ autoRaiseInterval WRITE setAutoRaiseInterval NOTIFY autoRaiseIntervalChanged)
    /**
     * Delayed focus interval.
     */
    Q_PROPERTY(int delayFocusInterval READ delayFocusInterval WRITE setDelayFocusInterval NOTIFY delayFocusIntervalChanged)
    /**
     * Whether shade hover is enabled or not.
     */
    Q_PROPERTY(bool shadeHover READ isShadeHover WRITE setShadeHover NOTIFY shadeHoverChanged)
    /**
     * Shade hover interval.
     */
    Q_PROPERTY(int shadeHoverInterval READ shadeHoverInterval WRITE setShadeHoverInterval NOTIFY shadeHoverIntervalChanged)
    /**
     * Whether to see Xinerama screens separately for focus (in Alt+Tab, when activating next client)
     */
    Q_PROPERTY(bool separateScreenFocus READ isSeparateScreenFocus WRITE setSeparateScreenFocus NOTIFY separateScreenFocusChanged)
    Q_PROPERTY(bool activeMouseScreen READ activeMouseScreen WRITE setActiveMouseScreen NOTIFY activeMouseScreenChanged)
    Q_PROPERTY(int placement READ placement WRITE setPlacement NOTIFY placementChanged)
    Q_PROPERTY(bool focusPolicyIsReasonable READ focusPolicyIsReasonable NOTIFY focusPolicyIsResonableChanged)
    /**
     * The size of the zone that triggers snapping on desktop borders.
     */
    Q_PROPERTY(int borderSnapZone READ borderSnapZone WRITE setBorderSnapZone NOTIFY borderSnapZoneChanged)
    /**
     * The size of the zone that triggers snapping with other windows.
     */
    Q_PROPERTY(int windowSnapZone READ windowSnapZone WRITE setWindowSnapZone NOTIFY windowSnapZoneChanged)
    /**
     * The size of the zone that triggers snapping on the screen center.
     */
    Q_PROPERTY(int centerSnapZone READ centerSnapZone WRITE setCenterSnapZone NOTIFY centerSnapZoneChanged)
    /**
     * Snap only when windows will overlap.
     */
    Q_PROPERTY(bool snapOnlyWhenOverlapping READ isSnapOnlyWhenOverlapping WRITE setSnapOnlyWhenOverlapping NOTIFY snapOnlyWhenOverlappingChanged)
    /**
     * Whether or not we roll over to the other edge when switching desktops past the edge.
     */
    Q_PROPERTY(bool rollOverDesktops READ isRollOverDesktops WRITE setRollOverDesktops NOTIFY rollOverDesktopsChanged)
    /**
     * 0 - 4 , see Workspace::allowClientActivation()
     */
    Q_PROPERTY(int focusStealingPreventionLevel READ focusStealingPreventionLevel WRITE setFocusStealingPreventionLevel NOTIFY focusStealingPreventionLevelChanged)
    Q_PROPERTY(KWin::Options::WindowOperation operationTitlebarDblClick READ operationTitlebarDblClick WRITE setOperationTitlebarDblClick NOTIFY operationTitlebarDblClickChanged)
    Q_PROPERTY(KWin::Options::WindowOperation operationMaxButtonLeftClick READ operationMaxButtonLeftClick WRITE setOperationMaxButtonLeftClick NOTIFY operationMaxButtonLeftClickChanged)
    Q_PROPERTY(KWin::Options::WindowOperation operationMaxButtonMiddleClick READ operationMaxButtonMiddleClick WRITE setOperationMaxButtonMiddleClick NOTIFY operationMaxButtonMiddleClickChanged)
    Q_PROPERTY(KWin::Options::WindowOperation operationMaxButtonRightClick READ operationMaxButtonRightClick WRITE setOperationMaxButtonRightClick NOTIFY operationMaxButtonRightClickChanged)
    Q_PROPERTY(MouseCommand commandActiveTitlebar1 READ commandActiveTitlebar1 WRITE setCommandActiveTitlebar1 NOTIFY commandActiveTitlebar1Changed)
    Q_PROPERTY(MouseCommand commandActiveTitlebar2 READ commandActiveTitlebar2 WRITE setCommandActiveTitlebar2 NOTIFY commandActiveTitlebar2Changed)
    Q_PROPERTY(MouseCommand commandActiveTitlebar3 READ commandActiveTitlebar3 WRITE setCommandActiveTitlebar3 NOTIFY commandActiveTitlebar3Changed)
    Q_PROPERTY(MouseCommand commandInactiveTitlebar1 READ commandInactiveTitlebar1 WRITE setCommandInactiveTitlebar1 NOTIFY commandInactiveTitlebar1Changed)
    Q_PROPERTY(MouseCommand commandInactiveTitlebar2 READ commandInactiveTitlebar2 WRITE setCommandInactiveTitlebar2 NOTIFY commandInactiveTitlebar2Changed)
    Q_PROPERTY(MouseCommand commandInactiveTitlebar3 READ commandInactiveTitlebar3 WRITE setCommandInactiveTitlebar3 NOTIFY commandInactiveTitlebar3Changed)
    Q_PROPERTY(MouseCommand commandWindow1 READ commandWindow1 WRITE setCommandWindow1 NOTIFY commandWindow1Changed)
    Q_PROPERTY(MouseCommand commandWindow2 READ commandWindow2 WRITE setCommandWindow2 NOTIFY commandWindow2Changed)
    Q_PROPERTY(MouseCommand commandWindow3 READ commandWindow3 WRITE setCommandWindow3 NOTIFY commandWindow3Changed)
    Q_PROPERTY(MouseCommand commandWindowWheel READ commandWindowWheel WRITE setCommandWindowWheel NOTIFY commandWindowWheelChanged)
    Q_PROPERTY(MouseCommand commandAll1 READ commandAll1 WRITE setCommandAll1 NOTIFY commandAll1Changed)
    Q_PROPERTY(MouseCommand commandAll2 READ commandAll2 WRITE setCommandAll2 NOTIFY commandAll2Changed)
    Q_PROPERTY(MouseCommand commandAll3 READ commandAll3 WRITE setCommandAll3 NOTIFY commandAll3Changed)
    Q_PROPERTY(uint keyCmdAllModKey READ keyCmdAllModKey WRITE setKeyCmdAllModKey NOTIFY keyCmdAllModKeyChanged)
    /**
     * Whether the visible name should be condensed.
     */
    Q_PROPERTY(bool condensedTitle READ condensedTitle WRITE setCondensedTitle NOTIFY condensedTitleChanged)
    /**
     * Whether a window gets maximized when it reaches top screen edge while being moved.
     */
    Q_PROPERTY(bool electricBorderMaximize READ electricBorderMaximize WRITE setElectricBorderMaximize NOTIFY electricBorderMaximizeChanged)
    /**
     * Whether a window is tiled to half screen when reaching left or right screen edge while been moved.
     */
    Q_PROPERTY(bool electricBorderTiling READ electricBorderTiling WRITE setElectricBorderTiling NOTIFY electricBorderTilingChanged)
    /**
     * Whether a window is tiled to half screen when reaching left or right screen edge while been moved.
     */
    Q_PROPERTY(float electricBorderCornerRatio READ electricBorderCornerRatio WRITE setElectricBorderCornerRatio NOTIFY electricBorderCornerRatioChanged)
    Q_PROPERTY(bool borderlessMaximizedWindows READ borderlessMaximizedWindows WRITE setBorderlessMaximizedWindows NOTIFY borderlessMaximizedWindowsChanged)
    /**
     * timeout before non-responding application will be killed after attempt to close.
     */
    Q_PROPERTY(int killPingTimeout READ killPingTimeout WRITE setKillPingTimeout NOTIFY killPingTimeoutChanged)
    /**
     * Whether to hide utility windows for inactive applications.
     */
    Q_PROPERTY(bool hideUtilityWindowsForInactive READ isHideUtilityWindowsForInactive WRITE setHideUtilityWindowsForInactive NOTIFY hideUtilityWindowsForInactiveChanged)
    Q_PROPERTY(int compositingMode READ compositingMode WRITE setCompositingMode NOTIFY compositingModeChanged)
    Q_PROPERTY(bool useCompositing READ isUseCompositing WRITE setUseCompositing NOTIFY useCompositingChanged)
    Q_PROPERTY(int hiddenPreviews READ hiddenPreviews WRITE setHiddenPreviews NOTIFY hiddenPreviewsChanged)
    /**
     * 0 = no, 1 = yes when transformed,
     * 2 = try trilinear when transformed; else 1,
     * -1 = auto
     */
    Q_PROPERTY(int glSmoothScale READ glSmoothScale WRITE setGlSmoothScale NOTIFY glSmoothScaleChanged)
    Q_PROPERTY(bool glStrictBinding READ isGlStrictBinding WRITE setGlStrictBinding NOTIFY glStrictBindingChanged)
    /**
     * Whether strict binding follows the driver or has been overwritten by a user defined config value.
     * If @c true glStrictBinding is set by the OpenGL Scene during initialization.
     * If @c false glStrictBinding is set from a config value and not updated during scene initialization.
     */
    Q_PROPERTY(bool glStrictBindingFollowsDriver READ isGlStrictBindingFollowsDriver WRITE setGlStrictBindingFollowsDriver NOTIFY glStrictBindingFollowsDriverChanged)
    Q_PROPERTY(GlSwapStrategy glPreferBufferSwap READ glPreferBufferSwap WRITE setGlPreferBufferSwap NOTIFY glPreferBufferSwapChanged)
    Q_PROPERTY(KWin::OpenGLPlatformInterface glPlatformInterface READ glPlatformInterface WRITE setGlPlatformInterface NOTIFY glPlatformInterfaceChanged)
    Q_PROPERTY(bool windowsBlockCompositing READ windowsBlockCompositing WRITE setWindowsBlockCompositing NOTIFY windowsBlockCompositingChanged)
    Q_PROPERTY(bool unredirectFullscreen READ unredirectFullscreen WRITE setUnredirectFullscreen NOTIFY unredirectFullscreenChanged)
    Q_PROPERTY(bool drmDirectScanout READ drmDirectScanout WRITE setDrmDirectScanout NOTIFY drmDirectScanoutChanged)
    Q_PROPERTY(bool debugUnredirect READ debugUnredirect WRITE setDebugUnredirect NOTIFY debugUnredirectChanged)
    Q_PROPERTY(bool unredirectNonOpaque READ unredirectNonOpaque WRITE setUnredirectNonOpaque NOTIFY unredirectNonOpaqueChanged)
    Q_PROPERTY(bool openGLIsAlwaysSafe READ openGLIsAlwaysSafe WRITE setOpenGLIsAlwaysSafe NOTIFY openGLIsAlwaysSafeChanged)
    Q_PROPERTY(bool setMaxFramesAllowed READ setMaxFramesAllowed WRITE setSetMaxFramesAllowed NOTIFY setMaxFramesAllowedChanged)
    Q_PROPERTY(bool debugCompositeTimer READ debugCompositeTimer WRITE setDebugCompositeTimer NOTIFY debugCompositeTimerChanged)
    Q_PROPERTY(int syncWindowX READ syncWindowX WRITE setSyncWindowX NOTIFY syncWindowXChanged)
    Q_PROPERTY(int syncWindowY READ syncWindowY WRITE setSyncWindowY NOTIFY syncWindowYChanged)
    Q_PROPERTY(bool forceDisableVSync READ forceDisableVSync WRITE setForceDisableVSync NOTIFY forceDisableVSyncChanged)
    Q_PROPERTY(LatencyPolicy latencyPolicy READ latencyPolicy WRITE setLatencyPolicy NOTIFY latencyPolicyChanged)
    Q_PROPERTY(RenderTimeEstimator renderTimeEstimator READ renderTimeEstimator WRITE setRenderTimeEstimator NOTIFY renderTimeEstimatorChanged)
    Q_PROPERTY(VSyncMechanism vSyncMechanism READ vSyncMechanism WRITE setVSyncMechanism NOTIFY vSyncMechanismChanged)
    Q_PROPERTY(CrashAction crashAction READ crashAction WRITE setCrashAction NOTIFY crashActionChanged)
public:

    explicit Options(QObject *parent = nullptr);
    ~Options() override;

    void updateSettings();

    /**
     * This enum type is used to specify the focus policy.
     *
     * Note that FocusUnderMouse and FocusStrictlyUnderMouse are not
     * particulary useful. They are only provided for old-fashined
     * die-hard UNIX people ;-)
     */
    enum FocusPolicy {
        /**
         * Clicking into a window activates it. This is also the default.
         */
        ClickToFocus,
        /**
         * Moving the mouse pointer actively onto a normal window activates it.
         * For convenience, the desktop and windows on the dock are excluded.
         * They require clicking.
         */
        FocusFollowsMouse,
        /**
         * The window that happens to be under the mouse pointer becomes active.
         * The invariant is: no window can have focus that is not under the mouse.
         * This also means that Alt-Tab won't work properly and popup dialogs are
         * usually unsable with the keyboard. Note that the desktop and windows on
         * the dock are excluded for convenience. They get focus only when clicking
         * on it.
         */
        FocusUnderMouse,
        /**
         * This is even worse than FocusUnderMouse. Only the window under the mouse
         * pointer is active. If the mouse points nowhere, nothing has the focus. If
         * the mouse points onto the desktop, the desktop has focus. The same holds
         * for windows on the dock.
         */
        FocusStrictlyUnderMouse
    };
    Q_ENUM(FocusPolicy)

    FocusPolicy focusPolicy() const {
        return m_focusPolicy;
    }
    bool isNextFocusPrefersMouse() const {
        return m_nextFocusPrefersMouse;
    }

    XwaylandCrashPolicy xwaylandCrashPolicy() const {
        return m_xwaylandCrashPolicy;
    }
    int xwaylandMaxCrashCount() const {
        return m_xwaylandMaxCrashCount;
    }

    /**
     * Whether clicking on a window raises it in FocusFollowsMouse
     * mode or not.
     */
    bool isClickRaise() const {
        return m_clickRaise;
    }

    /**
     * Whether autoraise is enabled FocusFollowsMouse mode or not.
     */
    bool isAutoRaise() const {
        return m_autoRaise;
    }

    /**
     * Autoraise interval
     */
    int autoRaiseInterval() const {
        return m_autoRaiseInterval;
    }

    /**
     * Delayed focus interval.
     */
    int delayFocusInterval() const {
        return m_delayFocusInterval;
    }

    /**
     * Whether shade hover is enabled or not.
     */
    bool isShadeHover() const {
        return m_shadeHover;
    }

    /**
     * Shade hover interval.
     */
    int shadeHoverInterval() {
        return m_shadeHoverInterval;
    }

    /**
     * Whether to see Xinerama screens separately for focus (in Alt+Tab, when activating next client)
     */
    bool isSeparateScreenFocus() const {
        return m_separateScreenFocus;
    }

    bool activeMouseScreen() const {
        return m_activeMouseScreen;
    }

    Placement::Policy placement() const {
        return m_placement;
    }

    bool focusPolicyIsReasonable() {
        return m_focusPolicy == ClickToFocus || m_focusPolicy == FocusFollowsMouse;
    }

    /**
     * The size of the zone that triggers snapping on desktop borders.
     */
    int borderSnapZone() const {
        return m_borderSnapZone;
    }

    /**
     * The size of the zone that triggers snapping with other windows.
     */
    int windowSnapZone() const {
        return m_windowSnapZone;
    }

    /**
     * The size of the zone that triggers snapping on the screen center.
     */
    int centerSnapZone() const {
        return m_centerSnapZone;
    }


    /**
     * Snap only when windows will overlap.
     */
    bool isSnapOnlyWhenOverlapping() const {
        return m_snapOnlyWhenOverlapping;
    }

    /**
     * Whether or not we roll over to the other edge when switching desktops past the edge.
     */
    bool isRollOverDesktops() const {
        return m_rollOverDesktops;
    }

    /**
     * Returns the focus stealing prevention level.
     *
     * @see allowClientActivation
     */
    int focusStealingPreventionLevel() const {
        return m_focusStealingPreventionLevel;
    }

    enum WindowOperation {
        MaximizeOp = 5000,
        RestoreOp,
        MinimizeOp,
        MoveOp,
        UnrestrictedMoveOp,
        ResizeOp,
        UnrestrictedResizeOp,
        CloseOp,
        OnAllDesktopsOp,
        ShadeOp,
        KeepAboveOp,
        KeepBelowOp,
        OperationsOp,
        WindowRulesOp,
        ToggleStoreSettingsOp = WindowRulesOp, ///< @obsolete
        HMaximizeOp,
        VMaximizeOp,
        LowerOp,
        FullScreenOp,
        NoBorderOp,
        NoOp,
        SetupWindowShortcutOp,
        ApplicationRulesOp,
    };
    Q_ENUM(WindowOperation)

    WindowOperation operationTitlebarDblClick() const {
        return OpTitlebarDblClick;
    }
    WindowOperation operationMaxButtonLeftClick() const {
        return opMaxButtonLeftClick;
    }
    WindowOperation operationMaxButtonRightClick() const {
        return opMaxButtonRightClick;
    }
    WindowOperation operationMaxButtonMiddleClick() const {
        return opMaxButtonMiddleClick;
    }
    WindowOperation operationMaxButtonClick(Qt::MouseButtons button) const;


    enum MouseCommand {
        MouseRaise, MouseLower, MouseOperationsMenu, MouseToggleRaiseAndLower,
        MouseActivateAndRaise, MouseActivateAndLower, MouseActivate,
        MouseActivateRaiseAndPassClick, MouseActivateAndPassClick,
        MouseMove, MouseUnrestrictedMove,
        MouseActivateRaiseAndMove, MouseActivateRaiseAndUnrestrictedMove,
        MouseResize, MouseUnrestrictedResize,
        MouseShade, MouseSetShade, MouseUnsetShade,
        MouseMaximize, MouseRestore, MouseMinimize,
        MouseNextDesktop, MousePreviousDesktop,
        MouseAbove, MouseBelow,
        MouseOpacityMore, MouseOpacityLess,
        MouseClose,
        MouseNothing
    };
    Q_ENUM(MouseCommand)

    enum MouseWheelCommand {
        MouseWheelRaiseLower, MouseWheelShadeUnshade, MouseWheelMaximizeRestore,
        MouseWheelAboveBelow, MouseWheelPreviousNextDesktop,
        MouseWheelChangeOpacity,
        MouseWheelNothing
    };
    Q_ENUM(MouseWheelCommand)

    MouseCommand operationTitlebarMouseWheel(int delta) const {
        return wheelToMouseCommand(CmdTitlebarWheel, delta);
    }
    MouseCommand operationWindowMouseWheel(int delta) const {
        return wheelToMouseCommand(CmdAllWheel, delta);
    }

    MouseCommand commandActiveTitlebar1() const {
        return CmdActiveTitlebar1;
    }
    MouseCommand commandActiveTitlebar2() const {
        return CmdActiveTitlebar2;
    }
    MouseCommand commandActiveTitlebar3() const {
        return CmdActiveTitlebar3;
    }
    MouseCommand commandInactiveTitlebar1() const {
        return CmdInactiveTitlebar1;
    }
    MouseCommand commandInactiveTitlebar2() const {
        return CmdInactiveTitlebar2;
    }
    MouseCommand commandInactiveTitlebar3() const {
        return CmdInactiveTitlebar3;
    }
    MouseCommand commandWindow1() const {
        return CmdWindow1;
    }
    MouseCommand commandWindow2() const {
        return CmdWindow2;
    }
    MouseCommand commandWindow3() const {
        return CmdWindow3;
    }
    MouseCommand commandWindowWheel() const {
        return CmdWindowWheel;
    }
    MouseCommand commandAll1() const {
        return CmdAll1;
    }
    MouseCommand commandAll2() const {
        return CmdAll2;
    }
    MouseCommand commandAll3() const {
        return CmdAll3;
    }
    MouseWheelCommand commandAllWheel() const {
        return CmdAllWheel;
    }
    uint keyCmdAllModKey() const {
        return CmdAllModKey;
    }
    Qt::KeyboardModifier commandAllModifier() const {
        switch (CmdAllModKey) {
        case Qt::Key_Alt:
            return Qt::AltModifier;
        case Qt::Key_Meta:
            return Qt::MetaModifier;
        default:
            Q_UNREACHABLE();
        }
    }

    static WindowOperation windowOperation(const QString &name, bool restricted);
    static MouseCommand mouseCommand(const QString &name, bool restricted);
    static MouseWheelCommand mouseWheelCommand(const QString &name);

    /**
     * Returns whether the user prefers his caption clean.
     */
    bool condensedTitle() const;

    /**
     * @returns true if a window gets maximized when it reaches top screen edge
     * while being moved.
     */
    bool electricBorderMaximize() const {
        return electric_border_maximize;
    }
    /**
     * @returns true if window is tiled to half screen when reaching left or
     * right screen edge while been moved.
     */
    bool electricBorderTiling() const {
        return electric_border_tiling;
    }
    /**
     * @returns the factor that determines the corner part of the edge (ie. 0.1 means tiny corner)
     */
    float electricBorderCornerRatio() const {
        return electric_border_corner_ratio;
    }

    bool borderlessMaximizedWindows() const {
        return borderless_maximized_windows;
    }

    /**
     * Timeout before non-responding application will be killed after attempt to close.
     */
    int killPingTimeout() const {
        return m_killPingTimeout;
    }

    /**
     * Whether to hide utility windows for inactive applications.
     */
    bool isHideUtilityWindowsForInactive() const {
        return m_hideUtilityWindowsForInactive;
    }

    /**
     * Returns the animation time factor for desktop effects.
     */
    double animationTimeFactor() const;

    //----------------------
    // Compositing settings
    void reloadCompositingSettings(bool force = false);
    CompositingType compositingMode() const {
        return m_compositingMode;
    }
    void setCompositingMode(CompositingType mode) {
        m_compositingMode = mode;
    }
    // Separate to mode so the user can toggle
    bool isUseCompositing() const;

    // General preferences
    HiddenPreviews hiddenPreviews() const {
        return m_hiddenPreviews;
    }
    // OpenGL
    // 1 = yes,
    // 2 = try trilinear when transformed; else 1,
    // -1 = auto
    int glSmoothScale() const {
        return m_glSmoothScale;
    }

    // Settings that should be auto-detected
    bool isGlStrictBinding() const {
        return m_glStrictBinding;
    }
    bool isGlStrictBindingFollowsDriver() const {
        return m_glStrictBindingFollowsDriver;
    }
    OpenGLPlatformInterface glPlatformInterface() const {
        return m_glPlatformInterface;
    }

    enum GlSwapStrategy {
        CopyFrontBuffer = 'c',
        PaintFullScreen = 'p',
        ExtendDamage = 'e',
        AutoSwapStrategy = 'a',
    };
    Q_ENUM(GlSwapStrategy)
    GlSwapStrategy glPreferBufferSwap() const {
        return m_glPreferBufferSwap;
    }

    bool windowsBlockCompositing() const
    {
        return m_windowsBlockCompositing;
    }

    bool unredirectFullscreen() const
    {
        return m_unredirectFullscreen;
    }

    bool drmDirectScanout() const
    {
        return m_drmDirectScanout;
    }

    bool debugUnredirect() const
    {
        return m_debugUnredirect;
    }

    bool unredirectNonOpaque() const
    {
        return m_unredirectNonOpaque;
    }

    bool openGLIsAlwaysSafe() const
    {
        return m_openGLIsAlwaysSafe;
    }

    bool setMaxFramesAllowed() const
    {
        return m_setMaxFramesAllowed;
    }

    bool debugCompositeTimer() const
    {
        return m_debugCompositeTimer;
    }

    int syncWindowX() const
    {
        return m_syncWindowX;
    }

    int syncWindowY() const
    {
        return m_syncWindowY;
    }

    bool forceDisableVSync() const
    {
        return m_forceDisableVSync;
    }

    bool moveMinimizedWindowsToEndOfTabBoxFocusChain() const {
        return m_MoveMinimizedWindowsToEndOfTabBoxFocusChain;
    }

    QStringList modifierOnlyDBusShortcut(Qt::KeyboardModifier mod) const;
    LatencyPolicy latencyPolicy() const;
    RenderTimeEstimator renderTimeEstimator() const;
    VSyncMechanism vSyncMechanism() const;
    CrashAction crashAction() const;

    // setters
    void setFocusPolicy(FocusPolicy focusPolicy);
    void setXwaylandCrashPolicy(XwaylandCrashPolicy crashPolicy);
    void setXwaylandMaxCrashCount(int maxCrashCount);
    void setNextFocusPrefersMouse(bool nextFocusPrefersMouse);
    void setClickRaise(bool clickRaise);
    void setAutoRaise(bool autoRaise);
    void setAutoRaiseInterval(int autoRaiseInterval);
    void setDelayFocusInterval(int delayFocusInterval);
    void setShadeHover(bool shadeHover);
    void setShadeHoverInterval(int shadeHoverInterval);
    void setSeparateScreenFocus(bool separateScreenFocus);
    void setActiveMouseScreen(bool activeMouseScreen);
    void setPlacement(int placement);
    void setBorderSnapZone(int borderSnapZone);
    void setWindowSnapZone(int windowSnapZone);
    void setCenterSnapZone(int centerSnapZone);
    void setSnapOnlyWhenOverlapping(bool snapOnlyWhenOverlapping);
    void setRollOverDesktops(bool rollOverDesktops);
    void setFocusStealingPreventionLevel(int focusStealingPreventionLevel);
    void setOperationTitlebarDblClick(WindowOperation operationTitlebarDblClick);
    void setOperationMaxButtonLeftClick(WindowOperation op);
    void setOperationMaxButtonRightClick(WindowOperation op);
    void setOperationMaxButtonMiddleClick(WindowOperation op);
    void setCommandActiveTitlebar1(MouseCommand commandActiveTitlebar1);
    void setCommandActiveTitlebar2(MouseCommand commandActiveTitlebar2);
    void setCommandActiveTitlebar3(MouseCommand commandActiveTitlebar3);
    void setCommandInactiveTitlebar1(MouseCommand commandInactiveTitlebar1);
    void setCommandInactiveTitlebar2(MouseCommand commandInactiveTitlebar2);
    void setCommandInactiveTitlebar3(MouseCommand commandInactiveTitlebar3);
    void setCommandWindow1(MouseCommand commandWindow1);
    void setCommandWindow2(MouseCommand commandWindow2);
    void setCommandWindow3(MouseCommand commandWindow3);
    void setCommandWindowWheel(MouseCommand commandWindowWheel);
    void setCommandAll1(MouseCommand commandAll1);
    void setCommandAll2(MouseCommand commandAll2);
    void setCommandAll3(MouseCommand commandAll3);
    void setKeyCmdAllModKey(uint keyCmdAllModKey);
    void setCondensedTitle(bool condensedTitle);
    void setElectricBorderMaximize(bool electricBorderMaximize);
    void setElectricBorderTiling(bool electricBorderTiling);
    void setElectricBorderCornerRatio(float electricBorderCornerRatio);
    void setBorderlessMaximizedWindows(bool borderlessMaximizedWindows);
    void setKillPingTimeout(int killPingTimeout);
    void setHideUtilityWindowsForInactive(bool hideUtilityWindowsForInactive);
    void setCompositingMode(int compositingMode);
    void setUseCompositing(bool useCompositing);
    void setHiddenPreviews(int hiddenPreviews);
    void setGlSmoothScale(int glSmoothScale);
    void setGlStrictBinding(bool glStrictBinding);
    void setGlStrictBindingFollowsDriver(bool glStrictBindingFollowsDriver);
    void setGlPreferBufferSwap(char glPreferBufferSwap);
    void setGlPlatformInterface(OpenGLPlatformInterface interface);
    void setWindowsBlockCompositing(bool set);
    void setUnredirectFullscreen(bool set);
    void setDrmDirectScanout(bool set);
    void setDebugUnredirect(bool set);
    void setUnredirectNonOpaque(bool set);
    void setOpenGLIsAlwaysSafe(bool set);
    void setSetMaxFramesAllowed(bool set);
    void setDebugCompositeTimer(bool set);
    void setSyncWindowX(int value);
    void setSyncWindowY(int value);
    void setForceDisableVSync(bool set);
    void setMoveMinimizedWindowsToEndOfTabBoxFocusChain(bool set);
    void setLatencyPolicy(LatencyPolicy policy);
    void setRenderTimeEstimator(RenderTimeEstimator estimator);
    void setVSyncMechanism(VSyncMechanism mechanism);
    void setCrashAction(CrashAction action);

    // default values
    static WindowOperation defaultOperationTitlebarDblClick() {
        return MaximizeOp;
    }
    static WindowOperation defaultOperationMaxButtonLeftClick() {
        return MaximizeOp;
    }
    static WindowOperation defaultOperationMaxButtonRightClick() {
        return HMaximizeOp;
    }
    static WindowOperation defaultOperationMaxButtonMiddleClick() {
        return VMaximizeOp;
    }
    static MouseCommand defaultCommandActiveTitlebar1() {
        return MouseRaise;
    }
    static MouseCommand defaultCommandActiveTitlebar2() {
        return MouseNothing;
    }
    static MouseCommand defaultCommandActiveTitlebar3() {
        return MouseOperationsMenu;
    }
    static MouseCommand defaultCommandInactiveTitlebar1() {
        return MouseActivateAndRaise;
    }
    static MouseCommand defaultCommandInactiveTitlebar2() {
        return MouseNothing;
    }
    static MouseCommand defaultCommandInactiveTitlebar3() {
        return MouseOperationsMenu;
    }
    static MouseCommand defaultCommandWindow1() {
        return MouseActivateRaiseAndPassClick;
    }
    static MouseCommand defaultCommandWindow2() {
        return MouseActivateAndPassClick;
    }
    static MouseCommand defaultCommandWindow3() {
        return MouseActivateAndPassClick;
    }
    static MouseCommand defaultCommandWindowWheel() {
        return MouseNothing;
    }
    static MouseCommand defaultCommandAll1() {
        return MouseUnrestrictedMove;
    }
    static MouseCommand defaultCommandAll2() {
        return MouseToggleRaiseAndLower;
    }
    static MouseCommand defaultCommandAll3() {
        return MouseUnrestrictedResize;
    }
    static MouseWheelCommand defaultCommandTitlebarWheel() {
        return MouseWheelNothing;
    }
    static MouseWheelCommand defaultCommandAllWheel() {
        return MouseWheelNothing;
    }
    static uint defaultKeyCmdAllModKey() {
        return Qt::Key_Alt;
    }
    static CompositingType defaultCompositingMode() {
        return OpenGLCompositing;
    }
    static bool defaultUseCompositing() {
        return true;
    }
    static HiddenPreviews defaultHiddenPreviews() {
        return HiddenPreviewsShown;
    }
    static int defaultGlSmoothScale() {
        return 2;
    }
    static bool defaultGlStrictBinding() {
        return true;
    }
    static bool defaultGlStrictBindingFollowsDriver() {
        return true;
    }
    static GlSwapStrategy defaultGlPreferBufferSwap() {
        return AutoSwapStrategy;
    }
    static OpenGLPlatformInterface defaultGlPlatformInterface() {
        return kwinApp()->shouldUseWaylandForCompositing() ? EglPlatformInterface : GlxPlatformInterface;
    }
    static XwaylandCrashPolicy defaultXwaylandCrashPolicy() {
        return XwaylandCrashPolicy::Restart;
    }
    static int defaultXwaylandMaxCrashCount() {
        return 3;
    }
    static LatencyPolicy defaultLatencyPolicy() {
        return LatencyMedium;
    }
    static RenderTimeEstimator defaultRenderTimeEstimator() {
        return RenderTimeEstimatorMaximum;
    }
    static VSyncMechanism defaultVSyncMechanism() {
        return VSyncMechanismAuto;
    }
    static CrashAction defaultCrashAction() {
        return CrashActionRestart;
    }
    /**
     * Performs loading all settings except compositing related.
     */
    void loadConfig();
    /**
     * Performs loading of compositing settings which do not depend on OpenGL.
     */
    bool loadCompositingConfig(bool force);
    void reparseConfiguration();

    //----------------------
Q_SIGNALS:
    // for properties
    void focusPolicyChanged();
    void focusPolicyIsResonableChanged();
    void xwaylandCrashPolicyChanged();
    void xwaylandMaxCrashCountChanged();
    void nextFocusPrefersMouseChanged();
    void clickRaiseChanged();
    void autoRaiseChanged();
    void autoRaiseIntervalChanged();
    void delayFocusIntervalChanged();
    void shadeHoverChanged();
    void shadeHoverIntervalChanged();
    void separateScreenFocusChanged(bool);
    void activeMouseScreenChanged();
    void placementChanged();
    void borderSnapZoneChanged();
    void windowSnapZoneChanged();
    void centerSnapZoneChanged();
    void snapOnlyWhenOverlappingChanged();
    void rollOverDesktopsChanged(bool enabled);
    void focusStealingPreventionLevelChanged();
    void operationTitlebarDblClickChanged();
    void operationMaxButtonLeftClickChanged();
    void operationMaxButtonRightClickChanged();
    void operationMaxButtonMiddleClickChanged();
    void commandActiveTitlebar1Changed();
    void commandActiveTitlebar2Changed();
    void commandActiveTitlebar3Changed();
    void commandInactiveTitlebar1Changed();
    void commandInactiveTitlebar2Changed();
    void commandInactiveTitlebar3Changed();
    void commandWindow1Changed();
    void commandWindow2Changed();
    void commandWindow3Changed();
    void commandWindowWheelChanged();
    void commandAll1Changed();
    void commandAll2Changed();
    void commandAll3Changed();
    void keyCmdAllModKeyChanged();
    void condensedTitleChanged();
    void electricBorderMaximizeChanged();
    void electricBorderTilingChanged();
    void electricBorderCornerRatioChanged();
    void borderlessMaximizedWindowsChanged();
    void killPingTimeoutChanged();
    void hideUtilityWindowsForInactiveChanged();
    void compositingModeChanged();
    void useCompositingChanged();
    void hiddenPreviewsChanged();
    void glSmoothScaleChanged();
    void glStrictBindingChanged();
    void glStrictBindingFollowsDriverChanged();
    void glPreferBufferSwapChanged();
    void glPlatformInterfaceChanged();
    void windowsBlockCompositingChanged();
    void unredirectFullscreenChanged();
    void drmDirectScanoutChanged();
    void debugUnredirectChanged();
    void unredirectNonOpaqueChanged();
    void openGLIsAlwaysSafeChanged();
    void setMaxFramesAllowedChanged();
    void debugCompositeTimerChanged();
    void syncWindowXChanged();
    void syncWindowYChanged();
    void forceDisableVSyncChanged();
    void animationSpeedChanged();
    void latencyPolicyChanged();
    void configChanged();
    void renderTimeEstimatorChanged();
    void vSyncMechanismChanged();
    void crashActionChanged();

private:
    void setElectricBorders(int borders);
    void syncFromKcfgc();
    QScopedPointer<Settings> m_settings;
    KConfigWatcher::Ptr m_configWatcher;

    FocusPolicy m_focusPolicy;
    bool m_nextFocusPrefersMouse;
    bool m_clickRaise;
    bool m_autoRaise;
    int m_autoRaiseInterval;
    int m_delayFocusInterval;
    bool m_shadeHover;
    int m_shadeHoverInterval;
    bool m_separateScreenFocus;
    bool m_activeMouseScreen;
    Placement::Policy m_placement;
    int m_borderSnapZone;
    int m_windowSnapZone;
    int m_centerSnapZone;
    bool m_snapOnlyWhenOverlapping;
    bool m_rollOverDesktops;
    int m_focusStealingPreventionLevel;
    int m_killPingTimeout;
    bool m_hideUtilityWindowsForInactive;
    XwaylandCrashPolicy m_xwaylandCrashPolicy;
    int m_xwaylandMaxCrashCount;
    LatencyPolicy m_latencyPolicy;
    RenderTimeEstimator m_renderTimeEstimator;
    VSyncMechanism m_vSyncMechanism;
    CrashAction m_crashAction;

    CompositingType m_compositingMode;
    bool m_useCompositing;
    HiddenPreviews m_hiddenPreviews;
    int m_glSmoothScale;
    // Settings that should be auto-detected
    bool m_glStrictBinding;
    bool m_glStrictBindingFollowsDriver;
    GlSwapStrategy m_glPreferBufferSwap;
    OpenGLPlatformInterface m_glPlatformInterface;
    bool m_windowsBlockCompositing;
    bool m_unredirectFullscreen;
    bool m_drmDirectScanout;
    bool m_debugUnredirect;
    bool m_unredirectNonOpaque;
    bool m_openGLIsAlwaysSafe;
    bool m_setMaxFramesAllowed;
    bool m_debugCompositeTimer;
    int m_syncWindowX;
    int m_syncWindowY;
    bool m_forceDisableVSync;
    bool m_MoveMinimizedWindowsToEndOfTabBoxFocusChain;

    WindowOperation OpTitlebarDblClick;
    WindowOperation opMaxButtonRightClick = defaultOperationMaxButtonRightClick();
    WindowOperation opMaxButtonMiddleClick = defaultOperationMaxButtonMiddleClick();
    WindowOperation opMaxButtonLeftClick = defaultOperationMaxButtonRightClick();

    // mouse bindings
    MouseCommand CmdActiveTitlebar1;
    MouseCommand CmdActiveTitlebar2;
    MouseCommand CmdActiveTitlebar3;
    MouseCommand CmdInactiveTitlebar1;
    MouseCommand CmdInactiveTitlebar2;
    MouseCommand CmdInactiveTitlebar3;
    MouseWheelCommand CmdTitlebarWheel;
    MouseCommand CmdWindow1;
    MouseCommand CmdWindow2;
    MouseCommand CmdWindow3;
    MouseCommand CmdWindowWheel;
    MouseCommand CmdAll1;
    MouseCommand CmdAll2;
    MouseCommand CmdAll3;
    MouseWheelCommand CmdAllWheel;
    uint CmdAllModKey;

    bool electric_border_maximize;
    bool electric_border_tiling;
    float electric_border_corner_ratio;
    bool borderless_maximized_windows;
    bool condensed_title;

    QHash<Qt::KeyboardModifier, QStringList> m_modifierOnlyShortcuts;

    MouseCommand wheelToMouseCommand(MouseWheelCommand com, int delta) const;
};

extern KWIN_EXPORT Options* options;

} // namespace

Q_DECLARE_METATYPE(KWin::Options::WindowOperation)
Q_DECLARE_METATYPE(KWin::OpenGLPlatformInterface)

#endif
