# fullscreen unredirection

this version of the compositor brings back the ability to unredirect fullscreen windows, which has been originally removed by the KDE developers because they thought their "block compositing" approach would be the future.

their "block compositing" approach has 3 flaws:

- it actually fully disables the compositor (which means it will take a while for it to re-enable)
- it can get annoying (especially with SDL apps. SDL requests KWin to disable compositing, even if not in fullscreen!)
- every application must be coded to support it. with unredirection it works on most apps without giving the developers extra burden.

however, fullscreen unredirection also has its flaws:

- in some setups it may crash, according to KDE developers
- some applications may interfere and prevent it from working

# non-working/crashing setups

none so far. please contact me if your setup causes KWin to crash with unredirection.

# known issues

the following applications prevent unredirection from working:

- calfjackhost
- [latte-dock](https://github.com/tildearrow/kwin-lowlatency/issues/1#issuecomment-483403493) (sometimes)
- [blizzard's Battle.net client](https://github.com/tildearrow/kwin-lowlatency/issues/1#issuecomment-491345833)

the following applications may misbehave when unredirection is enabled:

- some Wine applications. after switching windows they may turn black.
