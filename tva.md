# Tilde van Arrow

the ultimate project to optimize KWin and cut the rough edges.
the compositor should do nothing more than render windows in the exact time.
it is unknown why do some things take too long to render, such as moving between desktop, using present windows or even switching windows.
those are detrimental to smoothness and cause stutter.

this document is incomplete.

# possibilities

- too much processing when switching windows
- the need to recreate every single texture
- Qt is too high-level

# solutions

## window switch stutter

even after using the most basic theme, switching windows causes stutter.
maybe the toplevel is doing too much, or some effect...

## texture recreation

certain heavier themes cause more lag spikes. come on why can't we have a cache so we don't have to re-render everything?
decoration rendering is done in software so yeah...

## Qt is too high-level

...we could replace certain unneeded Qt parts by our own ones, and perhaps improve performance...

## kwin-lowerlatencyandlowpower

what about we go single-buffer, somehow make the card always wait for VBlank (or near-VBlank) and let that be it?

what about we use an hybrid setup?

- single-buffering when the compositor is not doing anything fancy
- double-buffering when it is
- clients that want VSync will have a big wait before present
- clients that don't, will be redrawn immediately (this may cause tearing), Quartz-style

## let's learn OpenGL

because this could allow for further optimizations on lower-end systems.

I just do not think KWin is optimized enough... for them...

## come on let's catch up already

if we do not do anything about this soon, the other guy working at Mutter will destroy KWin in performance.

sadly I think we already lost the race, because GNOME is beginning to run on Pi, and... KDE still does not :<

come on they will defeat us and team uid will get a chance to achieve World Domination!! like this

> Qt is there because Canonical used it to validate their own CLA. Basically “We are happy to sign Qt’s CLA so you should be happy to sign our CLA”. Fortunately that failed as well.

oh come on 144Hz you are SO annoying!

# frequently asked questions

> KDE is a downstream, Qt has CLA and C++ is an unwise choice. GNOME is the Standard-

144Hz. [look here](https://www.youtube.com/watch?v=VZlWCBhoCns&t=56s).
