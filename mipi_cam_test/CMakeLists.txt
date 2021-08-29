cmake_minimum_required(VERSION 2.8)
project( mipi_cam_test )

#set(CMAKE_FIND_DEBUG_MODE 1)

# optimize but also add debug symbols
set(CMAKE_BUILD_TYPE RelWithDebInfo)

find_package(OpenCV REQUIRED PATHS ../opencv4 NO_DEFAULT_PATH)
include_directories( ${OpenCV_INCLUDE_DIRS} )
# include_directories(
#     /opt/vc/include/
#     /opt/vc/src/hello_pi/libs/vgfont/
# )
include_directories(/opt/vc/include)
include_directories(/opt/vc/include/interface/vcos/pthreads)
include_directories(/opt/vc/include/interface/vmcs_host)
include_directories(/opt/vc/include/interface/vmcs_host/linux)

# include_directories(/home/pi/src/raspberrypi/userland)
# include_directories(/opt/vc/src/hello_pi/libs/vgfont)

link_directories(/opt/vc/lib)
# link_directories(/opt/vc/src/hello_pi/libs/vgfont)

add_executable( mipi_cam_test ../utils.cpp ../stereo_matching.cpp main.cpp)
target_link_libraries(
    mipi_cam_test
    ${OpenCV_LIBS}
    pthread
)

add_executable( mmal_window mmal_window.cpp)
target_link_directories(
    mmal_window
    PUBLIC /opt/vc/lib/
)
target_link_libraries(
    mmal_window
    mmal_core
    mmal_util
    vcos
    bcm_host
    ${OpenCV_LIBS}
    pthread
)
target_compile_options(mmal_window 
    PRIVATE -fpermissive
    PRIVATE -O2
    PRIVATE -march=native
)