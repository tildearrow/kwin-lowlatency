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

[kwin-lowlatency is available in the AUR. 5.23 only.](https://aur.archlinux.org/packages/kwin-lowlatency)

## Manjaro

Manjaro has updated to Plasma 5.23. use the AUR package.

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
$ git checkout Plasma/5.22
```

you may want to check the current stable version out:

```
$ git checkout v5.22.5
```

#### patch format

download stock KWin source and patch file:

```
$ wget https://download.kde.org/stable/plasma/5.22.5/kwin-5.22.5.tar.xz
$ wget https://tildearrow.org/storage/kwin-lowlatency/kwin-lowlatency-5.22.5.patch
```

extract:

```
$ tar -xvf kwin-5.22.5.tar.xz
```

patch:

```
$ cd kwin-5.22.5
$ patch -p1 < ../kwin-lowlatency-5.22.5.patch
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

KWin-lowlatency introduces a few options in System Settings > Display and Monitor > Compositor, which are used to control whether unredirection/direct scanout is on.

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
