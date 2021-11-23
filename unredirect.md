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

# using the debugger

you may enable the **Debug full-screen unredirection/direct scanout** setting in System Settings > Display and Monitor > Compositor in order to enable the unredirection debugger. after that, start KWin-lowlatency on a terminal (e.g  `kwin_x11 --replace`) and then observe the output.

the possible messages are:

- **Testing window of size...**: the compositor is going through the next window in a top-to-bottom order to determine which one to unredirect.
- **Too small. Next one.**: the window is smaller than 3x3. this exists to prevent dummy windows from messing up the unredirection process.
- **Client is not full-screen.**: self-explanatory. the window isn't full-screen.
- **Window has no surface item**: the window exists, but it contains nothing.
- **Surface item has no pixmap.**: the window exists, but its surface item contains nothing.
- **Position of surface item is not (0, 0).**: the window exists and is full-screen, but its contents do not actually cover the entire screen.
- **Window is not opaque.**: the window exists, is full-screen and its contents cover the entire screen, but it is transparent or has the potential to be.
- **Window not visible, toplevel not on output and/or not opaque.**: the window may be minimized and/or is transparent.
- **Scanning (nil)**: nothing to unredirect.
- **Scanning 0x?????????????**: there is a window being unredirected. the random number is a pointer to the surface item of aforementioned window.
- **Effect blocks DS.**: an effect is preventing full-screen unredirection from working.
- **Unredirection started**: unredirection has started for a window.
- **Unredirection window switch**: unredirection stops in one window and quickly starts on another.
- **Unredirection stopped**: unredirection has stopped.
- **Direct scanout starting on output**: direct scanout has started for a window/surface. Wayland only.
- **Direct scanout stopped on output**: direct scanout has stopped. Wayland only.

the control flow for the unredirection detection process is:

```
1. sort all windows from top to bottom.
2. are we done checking all windows? if so then quit.
3. query the next window.
4. is it smaller than 3x3?
  - if so, go to 2.
5. are all of the following conditions are met?
  - is an X11 session or the window is in the current output,
  - window is visible (e.g. not minimized), and
  - window is not transparent OR user has forced unredirecting non-opaque windows AND window is not 32x32 in size (for some reason there is a 32x32 dummy window on top all the time)
  if not then go to 2.
6. does the window exist and is it a full-screen window?
  - if not then quit.
7. does the window have a surface item?
  - if not then quit.
8. retrieve the contents from the surface item.
9. does it exist?
  - if not then quit.
10. update the contents.
11. do the contents cover the entire window?
  - if not then quit.
12. has the user forced unredirecting non-opaque windows OR the window is not transparent AND its contents cover the entire window?
  - if not then quit.
13. we have something to unredirect!
14. has the user turned unredirection or direct scanout on?
  - if not then quit.
15. is an effect active and preventing unredirection?
  - if so then quit.
16. unredirect the window.
```
