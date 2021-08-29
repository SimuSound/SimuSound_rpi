cmake_minimum_required(VERSION 2.8)
project( bt_remote_test )

#set(CMAKE_FIND_DEBUG_MODE 1)

# optimize but also add debug symbols
set(CMAKE_BUILD_TYPE RelWithDebInfo)


add_executable( bt_remote_test bt_remote_test.cpp ../bt_remote.cpp)
target_link_libraries(
    bt_remote_test
    pthread
)

target_compile_options(bt_remote_test 
    PRIVATE -fpermissive
    PRIVATE -Wall
)



add_executable( ble_remote_test ble_remote_test.cpp ../sound-environment-model/remote_control_io.cpp)
target_include_directories( ble_remote_test
    PUBLIC ${CMAKE_SOURCE_DIR}/../gattlib/include/
    PUBLIC ${CMAKE_SOURCE_DIR}/../sound-environment-model/
    PUBLIC ${CMAKE_SOURCE_DIR}/../sound-environment-model/include/
)
target_link_directories( ble_remote_test BEFORE
    PUBLIC ${CMAKE_SOURCE_DIR}/../gattlib/lib/
)

target_link_libraries( ble_remote_test
    gattlib
)

target_compile_options(ble_remote_test 
    PRIVATE -fpermissive
    PRIVATE -Wall
)