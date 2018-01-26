cmake_minimum_required(VERSION 3.7)
project(libxyzimage VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 90)

find_package(ZLIB REQUIRED)

add_library(xyzimage
        include/xyzimage.h
        src/xyzimage.c)

target_include_directories(xyzimage PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(xyzimage ZLIB::ZLIB)

include(GNUInstallDirs)

install(TARGETS xyzimage
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
