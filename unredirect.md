# fullscreen unredirection

this version of the compositor brings back the ability to unredirect fullscreen windows, which has been originally removed by the KDE developers because they thought their "block compositing" approach would be the future.

their "block compositing" approach has 3 flaws:

- it actually fully disables the compositor (which means it will take a while for it to re-enable)
- it can get annoying (especially with SDL apps. SDL requests KWin to disable compositing, even if not in fullscreen!)
- every application must be coded to support it. with unredirection it works on most apps without giving the developers extra burden.

however, fullscreen unredirection also has its flaws:

- in some setups it may crash, according to KDE developers
- some applications may interfere and prevent it from working

as of Plasma 5.22 KDE developers brought in a new direct scanout framework for Wayland which means that the new unredirection code uses that instead of being hacked around the compositor.

so far I haven't experienced any problems when using it, but if there are any, please let me know.
