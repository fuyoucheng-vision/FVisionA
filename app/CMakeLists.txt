# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.4.1)

# C++ flags.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

include_directories(${CMAKE_SOURCE_DIR}/../third_party/cardboard/include)
include_directories(${CMAKE_SOURCE_DIR}/../third_party/opencv/jni/include)
include_directories(${CMAKE_SOURCE_DIR}/../third_party/jsoncpp/include)
include_directories(${CMAKE_SOURCE_DIR}/../third_party/glm)

# Standard Android dependencies
find_library(android-lib android)
find_library(GLESv2-lib GLESv2)
find_library(log-lib log)
find_library(zip-lib z)

# cardboard 1.4.1
SET(cardboard_libs cardboard_api)

# cardboard libs
foreach (lib_name ${cardboard_libs})
    add_library(${lib_name} SHARED IMPORTED)
    set_target_properties(${lib_name} PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/../third_party/cardboard/libs/${CMAKE_ANDROID_ARCH_ABI}/lib${lib_name}.so)
endforeach (lib_name)

# opencv 3.4.15
SET(opencv_libs opencv_xfeatures2d opencv_features2d opencv_calib3d opencv_flann opencv_imgcodecs opencv_imgproc opencv_core)
SET(opencv_3rdparty_libs cpufeatures IlmImf ittnotify libjasper libjpeg-turbo libpng libprotobuf libtiff libwebp quirc tbb tegra_hal)

# opencv libs
foreach (lib_name ${opencv_libs})
    add_library(${lib_name} STATIC IMPORTED)
    set_target_properties(${lib_name} PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/../third_party/opencv/staticlibs/${CMAKE_ANDROID_ARCH_ABI}/lib${lib_name}.a)
endforeach (lib_name)
# opencv 3rdparty libs
foreach (lib_name ${opencv_3rdparty_libs})
    add_library(${lib_name} STATIC IMPORTED)
    set_target_properties(${lib_name} PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/../third_party/opencv/3rdparty/libs/${CMAKE_ANDROID_ARCH_ABI}/lib${lib_name}.a)
endforeach (lib_name)

# jsoncpp
SET(jsoncpp_libs jsoncpp)

# jsoncpp libs
foreach (lib_name ${jsoncpp_libs})
    add_library(${lib_name} STATIC IMPORTED)
    set_target_properties(${lib_name} PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/../third_party/jsoncpp/libs/${CMAKE_ANDROID_ARCH_ABI}/lib${lib_name}.a)
endforeach (lib_name)

# Sources
file(GLOB native_srcs
        "src/main/jni/*.cpp"
        "src/main/jni/cardboard/*.cpp" "src/main/jni/cardboard/*.cc"
        "src/main/jni/capture/*.cpp"
        "src/main/jni/stereo/*.cpp"
        "src/main/jni/display/*.cpp"
)
# Output binary
add_library(fvision_jni SHARED ${native_srcs})
# Build
target_link_libraries(fvision_jni
    ${jsoncpp_libs}
    ${opencv_libs}
    ${opencv_3rdparty_libs}
    ${cardboard_libs}
    ${android-lib}
    ${GLESv2-lib}
    ${log-lib}
    ${zip-lib}
)

add_custom_command(TARGET fvision_jni
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E
    copy_if_different
    ${CMAKE_SOURCE_DIR}/../third_party/cardboard/libs/${CMAKE_ANDROID_ARCH_ABI}/libcardboard_api.so
    ${CMAKE_SOURCE_DIR}/libs/${CMAKE_ANDROID_ARCH_ABI}/libcardboard_api.so
)
