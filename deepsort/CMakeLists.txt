cmake_minimum_required(VERSION 3.0.0)

find_package(OpenCV 4 REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# skip 3rd-party lib dependencies
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-shlib-undefined")

# install target and libraries
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/install/yolov5_deepsort_${CMAKE_SYSTEM_NAME})

set(CMAKE_SKIP_INSTALL_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "lib")

# rknn api
set(RKNN_API_PATH ${CMAKE_SOURCE_DIR}/3rdparty/librknn_api)
set(LIB_ARCH aarch64)
set(RKNN_RT_LIB ${CMAKE_SOURCE_DIR}/3rdparty/librknn_api/${LIB_ARCH}/librknnrt.so)

#rga
set(RGA_PATH ${CMAKE_SOURCE_DIR}/3rdparty/rga)
set(RGA_LIB ${RGA_PATH}/lib/librga.so)

include_directories(${RKNN_API_PATH}/include)
include_directories(${CMAKE_SOURCE_DIR}/3rdparty)
include_directories(${RGA_PATH}/include)
include_directories(${CMAKE_SOURCE_DIR}/yolov5/include)
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/deepsort/include)
include_directories(${PROJECT_SOURCE_DIR}/3rdparty/librknn_api/include)

message(${PROJECT_SOURCE_DIR})
message(${RKNN_API_PATH})

# set(
#     include_libs  
#     "${PROJECT_SOURCE_DIR}/include"
#     "${PROJECT_SOURCE_DIR}/deepsort/include"
#     ${PROJECT_SOURCE_DIR}/3rdparty/librknn_api/include
#     ${PROJECT_SOURCE_DIR}/3rdparty/rga/include
# )

# set(
#     dynamic_libs
#     ${PROJECT_SOURCE_DIR}/3rdparty/librknn_api/aarch64/librknnrt.so
#     ${PROJECT_SOURCE_DIR}/3rdparty/rga/lib/librga.so
# )

# include_directories(${include_libs})
aux_source_directory(${PROJECT_SOURCE_DIR}/deepsort/src DEEPSORT_SRC_DIR)
aux_source_directory(${PROJECT_SOURCE_DIR}/src DEEPSORT_SRC_DIR)

add_library(deepsort ${DEEPSORT_SRC_DIR})

target_link_libraries(deepsort ${OpenCV_LIBS} ${RKNN_RT_LIB} ${RGA_LIB})
# target_link_libraries(deepsort ${dynamic_libs})
set(CMAKE_INSTALL_RPATH lib)