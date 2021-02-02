# Example Android Project using IN3

## How to use IN3 in android project.
1. clone [in3](https://github.com/blockchainsllc/in3.git) into the `app`-folder or use this script to clone and update incubed:

    ```sh
    #!/usr/bin/env sh

    IN3_SRC=https://github.com/blockchainsllc/in3.git
    cd app

    if [ -f in3/CMakeLists.txt ]; then
        cd in3
        git pull
        cd ..
    else
        git clone $IN3_SRC
    fi


    # copy client to java path
    cp -r in3-c/java/src/in3 src/main/java/
    ```

2. add the native-build section in your `build.gradle` in the app-folder:

```gradle
    externalNativeBuild {
        cmake {
            path file('in3/CMakeLists.txt')
        }
    }
```

if you want to confiure which modules should be included, you can also specify the `externalNativeBuild` in the `defaultConfig':` 
```gradle
    defaultConfig {
        externalNativeBuild {
            cmake {
                arguments "-DBTC=OFF", "-DZKSYNC=OFF"
            }
        }
    }

```

For possible options, see https://in3.readthedocs.io/en/develop/api-c.html#cmake-options

3. Use methods available in app/src/main/java/in3/IN3.java from android activity to access IN3 functions.
