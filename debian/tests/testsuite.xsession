#!/bin/sh

#The XDG runtime dir path must not exceed 108 bytes; otherwise,
#apparently kwin wouldn't work

TEMPDIR=`mktemp -d`
trap cleanup EXIT
export XDG_RUNTIME_DIR=${TEMPDIR}

kwin_wayland --framebuffer & WMPID=$!
sleep 5

dh_auto_test

cleanup () {
    if [ -n "$WMPID" ]; then kill "$WMPID"; fi
    rm -rf ${TEMPDIR}
}
