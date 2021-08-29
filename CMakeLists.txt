cmake_minimum_required(VERSION 2.8)
project( SimuSound_rpi )

#set(CMAKE_FIND_DEBUG_MODE 1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# optimize but also add debug symbols
set(CMAKE_BUILD_TYPE RelWithDebInfo)

# To work on cifs ntfs drive
set( OpenCV_DIR ${CMAKE_SOURCE_DIR}/opencv4/lib/cmake/opencv4 )

find_package(OpenCV REQUIRED PATHS ./opencv4 NO_DEFAULT_PATH)
include_directories( ${OpenCV_INCLUDE_DIRS} )

include_directories(
    ./tensorflow
    ./flatbuffers
    ./tensorflow/tensorflow/lite/tools/make/downloads/absl
)

include_directories(
    /opt/vc/include
    /opt/vc/include/interface/vcos/pthreads
    /opt/vc/include/interface/vmcs_host
    /opt/vc/include/interface/vmcs_host/linux
)

link_directories(/opt/vc/lib)

# Mobiledet includes
include_directories(
    ${CMAKE_SOURCE_DIR}/tensorflow
    ${CMAKE_SOURCE_DIR}/flatbuffers
    ${CMAKE_SOURCE_DIR}/tensorflow/tensorflow/lite/tools/make/downloads/absl
)
include_directories(
    ${CMAKE_SOURCE_DIR}/edgetpu
)
include_directories(
    ${CMAKE_SOURCE_DIR}/mobiledet
)

# IMU library
include_directories( imu )
include_directories( imu/util )
add_subdirectory( imu )

# 3D SOUND & SOUND SYNTH & ENVIRONMENT MODEL
add_subdirectory( sound-environment-model )
add_subdirectory( flite )

include_directories( sound-environment-model/ )
include_directories( sound-environment-model/include )
include_directories( flite/lang )
include_directories( flite/include )

add_executable( SimuSound_rpi utils.cpp camera_usb.cpp camera_mipi.cpp ${CMAKE_SOURCE_DIR}/mobiledet/mobiledet.cpp stereo_matching.cpp test_driver.cpp )
target_link_directories(
    SimuSound_rpi BEFORE
    PUBLIC ./tensorflow/tensorflow/lite/
    PUBLIC ./flatbuffers/flatbuffers/
)
target_link_libraries(
    SimuSound_rpi
    ${OpenCV_LIBS}
    pthread
    libtensorflow-lite.a
    libflatbuffers.a
    /lib/arm-linux-gnueabihf/libdl.so.2
    ${CMAKE_SOURCE_DIR}/edgetpu/libedgetpu.so.1
    mmal_core
    mmal_util
    vcos
    bcm_host
    sound-environment-model-lib
    flite-voice-lib-cmu-us-kal16
    imu-lib
)

target_compile_options(SimuSound_rpi 
    PRIVATE -fpermissive
    PRIVATE -Wall
)

# SimuSound_rpi with env model
add_definitions(-DENGINE_DEBUG)
add_definitions(-DOPENGL_ES)
add_executable( SimuSound_rpi_final utils.cpp camera_usb.cpp camera_mipi.cpp unet.cpp stereo_matching.cpp main.cpp )
target_link_directories(
    SimuSound_rpi_final BEFORE
    PUBLIC ./tensorflow/tensorflow/lite/
    PUBLIC ./flatbuffers/flatbuffers/
)
target_link_libraries(
    SimuSound_rpi_final
    ${OpenCV_LIBS}
    pthread
    libtensorflow-lite.a
    libflatbuffers.a
    /lib/arm-linux-gnueabihf/libdl.so.2
    ${CMAKE_SOURCE_DIR}/edgetpu/libedgetpu.so.1
    mmal_core
    mmal_util
    vcos
    bcm_host
    sound-environment-model-lib
#    flite-voice-lib-cmu-us-kal
    flite-voice-lib-cmu-us-kal16
    imu-lib
)

target_compile_options(SimuSound_rpi_final 
    PRIVATE -fpermissive
    PRIVATE -Wall
)

# Audio test
add_definitions(-DENGINE_DEBUG)
add_definitions(-DOPENGL_ES)
add_executable( audio_test utils.cpp camera_usb.cpp camera_mipi.cpp unet.cpp stereo_matching.cpp audio_test.cpp)
target_link_directories(
    audio_test BEFORE
    PUBLIC ./tensorflow/tensorflow/lite/
    PUBLIC ./flatbuffers/flatbuffers/
)
target_link_libraries(
    audio_test
    ${OpenCV_LIBS}
    pthread
    libtensorflow-lite.a
    libflatbuffers.a
    /lib/arm-linux-gnueabihf/libdl.so.2
    ${CMAKE_SOURCE_DIR}/edgetpu/libedgetpu.so.1
    mmal_core
    mmal_util
    vcos
    bcm_host
    sound-environment-model-lib
#    flite-voice-lib-cmu-us-kal
    flite-voice-lib-cmu-us-kal16
    imu-lib
)

target_compile_options(audio_test 
    PRIVATE -fpermissive
    PRIVATE -Wall
)

# UNIT TEST
add_definitions(-DENGINE_DEBUG)
add_definitions(-DOPENGL_ES)
include_directories( ../flite/lang )
include_directories( ../flite/include )
add_executable( unit_test
    utils.cpp
    camera_usb.cpp
    camera_mipi.cpp
    unet.cpp
    stereo_matching.cpp
    unit_test.cpp
    unit_test_scenes/imu_scene.cpp )
target_link_directories(
    unit_test BEFORE
    PUBLIC ./tensorflow/tensorflow/lite/
    PUBLIC ./flatbuffers/flatbuffers/
)
target_link_libraries(
    unit_test
    ${OpenCV_LIBS}
    pthread
    libtensorflow-lite.a
    libflatbuffers.a
    /lib/arm-linux-gnueabihf/libdl.so.2
    ${CMAKE_SOURCE_DIR}/edgetpu/libedgetpu.so.1
    mmal_core
    mmal_util
    vcos
    bcm_host
    sound-environment-model-lib
#    flite-voice-lib-cmu-us-kal
    flite-voice-lib-cmu-us-kal16
    imu-lib
    stdc++fs
)

target_compile_options(unit_test 
    PRIVATE -fpermissive
    PRIVATE -Wall
)