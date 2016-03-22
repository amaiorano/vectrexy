cmake_minimum_required (VERSION 2.6)
project (vectrexy)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP") # Multiprocessor build
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX") # Max warning level and error on warnings
	if(MSVC_VERSION LESS 1900) # Starting from MSVC 14 (2015), STL needs language extensions enabled
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Za") # Disable language extensions
	endif()
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

file(GLOB SRC "src/*.*")
add_executable(vectrexy ${SRC})
