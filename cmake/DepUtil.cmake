# Dependency utility functions for CMake

# If vcpkg is found, sets VCPKG_FOUND to TRUE, and sets CMAKE_TOOLCHAIN_FILE and VCPKG_TARGET_TRIPLET cache variables,
# otherwise VCPKG_FOUND is set to FALSE.
function(init_vcpkg_vars)
	# Look for CMAKE_TOOLCHAIN_FILE env var, or set it if possible
	set(VCPKG_FOUND FALSE)
	if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
		if(DEFINED ENV{VCPKG_ROOT})
			message(STATUS "vcpkg from environment variable VCPKG_ROOT will be used: $ENV{VCPKG_ROOT}")
			set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
			set(VCPKG_FOUND TRUE)
		elseif(EXISTS ${CMAKE_SOURCE_DIR}/../vcpkg)
			message(STATUS "vcpkg at sibling directory will be used: ${CMAKE_SOURCE_DIR}/../vcpkg")
			set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/../vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
			set(VCPKG_FOUND TRUE)
		endif()
	endif()

	# Look for VCPKG_TARGET_TRIPLET env var, or set default
	if(NOT DEFINED VCPKG_TARGET_TRIPLET)
		if(DEFINED ENV{VCPKG_DEFAULT_TRIPLET})
			message(STATUS "vcpkg triplet from environment variable VCPKG_DEFAULT_TRIPLET will be used: ENV{VCPKG_DEFAULT_TRIPLET}")
			set(VCPKG_TARGET_TRIPLET "$ENV{VCPKG_DEFAULT_TRIPLET}" CACHE STRING "")
		else()
			if(WIN32)
				set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "")
				message(STATUS "vcpkg triplet not found, will be set to: x64-windows-static")
			else()
				message(STATUS "vcpkg triplet not found, will use default for current platform")
			endif()
		endif()
	endif()

	set(VCPKG_FOUND ${VCPKG_FOUND} PARENT_SCOPE)
endfunction()
