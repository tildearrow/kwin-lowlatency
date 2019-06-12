#!/bin/bash

klpath=${PWD##*/}
minorVer=2
version=$(sed -rn "s/^set\(PROJECT_VERSION \"([0-9.]+)\"\)$/\1/p" CMakeLists.txt)

cd ..
if [ ! -e kwin-$version.tar.xz ]
  then wget https://download.kde.org/stable/plasma/$version/kwin-$version.tar.xz || exit 1
fi

tar -xf kwin-$version.tar.xz
diff -ruNx .git -x build -x po -x CMakeLists.txt kwin-$version $klpath > kwin-lowlatency-$version-$minorVer.patch
