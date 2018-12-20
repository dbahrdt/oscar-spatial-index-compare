cmake_minimum_required(VERSION 3.9)
project(osi-compare)

set(CMAKE_MODULE_PATH
	${CMAKE_SOURCE_DIR}/cmake
)

set(H3_PREFIX "h3_" CACHE STRING "H3 prefix for exported symbols" FORCE)

include(add_buildtype_ultra)
include(add_buildtype_sanitize_debug)

add_subdirectory(vendor/liboscar liboscar)
add_subdirectory(vendor/lsst/sphgeom sphgeom)
add_subdirectory(vendor/h3 h3lib)

set(LIB_SOURCES_CPP
	src/htm-index.cpp
	src/OscarSgIndex.cpp
	src/OscarSearchSgIndex.cpp
	src/static-htm-index.cpp
	src/SpatialGrid.h
	src/HtmSpatialGrid.cpp
	src/H3SpatialGrid.cpp
)

set(LIB_SOURCES_H
	src/htm-index.h
	src/OscarSgIndex.h
	src/OscarSearchSgIndex.h
	src/static-htm-index.h
	src/HtmSpatialGrid.h
	src/H3SpatialGrid.h
)

set(SOURCES_CPP
	src/main.cpp
)

set(QUERY_SOURCES_CPP
	src/mainq.cpp
)

set(CREATE_SOURCES_CPP
	src/mainc.cpp
)


SET(LINK_LIBS
	liboscar
	sphgeom
	h3
)

add_library(lib${PROJECT_NAME} STATIC ${LIB_SOURCES_CPP} ${LIB_SOURCES_H})
target_link_libraries(lib${PROJECT_NAME} PUBLIC ${LINK_LIBS})

add_executable(${PROJECT_NAME} ${SOURCES_CPP})
target_link_libraries(${PROJECT_NAME} lib${PROJECT_NAME})

add_executable(${PROJECT_NAME}-create ${CREATE_SOURCES_CPP})
target_link_libraries(${PROJECT_NAME}-create lib${PROJECT_NAME})

add_executable(${PROJECT_NAME}-query ${QUERY_SOURCES_CPP})
target_link_libraries(${PROJECT_NAME}-query lib${PROJECT_NAME})
