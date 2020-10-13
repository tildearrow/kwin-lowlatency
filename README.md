# again, the Manjaro note.

please do not upgrade to KWin-lowlatency 5.20 yet, or at least not until they upgrade Plasma first.

# KWin-lowlatency

KWin-lowlatency is my attempt to reduce latency and stuttering in the popular KWin compositor used in KDE.

## old background

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

but how do we do that? there are several approaches, but they vary between drivers.

1. by using glFinish.
  - this is a much, **much** better solution over glXWaitVideoSyncSGI for Mesa, as it achieves the same effect and doesn't have a chance of freezing.
2. by using glXWaitVideoSyncSGI.
  - this solution works on Catalyst/AMDGPU-PRO. it was used before for Mesa too, but not anymore after Mesa exhibited freezes.
3. by doing nothing at all.
  - NVIDIA doesn't require anything in particular for this to work. the buffer swap function seems to wait for VBlank.
4. by polling the current VBlank interval and waiting until the next one.
  - sadly, Mesa (Intel) does not obey glFinish correctly. this is a hack, but it works.

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

by removing this code and simply presenting as soon as possible (we're blocking for retrace anyway due to the wait-for-vblank thingy), we cut off 1 whole frame of lag!

but hey, can we go further? yes, of course!

now, by sleeping for a very few milliseconds (up to 8 in high-end systems) the compositor has more time for user input before rendering, which further reduces latency.

the reason why only up to 8ms is because any further would leave little room for rendering, and that will actually produce more stuttering than fix it.

## KWin-lowlatency is not...

- perfect. it tries its best to deliver low-latency no-stutter video, but I can't promise this is always the case.
  - as an example, it will stutter if you select another window, or if you have too many windows open.
- truly designed for low-end systems. if you use KWin-lowlatency in one of them, you may experience stuttering.

# installation

## Arch Linux

[kwin-lowlatency is available in the AUR](https://aur.archlinux.org/packages/kwin-lowlatency).

## Fedora

ZaWertun provides pre-compiled packages [in the Copr](https://copr.fedorainfracloud.org/coprs/zawertun/kde/package/kwin-lowlatency/).

## Gentoo

[an overlay](https://github.com/agates/kwin-lowlatency-overlay) is available, courtesy of agates.

## Manjaro

**please do not upgrade to KWin-lowlatency 5.20 yet, or at least not until they upgrade Plasma first.**

in the meanwhile you can use the 5.19.5 version. I will open the Manjaro-specific repo soon.

## NixOS

check out pasqui23's [kwin-ll](https://github.com/pasqui23/nixpkgs/tree/kwin-ll) branch.

please note that this is an old version.

## openSUSE Leap and Tumbleweed

kwin-lowlatency can be found at [home:DarkWav:kwin-lowlatency](https://software.opensuse.org/download.html?project=home%3ADarkWav%3Akwin-lowlatency&package=kwin-lowlatency) (OBS).

## Ubuntu

[kwin-lowlatency PPA](https://launchpad.net/~maxiberta/+archive/ubuntu/kwin-lowlatency). thanks maxiberta!

## other distributions/manual method

you can compile/install this yourself if your distro isn't listed here, or if you merely want to.

### installing dependencies

you may need to install specific dependencies before compiling. [here](https://community.kde.org/Guidelines_and_HOWTOs/Build_from_source/Install_the_dependencies) you can find a list.

### acquiring the source

you can get the source code by using any of the following 2 methods:

#### git repo clone

```
$ git clone https://github.com/tildearrow/kwin-lowlatency.git
$ cd kwin-lowlatency
```

if you are not using the latest major version of Plasma (you can check by doing `kwin_x11 --version`), you may want to check out the branch for your version, e.g.:

```
$ git checkout Plasma/5.20
```

you may want to check the current stable version out:

```
$ git checkout v5.20.0
```

#### patch format

download stock KWin source and patch file:

```
$ wget https://download.kde.org/stable/plasma/5.20.0/kwin-5.20.0.tar.xz
$ wget https://tildearrow.zapto.org/storage/kwin-lowlatency/kwin-lowlatency-5-20.0.patch
```

extract:

```
$ tar -xvf kwin-5.20.0.tar.xz
```

patch:

```
$ cd kwin-5.20.0
$ patch -p1 < ../kwin-lowlatency-5.20.0.patch
```

### building

#### Ubuntu

Ubuntu uses a different library path, which means you'll have to tweak the library dir:

```
$ mkdir build
$ cd build
$ cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib/x86_64-linux-gnu -DCMAKE_INSTALL_LIBEXECDIR=lib/x86_64-linux-gnu -DBUILD_TESTING=OFF ..
$ make
```

#### other distros

```
$ mkdir build
$ cd build
$ cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib -DCMAKE_INSTALL_LIBEXECDIR=lib -DBUILD_TESTING=OFF ..
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

- animation curve: allows you to make animations look smoother. I have a gripe with linear animations, hence this option. the following options are available:
  - Linear: default and only option in stock KWin. ugly.
  - Quadratic: smooth curve. may cause a nuisance if your animation speed is high.
  - Sine: smooth curve. default in KWin-lowlatency.
  - Cubic: smooth curve. may cause an annoyance if your animation speed is high.
  - Quartic: smoothest curve. you definitely need to lower the animation speed for this one, though.
- latency/stutter control: use if you have a high-end system and want lower latency, or if you're having stuttering and want to reduce it.
- minimum/maximum latency reduction: allows you to configure the latency reduction window. examples (min/max): 0/8 default, 0/0 disable latency reduction, and 8/8 lowest latency possible. this is limited to 8ms, since any further would cause major stuttering and slowdowns.
- VSync mechanism: **use this if you have latency problems or frame rate halving.** allows you to set the mechanism used to detect VSync. the following options are available:
  - Automatic: auto-detect the VSync mechanism. **default, and recommended.**
  - None and just hope for the best:
    - NVIDIA drivers since version 435 wait for VSync on glXSwapBuffers if `__GL_MaxFramesAllowed` is set to 1.
    - **other drivers may cause latency problems.**
  - SGI video sync: use `GLX_SGI_video_sync` to wait for VBlank.
    - default for AMDGPU-PRO.
    - not recommended on recent Mesa drivers. I've had hangs using it.
    - not recommended on NVIDIA drivers 435+. causes frame rate halving. see above.
  - glFinish: use `glFinish` to force waiting for VBlank.
    - according to this [bug report](https://github.com/tildearrow/kwin-lowlatency/issues/17) this may actually increase latency on NVIDIA cards.
    - this is more of a workaround to the Mesa hang problem.
  - SGI video sync with horrible hack: use `GLX_SGI_video_sync` to partially busy-wait for VBlank.
    - for some reason glFinish doesn't work on Intel cards, and is just skipped without any VBlank waiting.
    - hence I had to write this dirty hack for such systems because SGI video sync hangs there too.
    - uses around ~10-20% CPU in my laptop when playing games or some other render activity because it has to poll the current frame number 10-20 times a second.
  - if none of these options solve any problems, please open a bug report. thanks.

# misc/FAQ

> I can't build it and it fails on the class ScreenLocker::KSldApp has no member named 'setWaylandFd'

update Plasma to 5.19 before building.
this might include going back to vanilla KWin for the purpose of building the package.

> what's this "Paint cursor" effect in Desktop Effects?

it's an effect I wrote back in 2018 when experimenting with kmsgrab for some private recordings.
it basically redraws the cursor. this may seem redundant, but actually is helpful for recording with kmsgrab (since it doesn't draw the hardware sprite).

the following applications may benefit from the usage of this effect:

- FFmpeg (kmsgrab input device)
- darmstadt
- w23's drmtoy/linux-libdrm-grab OBS plugin

> will this work under Wayland?

no, it won't, but I am working on it. so far using DRM VBlank only showed negative results, with applications running at half speed. now i'm trying again with glFinish and friends...

> do you have any plans to upstream this?

some. Daniel gets a chance to improve the GNOME side in mainline and reduce lag so I must fight to get our beloved KDE on par with GNOME! however:

- i still can't ensure this will work everywhere (but it should).
- this patchset brings full-screen unredirection back, which is a feature the KDE devs definitely [don't want to see in upstream](https://blog.martin-graesslin.com/blog/2016/08/opengl-changes-in-kwin-compositing/) since they have another "approach" (allow apps to block compositing).
  - the problem with their approach is that it means every app must support it in order to work, which is something not every app does. on the other hand, unredirection works for most apps, and doesn't require developers to change their code for it to work.
  - another problem is that instead of suspending compositing, it **disables** compositing. this means you get to see a few frames of an ugly desktop when the app quits. this doesn't happen with unredirection.
  - I can try to upstream just the glFinish/notimer/something bits, and have this project for the rest of features.
- furthermore, this also brings back "close" option in Present Windows, which once again the KDE devs despise.
  - apparently they officially brought the feature back in Plasma 5.17, but not for left click :(
- Roman Gilg took my idea away.

> unredirection is not working.

please see [unredirect.md](https://github.com/tildearrow/kwin-lowlatency/blob/Plasma/5.15/unredirect.md) for known issues. you may file a bug report as well.

# Support (bugs?)

use the issues section at the top. **but wait!** before reporting an issue, first test it again on [the official KWin](https://cgit.kde.org/kwin.git/) to determine whether this is really a KWin-lowlatency-only bug.

if the bug also reproduces in official KWin, please file a bug on [their bug tracker](https://bugs.kde.org/enter_bug.cgi?product=kwin) instead.

# Developing on KWin

the [hacking documentation](HACKING.md) has information on how to build and start KWin.
