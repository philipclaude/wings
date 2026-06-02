cmake_minimum_required(VERSION 3.10)

project(wings LANGUAGES CXX)
# TODO allow C++ 14 if abseil is not used
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-O3 -fPIC -Wall -Wextra")
set(CMAKE_C_FLAGS "-O3 -fPIC -Wall")
set_property(GLOBAL PROPERTY TARGET_MESSAGES OFF)
#set(CMAKE_CXX_FLAGS "-O0 -g -fsanitize=address")

set(ABSL_PROPAGATE_CXX_STD ON)
set(ABSL_USE_SYSTEM_INCLUDES ON)

# setup binary and lib directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(LIBRARIES)
if (APPLE)
	find_library(OpenGL_LIBRARY OpenGL)
	set(LIBRARIES ${LIBRARIES} ${OpenGL_LIBRARY})
else()
	find_package(OpenGL COMPONENTS REQUIRED OpenGL EGL)
	set(LIBRARIES ${LIBRARIES} OpenGL::EGL pthread)
endif()

set(extern_repositories)
function(add_extern_repository name)
	set(options "")
	set(one_value_args GIT_REPOSITORY FULL_HISTORY SKIP_CONFIG)
	set(multi_value_args "")
	cmake_parse_arguments(ARG "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})
	if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/${name})	
		if(ARG_GIT_REPOSITORY)
			set(clone_opts --recursive --depth=1 --single-branch)
			if(ARG_FULL_HISTORY)
				set(clone_opts --recursive)
			endif()
			set(fetch_get git)
			set(fetch_url ${ARG_GIT_REPOSITORY})
			set(fetch_arg clone ${clone_opts} ${ARG_GIT_REPOSITORY} ${CMAKE_CURRENT_SOURCE_DIR}/third_party/${name})
		else()
			message(FATAL_ERROR "unknown repository type")
		endif()
		message(STATUS "fetching ${name} from ${fetch_url}")
		execute_process(COMMAND ${fetch_get} ${fetch_arg} RESULT_VARIABLE status OUTPUT_QUIET ERROR_QUIET)
	endif()
	if(NOT ARG_SKIP_CONFIG)
		add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/${name} EXCLUDE_FROM_ALL)
	endif()
	list(APPEND extern_repositories "${CMAKE_CURRENT_SOURCE_DIR}/third_party/${name}")
	set(extern_repositories ${extern_repositories} PARENT_SCOPE)
endfunction()

# external repositories
set(external_libraries fmt)

add_library(wings SHARED wings.cpp)
set_property(TARGET wings PROPERTY CXX_STANDARD 17)
target_link_libraries(wings ${LIBRARIES})

option(WINGS_BUILD_UTIL "build utilities" TRUE)
option(WINGS_BUILD_APPS "build examples & applications" TRUE)

set(WINGS_STB_LOCATION "__stb_not_included__" CACHE STRING "location of stb installation")
if (${WINGS_STB_LOCATION} MATCHES "__stb_not_included__")
	add_extern_repository(stb GIT_REPOSITORY "https://github.com/nothings/stb" SKIP_CONFIG TRUE)
	set(WINGS_STB_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb)
	set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES ${CMAKE_CURRENT_SOURCE_DIR}/stb)
	add_definitions(-DWINGS_COMPILE_STB=1)
endif()

# set include directories
set(WINGS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/util ${CMAKE_CURRENT_SOURCE_DIR}/third_party
	${CMAKE_CURRENT_SOURCE_DIR}/third_party/libmeshb/sources
	${CMAKE_CURRENT_SOURCE_DIR}/third_party/fmt/include
	${CMAKE_CURRENT_SOURCE_DIR}/extern/abseil
)
include_directories(${WINGS_INCLUDE_DIRS})

# libraries to link against
set(WINGS_EXTERNAL_LIBRARIES ${LIBRARIES} ${external_libraries})

include_directories(${WINGS_STB_LOCATION})

if (${WINGS_BUILD_UTIL})
	# add all the source files
	set(SOURCES
	util/bvh.cpp
	util/field.cpp
	util/log.cpp
	util/mesh.cpp
	util/shader.cpp
	util/texture.cpp
	util/util.cpp)

	# create an object library for all the sources
	add_library(wings_util_src OBJECT ${SOURCES})
	add_definitions(-DWINGS_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
	target_compile_definitions(wings_util_src PUBLIC -DWINGS_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

	# create both shared and static libraries with same name
	add_library(wings_util_shared SHARED $<TARGET_OBJECTS:wings_util_src>)
	set_target_properties(wings_util_shared PROPERTIES OUTPUT_NAME wings_util CLEAN_DIRECT_OUTPUT 1)
	target_link_libraries(wings_util_shared dl)
	if (UNIX AND NOT APPLE)
	target_link_libraries(wings_util_shared rt dl)
	endif()
	add_library(wings_util_static STATIC $<TARGET_OBJECTS:wings_util_src>)
	set_target_properties(wings_util_static PROPERTIES OUTPUT_NAME wings_util CLEAN_DIRECT_OUTPUT 1)

	target_link_libraries(wings_util_shared ${WINGS_EXTERNAL_LIBRARIES})
	target_link_libraries(wings_util_static ${WINGS_EXTERNAL_LIBRARIES})
endif()

if (${WINGS_BUILD_APPS})
	# external repositories
	add_extern_repository(tinyobjloader GIT_REPOSITORY "https://github.com/tinyobjloader/tinyobjloader")
	add_extern_repository(fmt GIT_REPOSITORY "https://github.com/fmtlib/fmt")
	add_extern_repository(libmeshb GIT_REPOSITORY "https://github.com/philipclaude/libMeshb")
	add_extern_repository(abseil GIT_REPOSITORY "https://github.com/abseil/abseil-cpp")
	#add_extern_repository(argparse GIT_REPOSITORY "https://github.com/p-ranav/argparse")
	#add_extern_repository(json GIT_REPOSITORY "https://github.com/nlohmann/json.git" SKIP_CONFIG TRUE)

	add_library(wings_io_src OBJECT util/io.cpp)
	target_link_libraries(wings_io_src Meshb.7 tinyobjloader)

	# example program
	add_executable(wings_example example/main.cpp)
	target_link_libraries(wings_example wings tinyobjloader)
	set_property(TARGET wings_example PROPERTY CXX_STANDARD 17)

	# wings main program
	add_executable(wings_exe app/main.cpp)
	set_property(TARGET wings_exe PROPERTY OUTPUT_NAME wings)
	target_link_libraries(wings_exe wings_util_static wings_io_src wings dl absl::flat_hash_set)
	target_compile_definitions(wings_exe PUBLIC -DWINGS_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
endif()

# utility to remove external repositories
add_custom_target(wings_clean_extern COMMAND rm -rf ${extern_repositories})


