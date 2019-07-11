# Example Android Project using IN3

## How to use IN3 in android project.
1. Download [in3-core](https://git.slock.it/in3/c/in3-core.git).
2. Copy folders core, eth_api, eth_basic, eth_full, eth_nano and libin3 inside in3-core/src into android project folder app/src/main.
3. Copy folder in3 inside in3-core/src/java into android project folder app/src/main/java.
4. Make a top-level CMakeLists.txt in android project inside app folder then link this to gradle. Follow steps using this [guide](https://developer.android.com/studio/projects/gradle-external-native-builds on howto link).
5. Inside CMakeLists.txt specify the src and build folder of in3 copied folders except app/src/main/java/in3.
6. Remove include command for each CMakeLists.txt inside copied folders.
7. Remove transport_curl and in3.c in libin3/CMakeLists.txt.
8. Use methods available in app/src/main/java/in3/IN3.java from android activity to access IN3 functions.