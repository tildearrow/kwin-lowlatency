#!/bin/bash

klpath=${PWD##*/}
minorVer=2
version=$(sed -rn "s/^set\(PROJECT_VERSION \"([0-9.]+)\"\)$/\1/p" CMakeLists.txt)

cd ..
if [ ! -e kwin-$version.tar.xz ]
  then wget https://download.kde.org/stable/plasma/$version/kwin-$version.tar.xz || exit 1
fi

tar -xf kwin-$version.tar.xz

if [ $minorVer -eq 1 ]
  then filename="kwin-lowlatency-$version.patch"
  else filename="kwin-lowlatency-$version-$minorVer.patch"
fi
diff -ruNx .git -x build -x .clang-format -x .kdev4 -x po kwin-$version $klpath > $filename
