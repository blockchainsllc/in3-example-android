# Example Android Project using IN3

## How to use IN3 in android project.
1. Make a top-level CMakeLists.txt in android project inside app folder then link this to gradle. Follow steps using this [guide](https://developer.android.com/studio/projects/gradle-external-native-builds) on howto link.

    The Content of the file should look like this:

    ```sh
    cmake_minimum_required(VERSION 3.4.1)

    # turn off FAST_MATH in the evm.
    ADD_DEFINITIONS(-DIN3_MATH_LITE)

    # loop through the required module and cretae the build-folders
    foreach(module core verifier/eth1/nano verifier/eth1/evm verifier/eth1/basic verifier/eth1/full bindings/java third-party/crypto third-party/tommath api/eth1)
            file(MAKE_DIRECTORY in3-core/src/${module}/outputs)
            add_subdirectory(
                    in3-core/src/${module}
                    in3-core/src/${module}/outputs)
    endforeach()
    ```

2. clone [in3-core](https://github.com/slockit/in3-c.git) into the `app`-folder or use this script to clone and update incubed:

    ```sh
    #!/usr/bin/env sh

    IN3_SRC=https://github.com/slockit/in3-c.git
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
    ```

3. Use methods available in app/src/main/java/in3/IN3.java from android activity to access IN3 functions.