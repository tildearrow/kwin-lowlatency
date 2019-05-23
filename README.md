# KWin-lowlatency

KWin-lowlatency is my attempt to reduce latency and stuttering in the popular KWin compositor used in KDE.

## background

stock KWin has a major issue regarding stuttering. it stutters heavily, and if you don't want that, then you have the latency problem (of up to 50ms!).

the rationale for such a problem is that KWin uses a weird method to paint the screen.
instead of simply sync'ing to vblank, it uses a timer. yeah, a **timer** that is off-sync with the vblank interval.
you can prove this by disabling VSync in the system settings. you'll see just 1 line of tearing in your screen, and well, that's the time it swaps due to the timer.

in order to "fix" this under vanilla KWin, a typical solution is to insert this in kwinrc:

```
MaxFPS=200
RefreshRate=200
```

effectively making the timer faster than the actual screen's rate, forcing VSync at some point.

however, this introduces additional output lag, which varies depending on your driver, but usually it's 50ms (still too high).
you can prove this by moving a window. you'll see the cursor being ahead of the title bar, or at least so under X.

so, how to fix this? let's ditch the timer and let us access the VBlank interval directly.

but how do we do that? by using glFinish.

this is a much, **much** better solution over glXWaitVideoSyncSGI, as it achieves the same effect, doesn't have a chance of freezing under Mesa, and may work under EGL on X and Wayland.

now, by doing this, we have a proper desktop without stuttering, but the lag persists...

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

by removing this code and simply presenting as soon as possible (we're blocking for retrace anyway due to the glFinish thingy), we cut off 1 whole frame of lag!

but hey, can we go further? yes, of course!

now, by sleeping for a very few milliseconds (up to 8 in high-end systems) the compositor has more time for user input before rendering, which further reduces latency.

the reason why only up to 8ms is because any further would leave little room for rendering, and that will actually produce more stuttering than fix it.

## KWin-lowlatency is not...

* perfect. it tries its best to deliver low-latency no-stutter video, but I can't promise this is always the case.
  as an example, it will stutter if you select another window, or if you have too many windows open.
* truly designed for low-end systems. if you use KWin-lowlatency in one of them, you may experience stuttering.

# installation

## Arch Linux

[kwin-lowlatency is available in the AUR](https://aur.archlinux.org/packages/kwin-lowlatency).

## Fedora

ZaWertun provides pre-compiled packages [in the Copr](https://copr.fedorainfracloud.org/coprs/zawertun/kde/package/kwin-lowlatency/).

## Gentoo

[an overlay](https://github.com/agates/kwin-lowlatency-overlay) is available, courtesy of agates.

## openSUSE Tumbleweed

kwin-lowlatency can be found at [home:KAMiKAZOW:KDE](https://software.opensuse.org/download.html?project=home%3AKAMiKAZOW%3AKDE&package=kwin-lowlatency) (OBS).

## other distributions/manual method

you can compile/install this yourself if your distro isn't listed here (yes, I know Ubuntu is missing) or if you want to.

### acquiring the source

you can get the source code by using any of the following 2 methods:

#### git repo clone

```
$ git clone https://github.com/tildearrow/kwin-lowlatency.git
$ cd kwin-lowlatency
```

you may want to check the current stable version out:

```
$ git checkout v5.15.5
```

#### patch format

download stock KWin source and patch file:

```
$ wget https://download.kde.org/stable/plasma/5.15.5/kwin-5.15.5.tar.xz
$ wget http://tildearrow.zapto.org/storage/kwin-lowlatency/kwin-lowlatency-5.15.5.patch
```

extract:

```
$ tar -xvf kwin-5.15.5.tar.xz
```

patch:

```
$ cd kwin-5.15.5
$ patch -p1 < ../kwin-lowlatency-5.15.5.patch
```

### building

```
$ mkdir build
$ cd build
$ cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib -DCMAKE_INSTALL_LIBEXECDIR=lib -DBUILD_TESTING=OFF
$ make
```

### installing

```
$ sudo make install
```

# contacting original KWin development team

 * mailing list: [kwin@kde.org](https://mail.kde.org/mailman/listinfo/kwin)
 * IRC: #kwin on freenode

# additional options menu

KWin-lowlatency introduces few extra options in System Settings > Display and Monitor > Compositor. these are:

- animation curve: allows you to make animations look smoother. I have a gripe with linear animations, hence this option. i'll post a video about this later.
- latency/stutter control: use if you have a high-end system and want lower latency, or if you're having stuttering and want to reduce it.
- maximum/minimum latency reduction: allows you to configure the latency reduction window. examples (min/max): 8/0 default, 0/0 disable latency reduction, and 8/8 lowest latency possible. this is limited to 8ms, since any further would cause major stuttering and slowdowns.

# misc/FAQ

> Ubuntu Ubuntu Ubuntu

i'm working on it...

> what's this "Paint cursor" effect in Desktop Effects?

it's an effect I wrote back in 2018 when experimenting with kmsgrab for some private recordings.
it basically redraws the cursor. this may seem redundant, but actually is helpful for recording with kmsgrab (since it doesn't draw the hardware sprite).

> will this work under Wayland?

no, it won't, but I am working on it. so far using DRM VBlank only showed negative results, with applications running at half speed. now i'm trying again with glFinish and friends...

> i'm using the `modesetting` driver (instead of the `amdgpu` DDX driver) under an AMD card, and can see some latency. how do I fix this?

an option will come soon.

> do you have any plans to upstream this?

at this moment, not really:

- i still can't ensure this will work everywhere (but it should).
- this patchset brings full-screen unredirection back, which is a feature the KDE devs definitely [don't want to see in upstream](https://blog.martin-graesslin.com/blog/2016/08/opengl-changes-in-kwin-compositing/) since they have another "approach" (allow apps to block compositing).
  - the problem with their approach is that it means every app must support it in order to work, which is something not every app does. on the other hand, unredirection works for most apps, and doesn't require developers to change their code for it to work.
  - another problem is that instead of suspending compositing, it **disables** compositing. this means you get to see a few frames of an ugly desktop when the app quits. this doesn't happen with unredirection.
- furthermore, this also brings back "close" option in Present Windows, which once again the KDE devs despise.

> unredirection is not working.

please see [unredirect.md](https://github.com/tildearrow/kwin-lowlatency/blob/Plasma/5.15/unredirect.md) for known issues. you may file a bug report as well.

# support

use the issues section at the top.

# bugs?

use the issues section at the top. **but wait!** before reporting an issue, first test it again on [the official KWin](https://cgit.kde.org/kwin.git/) to determine whether this is really a KWin-lowlatency-only bug.

if the bug also reproduces in official KWin, please file a bug on [their bug tracker](https://bugs.kde.org/enter_bug.cgi?product=kwin) instead.

# Developing on KWin

Please refer to [hacking documentation](HACKING.md) for how to build and start KWin. Further information about KWin's test suite can be found in [TESTING.md](TESTING.md).

no, sorry, there are no tests for the low-latency features yet.
