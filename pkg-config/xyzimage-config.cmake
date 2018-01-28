#.rst:
# FindXYZImage
# ---------
#
# Find the native libxyzimage headers and library.
#
# Imported Targets
# ^^^^^^^^^^^^^^^^
#
# This module defines the following :prop_tgt:`IMPORTED` targets:
#
# ``xyzimage::xyzimage``
#   The ``xyzimage`` library, if found.
#
# Result Variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
# ``XYZIMAGE_INCLUDE_DIRS``
#   where to find xyzimage.h.
# ``XYZIMAGE_LIBRARIES``
#   the libraries to link against to use libxyzimage.
# ``XYZIMAGE_FOUND``
#   true if the libxyzimage headers and libraries were found.
#

find_package(PkgConfig QUIET)

find_package(ZLIB REQUIRED QUIET)

pkg_check_modules(PC_XYZIMAGE QUIET xyzimage)

# Look for the header file.
find_path(XYZIMAGE_INCLUDE_DIR
	NAMES xyzimage.h libxyzimage/xyzimage.h xyzimage/xyzimage.h
	HINTS ${PC_XYZIMAGE_INCLUDE_DIRS})

# Look for the library.
find_library(XYZIMAGE_LIBRARY
	NAMES xyzimage libxyzimage
	HINTS ${PC_XYZIMAGE_LIBRARY_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(xyzimage DEFAULT_MSG XYZIMAGE_LIBRARY XYZIMAGE_INCLUDE_DIR)

# Copy the results to the output variables and target.
if(XYZIMAGE_FOUND)
	set(XYZIMAGE_LIBRARIES ${XYZIMAGE_LIBRARY})
	set(XYZIMAGE_INCLUDE_DIRS ${XYZIMAGE_INCLUDE_DIR})

	if(NOT TARGET xyzimage::xyzimage)
		add_library(xyzimage::xyzimage UNKNOWN IMPORTED)

		set_target_properties(xyzimage::xyzimage PROPERTIES
			IMPORTED_LINK_INTERFACE_LANGUAGES "C"
			IMPORTED_LOCATION "${XYZIMAGE_LIBRARY}"
			INTERFACE_INCLUDE_DIRECTORIES "${XYZIMAGE_INCLUDE_DIRS}"
			INTERFACE_LINK_LIBRARIES ZLIB::ZLIB)
	endif()
endif()

mark_as_advanced(XYZIMAGE_INCLUDE_DIR XYZIMAGE_LIBRARY)
