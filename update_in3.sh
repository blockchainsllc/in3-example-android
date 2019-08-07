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

# patch jni
echo $"add_library(libin3 SHARED in3.c in3_IN3.c)\nSET_TARGET_PROPERTIES(libin3 PROPERTIES OUTPUT_NAME in3)\ntarget_link_libraries(libin3  eth_full)" > in3-core/src/bindings/java/CMakeLists.txt 

# copy client to java path
cp -r in3-core/src/bindings/java/in3 src/main/java/

