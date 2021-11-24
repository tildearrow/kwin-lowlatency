# KWin-lowlatency

KWin-lowlatency is (was?) my attempt to reduce latency and stuttering in the popular KWin compositor used in KDE.
since Plasma 5.21 the developers [merged official patches](https://invent.kde.org/plasma/kwin/-/merge_requests/507) which rewrite great parts of the compositing code, putting it on par with former KWin-lowlatency.

however, there's one thing that's not there yet: X11 full-screen unredirection....

## Unredirection

the main aim of this project is to add additional settings to the compositor and bring back unredirection based on the direct scanout code. see [unredirect.md](unredirect.md) for more details.

## KWin-lowlatency is not...

- perfect. it tries its best to allow full-screen unredirection to happen, but I can't promise this is always the case.
- truly designed for low-end systems. if you use KWin-lowlatency in one of them, you may experience stuttering.

# installation

## Arch Linux

[kwin-lowlatency is available in the AUR.](https://aur.archlinux.org/packages/kwin-lowlatency)

also available as a package in the [CachyOS repository](https://wiki.cachyos.org/en/home/Repo). thanks ptr1337!

## Manjaro

Manjaro has updated Plasma to 5.23! follow the Arch Linux instructions.

## openSUSE Leap and Tumbleweed

available at [home:DarkWav:kwin-lowlatency](https://software.opensuse.org/download.html?project=home%3ADarkWav%3Akwin-lowlatency&package=kwin-lowlatency). Leap is on 5.18 though.

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
$ git checkout Plasma/5.23
```

you may want to check the current stable version out:

```
$ git checkout v5.23.3-3
```

#### patch format

download stock KWin source and patch file:

```
$ wget https://download.kde.org/stable/plasma/5.23.3/kwin-5.23.3.tar.xz
$ wget https://tildearrow.org/storage/kwin-lowlatency/kwin-lowlatency-5.23.3-3.patch
```

extract:

```
$ tar -xvf kwin-5.23.3.tar.xz
```

patch:

```
$ cd kwin-5.23.3
$ patch -p1 < ../kwin-lowlatency-5.23.3-3.patch
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
 * IRC: #kde-kwin on irc.libera.chat

# additional options menu

KWin-lowlatency introduces a few options in System Settings > Display and Monitor > Compositor. these options are documented in this section.

the options are:
- **Enable full-screen unredirection (X11)**: allows a full-screen application to bypass the compositor, which reduces latency.
- **Enable full-screen direct scanout (Wayland)**: same as above, but for Wayland.
- **Debug full-screen unredirection/direct scanout**: writes unredirection/direct scanout check status to standard output on every frame, which is useful when it is not working and/or causing problems (KWin-lowlatency used to have problems with transparent dummy windows that stay on top hindering unredirection).
- **Force unredirection for non-opaque windows**: ignores whether a full-screen window is transparent and attempts to unredirect anyway. may cause minor glitches.
- **My OpenGL is always safe!**: tells the compositor to never set `OpenGLIsUnsafe` when it detects initialization problems. on vanilla KWin there is a bug which triggers a false-positive when KWin takes too long to start (e.g. on an HDD).
- **Set MaxFramesAllowed to 1 (NVIDIA only)**: sets `__GL_MaxFramesAllowed` to 1 which ensures the compositor will wait for vertical blank (use in conjunction with VSync mechanism described below). has no effect on AMD and Intel graphics.
- **Debug composite timer**: writes the value of the composite timer to standard output on every frame. useful for debugging latency problems.
- **Render time estimator**: this is a vanilla setting which isn't there for some reason. KWin-lowlatency adds it. may help with latency/stutter (use with caution).
- **VSync mechanism**: forces a different vertical blank detection system from the default one. the options are:
  - Automatic: default KWin behavior.
    - uses Intel swap event if available (AMD and Intel (but I've heard it's disabled there due to bugs))
    - uses `GLX_SGI_video_sync` extension otherwise (NVIDIA) or `GLX_OML_sync_control` if the former isn't available.
    - worst case it falls back to using a timer (which may cause stuttering).
  - None: don't use anything at all. will increase latency in nearly every case.
  - Intel swap event: use the `GLX_INTEL_swap_event` extension (implemented by Mesa) for detecting the vertical blank. ironically this is not the default for Intel graphics due to some reason.
  - glFinish: use the glFinish() operation at the end of a frame to block until the next vertical blank interval. doesn't always work.
  - SGI video sync: use the `GLX_SGI_video_sync` extension to determine the next vertical blank.
  - OML sync control: use the `GLX_OML_sync_control` extension to determine the next vertical blank. not available on NVIDIA, or at least not yet.
  - SGI video sync busy-wait: use the `GLX_SGI_video_sync` extension, but poll every millisecond until the next vertical blank interval occurs. this option is there because there was a bug in Mesa which caused hangs when using the wait functions.
- **Action on crash**: allows you to choose what happens when KWin-lowlatency crashes.
- **Position of VSync pixel (SGI/OML only)**: COMING SOON! allows you to change the position of the dummy window created for detecting the next vertical blank when using SGI video sync or OML sync control. this may come in handy if you use more than one monitor.
- **Force disable VSync**: turns VSync off completely. no need to pass an environment variable to do that anymore with KWin-lowlatency! **may require restarting compositor manually.**
  - use in conjunction with the settings above for _some_ kind of multi-monitor support. not perfect though.

# misc/FAQ

> what's this "Paint cursor" effect in Desktop Effects?

it's an effect I wrote back in 2018 when experimenting with kmsgrab for some private recordings.
it basically redraws the cursor. this may seem redundant, but actually is helpful for recording with kmsgrab (since it doesn't draw the hardware sprite).

the following applications may benefit from the usage of this effect:

- FFmpeg (kmsgrab input device)
- darmstadt
- w23's drmtoy/linux-libdrm-grab OBS plugin

> will this work under Wayland?

it probably will, but there isn't too much difference

> do you have any plans to upstream this?

the unredirection part? maybe. I'll think about it.

> unredirection is not working.

please see [unredirect.md](unredirect.md) for known issues. you may file a bug report as well.

# Support (bugs?)

use the issues section at the top. **but wait!** before reporting an issue, first test it again on [the official KWin](https://invent.kde.org/plasma/kwin) to determine whether this is really a KWin-lowlatency-only bug.

if the bug also reproduces in official KWin, please file a bug on [their bug tracker](https://bugs.kde.org/enter_bug.cgi?product=kwin) instead.

# Developing on KWin

the [hacking documentation](HACKING.md) has information on how to build and start KWin.
