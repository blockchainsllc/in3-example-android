# Example Android Project using IN3

## How to use IN3 in android project.
1. clone [in3](https://github.com/blockchainsllc/in3.git) or use this script to clone and update incubed:

    ```sh
    #!/usr/bin/env sh
    if [ -f in3/CMakeLists.txt ]; then
        cd in3
        git pull
        cd ..
    else
        git clone https://github.com/blockchainsllc/in3.git
    fi
    ```

2. add the native-build section and the additional source-set in your `build.gradle` in the app-folder inside the `android`-section:

```gradle
    externalNativeBuild {
        cmake {
            path file('in3/CMakeLists.txt')
        }
    }
    sourceSets {
       main.java.srcDirs += ['../in3/java/src']
    }
```

if you want to configure which modules should be included, you can also specify the `externalNativeBuild` in the `defaultConfig':` 
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

Now you can use any Functions as defined here https://in3.readthedocs.io/en/develop/api-java.html