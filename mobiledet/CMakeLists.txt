cmake_minimum_required(VERSION 2.8)
project( mobiledet_testing )

#set(CMAKE_FIND_DEBUG_MODE 1)

# optimize but also add debug symbols
set(CMAKE_BUILD_TYPE RelWithDebInfo)

# To work on cifs ntfs drive
set( OpenCV_DIR ${CMAKE_SOURCE_DIR}/../opencv4/lib/cmake/opencv4 )

find_package(OpenCV REQUIRED PATHS ./opencv4 NO_DEFAULT_PATH)
include_directories( ${OpenCV_INCLUDE_DIRS} )

include_directories(
    ../tensorflow
    ../flatbuffers
    ../tensorflow/tensorflow/lite/tools/make/downloads/absl
)

include_directories(
    ../edgetpu
)

add_executable( mobiledet_testing mobiledet.cpp mobiledet_testing.cpp )
target_link_directories(
    mobiledet_testing BEFORE
    PUBLIC ../tensorflow/tensorflow/lite/
    PUBLIC ../flatbuffers/flatbuffers/
)
target_link_libraries(
    mobiledet_testing
    ${OpenCV_LIBS}
    pthread
    libtensorflow-lite.a
    libflatbuffers.a
    /lib/arm-linux-gnueabihf/libdl.so.2
    ${CMAKE_SOURCE_DIR}/../edgetpu/libedgetpu.so.1
)

target_compile_options(mobiledet_testing 
    PRIVATE -fpermissive
    PRIVATE -Wall
)
