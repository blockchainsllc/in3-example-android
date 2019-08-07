#!/usr/bin/env sh

IN3_SRC=git@github.com:SlockItEarlyAccess/in3-core.git
cd app
git clone $IN3_SRC

# patch jni
echo $"add_library(libin3 SHARED in3.c in3_IN3.c)\nSET_TARGET_PROPERTIES(libin3 PROPERTIES OUTPUT_NAME in3)\ntarget_link_libraries(libin3  eth_full)" > in3-core/src/bindings/java/CMakeLists.txt 

cp -r in3-core/src/bindings/java/in3 src/main/java/

