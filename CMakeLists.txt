cmake_minimum_required(VERSION 3.9)
project(htm-index-compare)

set(CMAKE_MODULE_PATH
	${CMAKE_SOURCE_DIR}/cmake
)

include(add_buildtype_ultra)

add_subdirectory(vendor/liboscar liboscar)
add_subdirectory(vendor/lsst/sphgeom sphgeom)

set(SOURCES_CPP
	src/main.cpp
	src/htm-index.cpp
	src/static-htm-index.cpp
)

set(SOURCES_H
	src/htm-index.h
	src/static-htm-index.h
)

SET(LINK_LIBS
	liboscar
	sphgeom
)

add_executable(${PROJECT_NAME} ${SOURCES_CPP} ${SOURCES_H})
target_link_libraries(${PROJECT_NAME} ${LINK_LIBS})
