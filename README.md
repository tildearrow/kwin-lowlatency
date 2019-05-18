# KWin-lowlatency

KWin-lowlatency is my attempt to reduce latency and stuttering in the popular KWin compositor used by KDE Plasma.

## Background

Stock KWin has a major issue regarding stuttering. It stutters heavily, and if you do not want that, then you have the input latency problem (of up to 50ms!).

The rationale for such a problem is that KWin uses a weird method to paint the screen.
Instead of simply sync'ing to vblank, it uses a timer. yeah, a **timer** that is off-sync with the vblank interval.
You can prove this by disabling VSync in the system settings. You will see just 1 line of tearing in your screen, and well, that's the time it swaps due to the timer.

In order to "fix" this under vanilla KWin, a typical solution is to insert this in kwinrc:

```
MaxFPS=200
RefreshRate=200
```

effectively making the timer faster than the actual screen's rate, forcing VSync at some point.

However, this introduces additional input lag, which varies depending on your driver, but usually it's 50ms (still too high).
You can prove this by moving a window. You will see the cursor being ahead of the title bar, or at least so under X.

So, how to fix this? Let's ditch the timer and let us access the VBlank interval directly.

But how do we do that? By using glFinish.

This is a much, **much** better solution over glXWaitVideoSyncSGI, as it achieves the same effect, does not have a chance of freezing under Mesa, and may work under EGL on X and Wayland.

Now, by doing this, we have a proper desktop without stuttering, but the input lag persists…

After digging deep into the code, I found this piece of code in particular, which is pretty much the culprit:

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

By removing this code and simply presenting as soon as possible (we are blocking for retrace anyway due to the glFinish thingy), we cut off 1 whole frame of lag!

But hey, can we go further? Yes, of course!

Now, by sleeping for a very few milliseconds (up to 8 in high-end systems) the compositor has more time for user input before rendering, which further reduces input lag.

The reason why only up to 8ms is because any further would leave little room for rendering, and that will actually produce more stuttering than fix it.

## KWin-lowlatency is not…

* …perfect. It tries its best to deliver low-latency no-stutter video, but I cannot promise this is always the case.
  As an example, it will stutter if you select another window, or if you have too many windows open.
* …truly designed for low-end systems. If you use KWin-lowlatency in one of them, you may experience stuttering.

# Installation

## Arch Linux

[kwin-lowlatency is available in the AUR](https://aur.archlinux.org/packages/kwin-lowlatency).

## Fedora

ZaWertun provides pre-compiled packages [in the Copr](https://copr.fedorainfracloud.org/coprs/zawertun/kde/package/kwin-lowlatency/).

## Gentoo

[An overlay](https://github.com/agates/kwin-lowlatency-overlay) is available, courtesy of agates.

## openSUSE Tumbleweed
kwin-lowlatency has been made available in [home:KAMiKAZOW:KDE](https://software.opensuse.org/download.html?project=home%3AKAMiKAZOW%3AKDE&package=kwin-lowlatency) of openSUSE Build Service.

## Other distributions/manual method

You can compile/install this yourself if your distro is not listed here (yes, I know Ubuntu is missing) or if you want to.

### Acquiring the source

You can get the source code by using any of the following 2 methods:

#### git repo clone

```
$ git clone https://github.com/tildearrow/kwin-lowlatency.git
$ cd kwin-lowlatency
```

You may want to check the current stable version out:

```
$ git checkout v5.15.5
```

#### Patch format

Download stock KWin source and patch file:

```
$ wget https://download.kde.org/stable/plasma/5.15.5/kwin-5.15.5.tar.xz
$ wget http://tildearrow.zapto.org/storage/kwin-lowlatency/kwin-lowlatency-5.15.5.patch
```

Extract:

```
$ tar -xvf kwin-5.15.5.tar.xz
```

Patch:

```
$ cd kwin-5.15.5
$ patch -p1 < ../kwin-lowlatency-5.15.5.patch
```

### Building

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

# Contacting original KWin development team

 * Mailing list: [kwin@kde.org](https://mail.kde.org/mailman/listinfo/kwin)
 * IRC: #kwin on freenode

# Additional options menu

KWin-lowlatency introduces few extra options in System Settings > Display and Monitor > Compositor. These are:

- Animation curve: Allows you to make animations look smoother. I have a gripe with linear animations, hence this option. I Will post a video about this later.
- Latency/stutter control: Use if you have a high-end system and want lower latency, or if you're having stuttering and want to reduce it.
- Maximum/minimum latency reduction: Allows you to configure the latency reduction window. Examples (min/max): 8/0 default, 0/0 disable latency reduction, and 8/8 lowest latency possible. This is limited to 8ms, since any further would cause major stuttering and slowdowns.

# Misc/FAQ

> What is this "Paint cursor" effect in Desktop Effects?

It is an effect I wrote back in 2018 when experimenting with kmsgrab for some private recordings.
It basically redraws the cursor. this may seem redundant, but actually is helpful for recording with kmsgrab (since it does not draw the hardware sprite).

> Will this work under Wayland?

No, it will not. It has not been done yet, since there is no way to ensure every retrace will wait for VBlank (especially on Mesa)… and although I could use DRM VBlank waiting there, it will not work on NVIDIA. Sorry.

> I am using the `modesetting` driver (instead of the `amdgpu` DDX driver) under an AMD card, and can see some latency. How do I fix this?

An option will come soon.

> Do you have any plans to upstream this?

At this moment, not really:

- I still cannot ensure this will work everywhere (but it should).
- eventually I am merging the unredirect branch, which is a feature the KDE devs definitely [do not want to see in upstream](https://blog.martin-graesslin.com/blog/2016/08/opengl-changes-in-kwin-compositing/) since they have another "approach" (allow apps to block compositing).
  - The problem with their approach is that it means every app must support it in order to work, which is something not every app does. On the other hand, unredirection works for most apps, and does not require developers to change their code for it to work.
  - Another problem is that instead of suspending compositing, it **disables** compositing. This means you get to see a few frames of an ugly desktop when the app quits. This does not happen with unredirection.
- Furthermore, this also brings back "close" option in Present Windows, which once again the KDE devs despise.

> Unredirection is not working.

Please see [unredirect.md](https://github.com/tildearrow/kwin-lowlatency/blob/Plasma/5.15/unredirect.md) for known issues. You may file a bug report as well.

# Support

Use the issues section at the top.

# Bugs?

Use the issues section at the top. **but wait!** before reporting an issue, first test it again on [the official KWin](https://cgit.kde.org/kwin.git/) to determine whether this is really a KWin-lowlatency-only bug.

If the bug also reproduces in official KWin, please file a bug on [their bug tracker](https://bugs.kde.org/enter_bug.cgi?product=kwin) instead.

# Developing on KWin

Please refer to [hacking documentation](HACKING.md) for how to build and start KWin. Further information about KWin's test suite can be found in [TESTING.md](TESTING.md).

No, sorry, there are no tests for the low-latency features yet.
