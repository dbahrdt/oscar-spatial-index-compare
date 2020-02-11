cmake_minimum_required(VERSION 3.9)
project(osi-compare)

set(CMAKE_MODULE_PATH
	${CMAKE_SOURCE_DIR}/cmake
	${CMAKE_SOURCE_DIR}/vendor/s2geometry/third_party/cmake
)

set(H3_PREFIX "h3_" CACHE STRING "H3 prefix for exported symbols" FORCE)

include(add_buildtype_ultra)
include(add_buildtype_sanitize_debug)

add_subdirectory(vendor/liboscar liboscar)
add_subdirectory(vendor/lsst/sphgeom sphgeom)
add_subdirectory(vendor/h3 h3lib)
add_subdirectory(vendor/s2geometry s2geometry)

set(LIB_SOURCES_CPP
	src/OscarSearchWithSg.cpp
	src/OscarSgIndex.cpp
	src/OscarSearchSgIndex.cpp
	src/static-htm-index.cpp
	src/HtmSpatialGrid.cpp
	src/H3SpatialGrid.cpp
	src/S2GeomSpatialGrid.cpp
	src/HcqrOpTree.cpp
	src/GeoHierarchyHCQRCompleter.cpp
	src/HCQRCompleter.cpp
	src/SpatialGridInitializer.cpp
)

set(LIB_SOURCES_H
	include/hic/OscarSearchWithSg.h
	include/hic/OscarSgIndex.h
	include/hic/OscarSearchSgIndex.h
	include/hic/static-htm-index.h
	include/hic/HtmSpatialGrid.h
	include/hic/H3SpatialGrid.h
	include/hic/S2GeomSpatialGrid.h
	include/hic/HcqrOpTree.h
	include/hic/GeoHierarchyHCQRCompleter.h
	include/hic/HCQRCompleter.h
)

set(SOURCES_CPP
	apps/main.cpp
)

set(QUERY_SOURCES_CPP
	apps/mainq.cpp
)

set(CREATE_SOURCES_CPP
	apps/mainc.cpp
)

SET(LINK_LIBS
	liboscar
	sphgeom
	h3
	s2
)

add_library(lib${PROJECT_NAME} STATIC ${LIB_SOURCES_CPP} ${LIB_SOURCES_H})
target_link_libraries(lib${PROJECT_NAME} PUBLIC ${LINK_LIBS})
target_include_directories(lib${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(lib${PROJECT_NAME} PUBLIC cxx_std_17)

add_executable(${PROJECT_NAME} ${SOURCES_CPP})
target_link_libraries(${PROJECT_NAME} lib${PROJECT_NAME})

add_executable(${PROJECT_NAME}-create ${CREATE_SOURCES_CPP})
target_link_libraries(${PROJECT_NAME}-create lib${PROJECT_NAME})

add_executable(${PROJECT_NAME}-query ${QUERY_SOURCES_CPP})
target_link_libraries(${PROJECT_NAME}-query lib${PROJECT_NAME})
