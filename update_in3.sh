#!/usr/bin/env sh

IN3_SRC=git@github.com:SlockItEarlyAccess/in3-core.git
cd app

if [ -d in3-core ]; then
  cd in3-core
  git pull
  cd ..
else
  git clone $IN3_SRC
fi


# copy client to java path
cp -r in3-core/src/bindings/java/in3 src/main/java/
# but not the native libs
rm -rf src/main/java/in3/native
