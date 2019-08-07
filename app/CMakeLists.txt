# Sets the minimum version of CMake required to build your native library.
# This ensures that a certain set of CMake features is available to
# your build.

cmake_minimum_required(VERSION 3.4.1)

OPTION(FAST_MATH "Math optimizations" OFF)
IF(FAST_MATH)
  MESSAGE ( STATUS "Enable math optimizations (excutable size may increase)" )
  ADD_DEFINITIONS(-DIN3_MATH_FAST)
ELSE()
  MESSAGE ( STATUS "Disable math optimizations (optimised for executable size)" )
  ADD_DEFINITIONS(-DIN3_MATH_LITE)
ENDIF(FAST_MATH)


foreach(module core verifier/eth1/nano verifier/eth1/basic verifier/eth1/full bindings/java third-party/crypto third-party/tommath )
        file(MAKE_DIRECTORY in3-core/src/${module}/outputs)
        add_subdirectory(
                in3-core/src/${module}
                in3-core/src/${module}/outputs)
endforeach()

