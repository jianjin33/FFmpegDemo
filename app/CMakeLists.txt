cmake_minimum_required(VERSION 3.4.1) #cmake最低版本要求

add_library(native-lib
             SHARED
             src/main/cpp/jni_ffmpeg.cpp )

include_directories(
#         src/main/cpp/include/libavcodec
         src/main/cpp/include/libavfilter
         src/main/cpp/include/libavformat
         src/main/cpp/include/libavutil
         src/main/cpp/include/libswresample
         src/main/cpp/include/libswscale )

#添加ffmpeg
add_library(ffmpeg SHARED IMPORTED)
set_target_properties(ffmpeg
        PROPERTIES IMPORTED_LOCATION
        ${CMAKE_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libffmpeg.so)


find_library( log-lib
              log )

target_link_libraries( native-lib
                       ffmpeg
                       ${log-lib} )
