cmake_minimum_required(VERSION 3.0.0)
project(yolov5_deepsort VERSION 0.1.0)




# skip 3rd-party lib dependencies
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-shlib-undefined")

# install target and libraries
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/install/yolov5_deepsort_${CMAKE_SYSTEM_NAME})

set(CMAKE_SKIP_INSTALL_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

add_subdirectory(deepsort)

find_package(ZLIB REQUIRED)

find_package(OpenCV 4 REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})
find_package(Eigen3 REQUIRED)
include_directories(${EIGEN3_INCLUDE_DIRS})


# rknn api
set(RKNN_API_PATH ${CMAKE_SOURCE_DIR}/3rdparty/librknn_api)
set(LIB_ARCH aarch64)
set(RKNN_RT_LIB ${CMAKE_SOURCE_DIR}/3rdparty/librknn_api/${LIB_ARCH}/librknnrt.so)

#rga
set(RGA_PATH ${CMAKE_SOURCE_DIR}/3rdparty/rga)
set(RGA_LIB ${RGA_PATH}/lib/librga.so)

include_directories(${RKNN_API_PATH}/include)

# set(
#     include_libs  
#     "${PROJECT_SOURCE_DIR}/include"
#     "${PROJECT_SOURCE_DIR}/yolov5/include"
#     "${PROJECT_SOURCE_DIR}/deepsort/include"
#     ${PROJECT_SOURCE_DIR}/3rdparty/librknn_api/include
#     ${PROJECT_SOURCE_DIR}/3rdparty/rga/include
# )

# include_directories(${include_libs})

include_directories(${CMAKE_SOURCE_DIR}/3rdparty)
include_directories(${RGA_PATH}/include)
include_directories(${CMAKE_SOURCE_DIR}/yolov5/include)
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/deepsort/include)



aux_source_directory(${PROJECT_SOURCE_DIR}/yolov5/src YOLO_SRC_DIR)
aux_source_directory(${PROJECT_SOURCE_DIR}/src YOLO_SRC_DIR)
add_executable(yolov5_deepsort yolov5_deepsort.cpp ${YOLO_SRC_DIR})


set(CMAKE_INSTALL_RPATH "lib")

target_link_libraries(yolov5_deepsort ${OpenCV_LIBS} deepsort ${RKNN_RT_LIB} ${RGA_LIB} ${ZLIB_LIBRARIES})

# install target and libraries
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/install/yolov5_deepsort_${CMAKE_SYSTEM_NAME})
install(TARGETS yolov5_deepsort DESTINATION ./)
install(PROGRAMS ${RKNN_RT_LIB} DESTINATION lib)
install(PROGRAMS ${RGA_LIB} DESTINATION lib)
install(DIRECTORY model DESTINATION ./)
install(DIRECTORY data DESTINATION ./)
