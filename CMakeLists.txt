cmake_minimum_required(VERSION 3.9)
project(htm-index-compare)

set(CMAKE_MODULE_PATH
	${CMAKE_SOURCE_DIR}/cmake
)

include(add_buildtype_ultra)
include(add_buildtype_sanitize_debug)

add_subdirectory(vendor/liboscar liboscar)
add_subdirectory(vendor/lsst/sphgeom sphgeom)

set(LIB_SOURCES_CPP
	src/htm-index.cpp
	src/static-htm-index.cpp
)

set(LIB_SOURCES_H
	src/htm-index.h
	src/static-htm-index.h
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
)

add_library(lib${PROJECT_NAME} STATIC ${LIB_SOURCES_CPP} ${LIB_SOURCES_H})
target_link_libraries(lib${PROJECT_NAME} PUBLIC ${LINK_LIBS})

add_executable(${PROJECT_NAME} ${SOURCES_CPP})
target_link_libraries(${PROJECT_NAME} lib${PROJECT_NAME})

add_executable(${PROJECT_NAME}-create ${CREATE_SOURCES_CPP})
target_link_libraries(${PROJECT_NAME}-create lib${PROJECT_NAME})

add_executable(${PROJECT_NAME}-query ${QUERY_SOURCES_CPP})
target_link_libraries(${PROJECT_NAME}-query lib${PROJECT_NAME})
