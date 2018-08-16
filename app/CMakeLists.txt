cmake_minimum_required(VERSION 3.4.1) #cmake最低版本要求

#添加curl库教材https://blog.csdn.net/yoyo_newbie/article/details/74427938

add_library(native-request
             SHARED
             src/main/cpp/jniRequest.cpp
             src/main/cpp/JniUtils.cpp
             src/main/cpp/web_task.cpp )

 include_directories( src/main/cpp/include/jsoncpp
                      src/main/cpp/include/curl )

#添加curl网络请求
add_library(curl STATIC IMPORTED)
set_target_properties(curl
  PROPERTIES IMPORTED_LOCATION
  ${CMAKE_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libcurl.a)

#添加json库
add_library(jsoncpp STATIC IMPORTED)
set_target_properties(jsoncpp
  PROPERTIES IMPORTED_LOCATION
  ${CMAKE_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libjsoncpp.a)
#https://blog.csdn.net/lcqlw123/article/details/77944411
find_library( log-lib
              log )

target_link_libraries( native-request
                       jsoncpp
                       curl
                       ${log-lib} )