#!/bin/bash

klpath=${PWD##*/}
minorVer=1
tinyPatch=0
version=$(sed -rn "s/^set\(PROJECT_VERSION \"([0-9.]+)\"\)$/\1/p" CMakeLists.txt)

if [ $tinyPatch -gt 0 ]; then
  downversion=$version"."$tinyPatch
else
  downversion=$version
fi

cd ..
if [ ! -e kwin-$downversion.tar.xz ]
  then wget https://download.kde.org/unstable/plasma/$version/kwin-$downversion.tar.xz || exit 1
fi

tar -xf kwin-$downversion.tar.xz

if [ $minorVer -eq 1 ]
  then filename="kwin-lowlatency-$version.patch"
  else filename="kwin-lowlatency-$version-$minorVer.patch"
fi
diff -ruNx .git -x build -x .clang-format -x .kdev4 -x po kwin-$downversion $klpath > $filename
