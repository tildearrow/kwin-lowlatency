#!/bin/bash
# make po

klpath=${PWD##*/}
version=$(sed -rn "s/^set\(PROJECT_VERSION \"([0-9.]+)\"\)$/\1/p" CMakeLists.txt)

rm -r po

cd ..
if [ ! -e kwin-$version.tar.xz ]
  then wget https://download.kde.org/stable/plasma/$version/kwin-$version.tar.xz || exit 1
fi

tar -xf kwin-$version.tar.xz

cp -r kwin-$version/po $klpath/po
