# KWin-lowlatency

KWin-lowlatency is my attempt to reduce latency and stuttering in the popular KWin compositor used in KDE.

## background

stock KWin has a major issue regarding stuttering. it stutters heavily, and if you don't want that, then you have the input latency problem (of up to 50ms!).

the rationale for such a problem is that KWin uses a weird method to paint the screen.
instead of simply sync'ing to vblank, it uses a timer. yeah, a **timer** that is off-sync with the vblank interval.
you can prove this by disabling VSync in the system settings. you'll see just 1 line of tearing in your screen, and well, that's the time it swaps due to the timer.

in order to "fix" this under vanilla KWin, a typical solution is to insert this in kwinrc:

```
MaxFPS=200
RefreshRate=200
```

effectively making the timer faster than the actual screen's rate, forcing VSync at some point.

however, this introduces additional input lag, which varies depending on your driver, but usually it's 50ms (still too high).
you can prove this by moving a window. you'll see the cursor being ahead of the title bar, or at least so under X.

so, how to fix this? let's ditch the timer and let us access the VBlank interval directly.

but how do we do that? by using glXWaitVideoSync.

the only problem is that this only works under GLX (which means no X with EGL support, OpenGL ES or Wayland support).

however, this is much better than the formerly-employed DRM approach, since you don't have to deal anymore with the code if you had multiple cards.

now, by doing this, we have a proper desktop without stuttering, but the input lag persists...

after digging deep into the code, i found this piece of code in particular, which is pretty much the culprit:

```
if (!blocksForRetrace()) {
  // This also sets lastDamage to empty which prevents the frame from
  // being posted again when prepareRenderingFrame() is called.
  present();
} else {
  // Make sure that the GPU begins processing the command stream
  // now and not the next time prepareRenderingFrame() is called.
  glFlush();
}
```

by removing this code and simply presenting as soon as possible (we're blocking for retrace anyway due to the glXWaitVideoSync thingy), we cut off 1 whole frame of lag!

but hey, can we go further? yes, of course!

now, by sleeping for a very few milliseconds (up to 8 in high-end systems) the compositor has more time for user input before rendering, which further reduces input lag.

the reason why only up to 8ms is because any further would leave little room for rendering, and that will actually produce more stuttering than fix it.

## KWin-lowlatency is not...

* perfect. it tries its best to deliver low-latency no-stutter video, but I can't promise this is always the case.
  as an example, it will stutter if you select another window, or if you have too many windows open.
* truly designed for low-end systems. if you use KWin-lowlatency in one of them, you may experience stuttering.

# contacting original KWin development team

 * mailing list: [kwin@kde.org](https://mail.kde.org/mailman/listinfo/kwin)
 * IRC: #kwin on freenode

# support

use the issues section at the top.

# bugs?

use the issues section at the top. **but wait!** before reporting an issue, first test it again on [the official KWin](https://cgit.kde.org/kwin.git/) to determine whether this is really a KWin-lowlatency-only bug.

if the bug also reproduces in official KWin, please file a bug on [their bug tracker](https://bugs.kde.org/enter_bug.cgi?product=kwin) instead.

# Developing on KWin

Please refer to [hacking documentation](HACKING.md) for how to build and start KWin. Further information about KWin's test suite can be found in [TESTING.md](TESTING.md).

no, sorry, there are no tests for the low-latency features yet.
