# Example Android Project using IN3

## How to use IN3 in android project.
1. Make a top-level CMakeLists.txt in android project inside app folder then link this to gradle. Follow steps using this [guide](https://developer.android.com/studio/projects/gradle-external-native-builds) on howto link.

    The Content of the file should look like this:

    ```sh
    cmake_minimum_required(VERSION 3.4.1)

    # turn off FAST_MATH in the evm.
    ADD_DEFINITIONS(
    -DIN3_MATH_LITE
    -DUSE_PRECOMPUTED_CP=1
    -DERR_MSG
    -DETH_FULL
    -DETH_BASIC
    -DETH_NANO
    -DIN3_EXPORT_TEST=static
    )
    # loop through the required module and cretae the build-folders
    foreach(module core verifier/eth1/nano verifier/eth1/evm verifier/eth1/basic verifier/eth1/full ../../java/src third-party/crypto third-party/tommath api)
            file(MAKE_DIRECTORY in3-c/c/src/${module}/outputs)
            add_subdirectory(
                    in3-c/c/src/${module}
                    in3-c/c/src/${module}/outputs)
    endforeach()
    ```

2. clone [in3-c](https://github.com/slockit/in3-c.git) into the `app`-folder or use this script to clone and update incubed:

    ```sh
    #!/usr/bin/env sh

    IN3_SRC=https://github.com/slockit/in3-c.git
    cd app

    if [ -d in3-c ]; then
        cd in3-c
        git pull
        cd ..
    else
        git clone $IN3_SRC
    fi


    # copy client to java path
    cp -r in3-c/java/src/in3 src/main/java/
    mkdir -p c
    cp in3-c/c/compiler.cmake c/
    ```

3. Use methods available in app/src/main/java/in3/IN3.java from android activity to access IN3 functions.
