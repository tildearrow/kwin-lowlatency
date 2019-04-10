# note for NVIDIA users

this will most likely not work. sorry. I don't have an NVIDIA card to test with so I can't ensure it will work for you.
please see [bug #2](https://github.com/tildearrow/kwin-lowlatency/issues/2).

# KWin-lowlatency

KWin-lowlatency is my attempt to reduce latency and stuttering in the popular KWin compositor used in KDE.

## background

stock KWin has a major issue regarding stuttering. it stutters heavily, and if you don't want that, then you have the input latency problem (of up to 50ms! (probably even worse on NVIDIA cards!)).

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

but how do we do that? there are 2 methods. both have flaws though:

1. GLX_OML_sync_control
2. DRM (Direct Rendering Manager)

the first one has the problem of working only under GLX (which means on X with EGL support, OpenGL ES or Wayland support), but works everywhere and no tinkering is necessary if you have multiple cards.

the second one in the other hand works everywhere (although I only tested with GLX), but requires addressing the card at a lower level than X, and may require tinkering if multiple cards are present.

I went for the second one. now, by doing this, we have a proper desktop without stuttering, but hey, can we go further? yes, of course!

now, by sleeping for a very few milliseconds (up to 10) the compositor has more time for user input before rendering, which further reduces input lag.

the reason why only up to 10ms is because any further would leave little room for rendering, and that will actually produce more stuttering than fix it.

## KWin-lowlatency is not...

* perfect. it tries its best to deliver low-latency no-stutter video, but I can't promise this is always the case.
  as an example, it will stutter if you select another window, or if you have too many windows open.
  I eventually will think of some approach to fix this, however.

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
