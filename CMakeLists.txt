cmake_minimum_required(VERSION 3.30)
project(EchoVision)

set(CMAKE_CXX_STANDARD 20)

# 根据配置选择合适的OpenCV路径
if(CMAKE_BUILD_TYPE STREQUAL "x86_64")
    # 对于x86_64, 使用系统自带的OpenCV库
    add_compile_definitions(__X86_64)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/output/x86_64/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/output/x86_64/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/output/x86_64/lib)
    set(ONNXRUNTIME_ROOT /opt/onnxruntime-linux-x64-1.21.0)
    find_package(OpenCV REQUIRED HINTS /usr/local/lib/cmake/opencv4)
    find_package(yaml-cpp REQUIRED)
    find_package(onnxruntime REQUIRED HINTS ${ONNXRUNTIME_ROOT})
    include_directories(${ONNXRUNTIME_ROOT}/include)
    set(ONNXRUNTIME_LIBS
            ${ONNXRUNTIME_ROOT}/lib/libonnxruntime.so
            ${ONNXRUNTIME_ROOT}/lib/libonnxruntime_providers_shared.so
    )
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libswscale)

    # 包含 FFmpeg 的头文件路径
    include_directories(${FFMPEG_INCLUDE_DIRS})
    set(FFMPEG_LIBS ${FFMPEG_LIBRARIES})
elseif(CMAKE_BUILD_TYPE STREQUAL "aarch64")
    # 对于aarch64, 使用指定路径下的OpenCV库
    add_compile_definitions(__AARCH64__)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/output/aarch64/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/output/aarch64/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/output/aarch64/lib)
    set(OPENCV_DIR /opt/opencv-4.11.0-aarch64)
    find_package(OpenCV REQUIRED PATHS ${OPENCV_DIR})
    find_package(yaml-cpp REQUIRED)
    include_directories(${OPENCV_DIR}/include/opencv4)
    set(OpenCV_LIBS
            ${OPENCV_DIR}/lib/libopencv_core.so
            ${OPENCV_DIR}/lib/libopencv_highgui.so
            ${OPENCV_DIR}/lib/libopencv_imgcodecs.so
            ${OPENCV_DIR}/lib/libopencv_imgproc.so
            ${OPENCV_DIR}/lib/libopencv_videoio.so
    )
else()
    message(FATAL_ERROR "Unsupported build type: ${CMAKE_BUILD_TYPE}")
endif()

include_directories(
        include
        peripherals/DualLensCamera/include
        core/HAL/include
        peripherals/GNSS/include
        Abilities/AiAbility/General/include
        # Abilities/AiAbility/Ascend/include
        Abilities/NetworkAbility/include
        Abilities/StreamAbility/include
)

add_executable(EchoVision
        EchoVision.cpp
        include/EchoVision.h
        peripherals/DualLensCamera/src/DualLensCamera.cpp
        peripherals/DualLensCamera/include/DualLensCamera.h
        core/HAL/include/HAL.h
        core/HAL/include/HAL_GPIO.h
        core/HAL/include/HAL_UART.h
        core/HAL/src/HAL_UART.cpp
        peripherals/GNSS/src/GNSS.cpp
        peripherals/GNSS/include/GNSS.h
        Abilities/AiAbility/General/src/ONNX.cpp
        Abilities/AiAbility/General/include/ONNX.h
        Abilities/NetworkAbility/src/NetworkAbility.cpp
        Abilities/NetworkAbility/include/NetworkAbility.h
        # Abilities/AiAbility/Ascend/src/CANN.cpp
        # Abilities/AiAbility/Ascend/include/CANN.h
        Abilities/StreamAbility/src/LiveStream.cpp
        Abilities/StreamAbility/include/LiveStream.h
)
if(CMAKE_BUILD_TYPE STREQUAL "x86_64")
    target_link_libraries(EchoVision
            ${OpenCV_LIBS}
            ${ONNXRUNTIME_LIBS}
            yaml-cpp::yaml-cpp
            ${FFMPEG_LIBS}  # 添加FFmpeg库到这里
    )
elseif(CMAKE_BUILD_TYPE STREQUAL "aarch64")
    target_link_libraries(EchoVision ${OpenCV_LIBS} yaml-cpp::yaml-cpp)
endif()