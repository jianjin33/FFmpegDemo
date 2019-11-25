cmake_minimum_required(VERSION 3.4.1) #cmake最低版本要求

add_library(native-lib
        SHARED
        src/main/cpp/jni_ffmpeg.cpp
        src/main/cpp/jni_ffmpeg2.cpp)

# 这里巨坑 之前写成 src/main/cpp/include/libavfilter src/main/cpp/include/libavformat src/main/cpp/include/libavformat...
# ndk编译会报错不通过，卡在这，其实写一个最外层就行
include_directories(src/main/cpp/include/ffmpeg src/main/cpp/include/libyuv)

#添加ffmpeg
add_library(ffmpeg SHARED IMPORTED)
set_target_properties(ffmpeg
        PROPERTIES IMPORTED_LOCATION
        ${CMAKE_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libffmpeg.so)

#添加libyuv
add_library(libyuv SHARED IMPORTED)
set_target_properties(libyuv
        PROPERTIES IMPORTED_LOCATION
        ${CMAKE_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libyuv.so)


find_library(log-lib
        log)

find_library(android
        android)

target_link_libraries(native-lib
        ffmpeg
        libyuv
        ${log-lib}
        ${android})
