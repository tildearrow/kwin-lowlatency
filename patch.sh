#!/bin/bash

klpath=${PWD##*/}
actualVer=5.15.5
version=$(sed -rn "s/^set\(PROJECT_VERSION \"([0-9.]+)\"\)$/\1/p" CMakeLists.txt)

cd ..
if [ ! -e kwin-$actualVer.tar.xz ]
  then wget https://download.kde.org/stable/plasma/$actualVer/kwin-$actualVer.tar.xz || exit 1
fi

tar -xf kwin-$actualVer.tar.xz
diff -ruNx .git -x build -x .kdev4 -x po -x CMakeLists.txt kwin-$actualVer $klpath > kwin-lowlatency-$version.patch
