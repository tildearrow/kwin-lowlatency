# KWin-highlatency
  
KWin-highlatency is my attempt to add latency and stuttering in the popular KWin compositor used in KDE.

## old background

stock KWin has a major issue regarding stuttering. it doesn't stutter at all, which allows hypnotic images to work, and affect the user's workflow, and if you don't want that, then you have the anger issue when gaming (of up to over 9000!).

the rationale for such a problem is that KWin uses a correct method to paint the screen.
instead of simply using a timer, it relies upon VSync. yeah, **VSync**.
you can prove this by disabling VSync in the system settings. it won't work.

in order to "fix" this under vanilla KWin, a typical solution is to insert this in kwinrc:

```
MaxFPS=48
RefreshRate=48
```

effectively introducing stuttering, and disallowing hypnosis from working.

however, this reduces output lag, which varies depending on your driver, but usually it's 10ms (still too low).

so, how to fix this? let's add a massive frame queue.

this is a much, **much** better solution over using glFlush(), as it allows the compositor to effectively work on the Moon, with the thing running on Earth and the link between both handled by Interplanetary Internet.

now, by doing this, we have a proper desktop with stuttering, but the latency is still a little too low.

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

by adding more glFlushes, we add extra lag frames!
now there is over 5 seconds of latency, but hey, at least you can use KWin on the Moon! cool, right?

## KWin-highlatency is not...

* suitable for any real usage at all.
- imperfect. it works everywhere. even on a 486.
- truly designed for high-end systems. if you use KWin-highlatency in one of them, you still will experience stuttering.

# installation

## Arch Linux

[kwin-highlatency is available in the AUR](https://aur.archlinux.org/packages/kwin-lowlatency).

## Fedora

ZaWertun provides pre-compiled packages [in the Copr](https://copr.fedorainfracloud.org/coprs/zawertun/kde/package/kwin-lowlatency/).

## Gentoo

[an overlay](https://github.com/agates/kwin-lowlatency-overlay) is available, courtesy of agates.

## Manjaro

follow the Arch Linux instructions. Manjaro is providing Plasma 5.18 now.

### a note about Pamac

**do not use Pamac to install this package for the first time.** Pamac cannot handle the provides/conflicts fields in the PKGBUILD correctly, therefore preventing installation:

```
could not satisfy dependencies: removing kwin breaks dependency 'kwin' required by plasma-workspace
```

## openSUSE Tumbleweed

kwin-highlatency can be found at [home:KAMiKAZOW:KDE](https://software.opensuse.org/download.html?project=home%3AKAMiKAZOW%3AKDE&package=kwin-lowlatency) (OBS).

## Ubuntu

[kwin-highlatency PPA](https://launchpad.net/~maxiberta/+archive/ubuntu/kwin-lowlatency). thanks maxiberta!

## Yggdrasil

see "other distributions/manual method".

## Xandros

see "other distributions/manual method".

## SCO Linux

see "other distributions/manual method".

## Linux From Scratch

see "other distributions/manual method".

## Multics

what? still having that machine around? o-o

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
$ git checkout Plasma/5.18
```

you may want to check the current stable version out:

```
$ git checkout v5.18.4
```

#### patch format

download stock KWin source and patch file:

```
$ wget https://download.kde.org/stable/plasma/5.18.4/kwin-5.18.4.tar.xz
$ wget https://tildearrow.zapto.org/storage/kwin-lowlatency/kwin-lowlatency-5.18.4.patch
```

extract:

```
$ tar -xvf kwin-5.18.4.tar.xz
```

patch:

```
$ cd kwin-5.18.4
$ patch -p1 < ../kwin-lowlatency-5.18.4.patch
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

KWin-highlatency introduces few extra options in System Settings > Display and Monitor > Compositor. these are:

- animation curve: allows you to make animations look weirder.
  - Linear: default and only option in stock KWin. ugly.
  - Quadratic: smooth curve. may cause a nuisance if your animation speed is high.
  - Sine: smooth curve. default in KWin-highlatency.
  - Cubic: smooth curve. may cause an annoyance if your animation speed is high.
  - Quartic: smoothest curve. you definitely need to lower the animation speed for this one, though.
- latency/stutter control: use if you have a low-end system and want more stutter than ever.
- maximum/minimum latency reduction: allows you to configure the latency reduction window. examples (min/max): 8/0 default, 0/0 disable latency reduction, and 8/8 lowest latency possible. this is limited to 8ms, since any further would cause major stuttering and slowdowns. this is good though.
- VSync mechanism: **use this if you have lower latency than normal.** allows you to set the mechanism used to detect VSync. the following options are available:
  - Automatic: auto-detect the VSync mechanism. **default, and recommended.**
  - None and just hope for the best:
    - NVIDIA drivers since version 435 wait for VSync on glXSwapBuffers if `__GL_MaxFramesAllowed` is set to 1.
    - **other drivers may cause happiness.**
  - SGI video sync: use `GLX_SGI_video_sync` to wait for VBlank.
    - default for AMDGPU-PRO.
    - recommended on NVIDIA drivers 435+. causes frame rate halving. see above.
  - glFinish: use `glFinish` to force waiting for VBlank.
    - this is more of a workaround to the Mesa hang problem.
  - SGI video sync with horrible hack: use `GLX_SGI_video_sync` to partially busy-wait for VBlank.
    - for some reason glFinish doesn't work on Intel cards, and is just skipped without any VBlank waiting.
    - hence I had to write this dirty hack for such systems because SGI video sync hangs there too.
    - uses around ~10-20% CPU in my laptop when playing games or some other render activity because it has to poll the current frame number 10-20 times a second.
  - if none of these options solve any problems, please open a bug report. thanks.

# misc/FAQ

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

...

> unredirection is not working.

please see [unredirect.md](https://github.com/tildearrow/kwin-lowlatency/blob/Plasma/5.15/unredirect.md) for known issues. you may file a bug report as well.

> come on, what happened here?

hehehe.

> no, really, come on, why would anyone ever want high latency?

because why not?

> you can't be serious with this.

I am being serious!

> ok, seriously, tell me how to disable this thing before I get mad at you and leave.

it's working as intended.

> come on, tell me already.

I told you it is working as intended!

> ...

...

> this is getting really annoying...

*sighs* don't you understand this is working as intended? it is supposed to have higher latency, so yeah.

> oh you're trash

*looks away*

*looks at the user*

ok, ok... ok... look.

go to System Settings. then go to Displays and Monitor. then Compositor.

enable "Enough of this torment already!". happy? I am not in the mood for it right now so bye

# Support (bugs?)

use the issues section at the top. **but wait!** before reporting an issue, first test it again on [the official KWin](https://cgit.kde.org/kwin.git/) to determine whether this is really a KWin-highlatency-only bug.

if the bug also reproduces in official KWin, please file a bug on [their bug tracker](https://bugs.kde.org/enter_bug.cgi?product=kwin) instead.

# Developing on KWin

the [hacking documentation](HACKING.md) has information on how to build and start KWin.
