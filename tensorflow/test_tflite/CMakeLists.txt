cmake_minimum_required(VERSION 2.8)
project( get_ver )

#set(CMAKE_FIND_DEBUG_MODE 1)

# optimize but also add debug symbols
set(CMAKE_BUILD_TYPE RelWithDebInfo)

include_directories(
    ../../tensorflow
    ../../flatbuffers
    ../../tensorflow/tensorflow/lite/tools/make/downloads/absl
)

add_executable( get_ver get_ver.cpp)
target_link_directories(
    get_ver BEFORE
    PUBLIC ../../tensorflow/tensorflow/lite/
    PUBLIC ../../flatbuffers/flatbuffers/
)
target_link_libraries(
    get_ver
    pthread
    libtensorflow-lite.a
    libflatbuffers.a
    /lib/arm-linux-gnueabihf/libdl.so.2
)

target_compile_options(get_ver 
    PRIVATE -fpermissive
    PRIVATE -Wall
)