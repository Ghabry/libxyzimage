cmake_minimum_required(VERSION 3.7)

project(libxyzimage VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 90)

add_library(xyzimage
	include/xyzimage.h
	src/xyzimage.c)

target_include_directories(
	xyzimage PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>)

find_package(ZLIB REQUIRED)
target_link_libraries(xyzimage ZLIB::ZLIB)

# pkg-config
set(PACKAGE_TARNAME ${PROJECT_NAME})
set(prefix "${CMAKE_INSTALL_PREFIX}")
set(exec_prefix "\${prefix}")
set(libdir "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
set(includedir "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
string(REPLACE ";" " " AX_PACKAGE_REQUIRES_PRIVATE "zlib")
configure_file(pkg-config/${PROJECT_NAME}.pc.in pkg-config/${PROJECT_NAME}.pc @ONLY)

# install
include(GNUInstallDirs)

install(
	TARGETS xyzimage
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(
	DIRECTORY include/
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/xyzimage)

install(
	FILES pkg-config/${PROJECT_NAME}.pc
	DESTINATION lib/pkgconfig
)

install(
	FILES pkg-config/xyzimage-config.cmake
	DESTINATION lib/cmake/xyzimage)
