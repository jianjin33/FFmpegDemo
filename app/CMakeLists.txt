cmake_minimum_required(VERSION 3.4.1) # cmake最低版本要求

add_library(native-request
             SHARED
             src/main/cpp/jni-request.cpp
             src/main/cpp/JniUtils.cpp
             src/main/cpp/web_task.cpp )

 include_directories( src/main/cpp/include/jsoncpp
                      src/main/cpp/include/curl )

add_library(curl STATIC IMPORTED)
set_target_properties(curl
  PROPERTIES IMPORTED_LOCATION
  ${CMAKE_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libcurl.a)

add_library(jsoncpp STATIC IMPORTED)
set_target_properties(jsoncpp
  PROPERTIES IMPORTED_LOCATION
  ${CMAKE_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libjsoncpp.a)




find_library( log-lib
              log )

target_link_libraries( native-request
                       jsoncpp
                       curl
                       ${log-lib} )