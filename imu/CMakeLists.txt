cmake_minimum_required(VERSION 2.8)
project( imu_pi )

# optimize but also add debug symbols
set(CMAKE_BUILD_TYPE RelWithDebInfo)

include_directories(
    ./util/
)

add_library( imu-lib
    STATIC
    ICM_20948_rpi.cpp
    util/ICM_20948_C.c )
add_executable( imu_test imu_test.cpp )

target_link_libraries( imu-lib PUBLIC
    pigpio
)

target_link_libraries( imu_test
    imu-lib
)

target_compile_options( imu-lib
    PRIVATE -fpermissive
    PRIVATE -Wall
    PRIVATE -Wno-comment
)