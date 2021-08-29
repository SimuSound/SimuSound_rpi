cmake_minimum_required(VERSION 2.8)
project( stereo_cal )

# optimize but also add debug symbols
set(CMAKE_BUILD_TYPE RelWithDebInfo)

find_package(OpenCV REQUIRED PATHS ../opencv4 NO_DEFAULT_PATH)
include_directories( ${OpenCV_INCLUDE_DIRS} )

include_directories(
    /opt/vc/include
    /opt/vc/include/interface/vcos/pthreads
    /opt/vc/include/interface/vmcs_host
    /opt/vc/include/interface/vmcs_host/linux
)

link_directories(/opt/vc/lib)

include_directories( ../ )

add_executable( stereo_cal ../utils.cpp ../camera_mipi.cpp ../extra_args.cpp stereo_cal.cpp)
target_link_libraries(
    stereo_cal
    ${OpenCV_LIBS}
    pthread
    mmal_core
    mmal_util
    vcos
    bcm_host
)

target_compile_options(stereo_cal 
    PRIVATE -fpermissive
    PRIVATE -Wall
)


add_executable( stereo_undistort ../utils.cpp ../camera_mipi.cpp ../stereo_matching.cpp stereo_undistort.cpp)
target_link_libraries(
    stereo_undistort
    ${OpenCV_LIBS}
    pthread
    mmal_core
    mmal_util
    vcos
    bcm_host
)

target_compile_options(stereo_cal 
    PRIVATE -fpermissive
    PRIVATE -Wall
)