cmake_minimum_required(VERSION 3.20)
project(PaintStation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Qt6
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# ---- OpenCV ----
if(NOT OpenCV_DIR)
    set(OpenCV_DIR "C:/opencv/build/x64/vc16/lib" CACHE PATH "OpenCV config dir")
endif()
find_package(OpenCV REQUIRED)
message(STATUS "OpenCV: ${OpenCV_DIR}")

# ---- RealSense D435i（解压版，手动指路径） ----
set(REALSENSE_ROOT "C:/Users/ADMIN/Documents/RealSense SDK 2.0" CACHE PATH "RealSense SDK root")
set(HAVE_LIBREALSENSE 1)
set(REALSENSE_INCLUDE_DIR "${REALSENSE_ROOT}/include")
set(REALSENSE_LIB_DIR     "${REALSENSE_ROOT}/lib/x64")
set(REALSENSE_DLL         "${REALSENSE_ROOT}/bin/x64/realsense2.dll")
message(STATUS "RealSense: ${REALSENSE_ROOT}")

# ---- 埃夫特本地 SDK (V2.8.0) ----
set(EFORT_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../V2.8.0")
set(EFORT_INCLUDE_DIR "${EFORT_SDK_ROOT}/include")
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(EFORT_LIB_DIR "${EFORT_SDK_ROOT}/libs/x64")
else()
    set(EFORT_LIB_DIR "${EFORT_SDK_ROOT}/libs/x86")
endif()
set(EFORT_LIBS
    "${EFORT_LIB_DIR}/EftSdk.lib"
    "${EFORT_LIB_DIR}/log4cpp.lib"
    "${EFORT_LIB_DIR}/rlibcpp-bcc.lib"
    "${EFORT_LIB_DIR}/rlibcpp-tool.lib"
)

set(SRC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(PROJECT_SOURCES
    ${SRC_ROOT}/main.cpp
    ${SRC_ROOT}/userdata/AppConfig.cpp         ${SRC_ROOT}/userdata/AppConfig.h
    ${SRC_ROOT}/userdata/PointStore.cpp        ${SRC_ROOT}/userdata/PointStore.h
    ${SRC_ROOT}/userdata/FlowStore.cpp         ${SRC_ROOT}/userdata/FlowStore.h
    ${SRC_ROOT}/robot/RobotController.cpp      ${SRC_ROOT}/robot/RobotController.h
    ${SRC_ROOT}/robot/RobotTypes.cpp           ${SRC_ROOT}/robot/RobotTypes.h
    ${SRC_ROOT}/vision/Camera.h
    ${SRC_ROOT}/vision/RealSenseCamera.cpp     ${SRC_ROOT}/vision/RealSenseCamera.h
    ${SRC_ROOT}/vision/HandEyeCalibrator.cpp    ${SRC_ROOT}/vision/HandEyeCalibrator.h
    ${SRC_ROOT}/ui/MainWindow.cpp              ${SRC_ROOT}/ui/MainWindow.h
    ${SRC_ROOT}/ui/ConfigPage.cpp               ${SRC_ROOT}/ui/ConfigPage.h
    ${SRC_ROOT}/ui/CalibVerifyPage.cpp         ${SRC_ROOT}/ui/CalibVerifyPage.h
    ${SRC_ROOT}/ui/PointManagerPage.cpp        ${SRC_ROOT}/ui/PointManagerPage.h
    ${SRC_ROOT}/ui/FlowEditPage.cpp            ${SRC_ROOT}/ui/FlowEditPage.h
    ${SRC_ROOT}/ui/ImageWidget.cpp             ${SRC_ROOT}/ui/ImageWidget.h
)

add_executable(${PROJECT_NAME} WIN32 ${PROJECT_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${SRC_ROOT}
    ${SRC_ROOT}/ui
    ${SRC_ROOT}/robot
    ${SRC_ROOT}/vision
    ${SRC_ROOT}/userdata
    ${EFORT_INCLUDE_DIR}
    ${REALSENSE_INCLUDE_DIR}
    ${OpenCV_INCLUDE_DIRS}
)

target_link_directories(${PROJECT_NAME} PRIVATE
    ${EFORT_LIB_DIR}
    ${REALSENSE_LIB_DIR}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_LIBREALSENSE=1)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core Qt6::Widgets
    ${OpenCV_LIBS}
    realsense2.lib
    ${EFORT_LIBS}
)

# POST_BUILD: 拷 DLL 到 exe 目录
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${EFORT_LIB_DIR}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${REALSENSE_DLL}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/realsense2.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "C:/opencv/build/x64/vc16/bin/opencv_world4120d.dll" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/opencv_world4120d.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/resources" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources"
        COMMENT "Copy SDK/OpenCV/RealSense DLLs + QSS next to exe"
    )
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(PaintStationTests tests/review_tests.cpp
        src/vision/HandEyeCalibrator.cpp src/userdata/PointStore.cpp
        src/userdata/FlowStore.cpp src/robot/RobotTypes.cpp)
    target_include_directories(PaintStationTests PRIVATE src/vision src/robot src/userdata ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(PaintStationTests PRIVATE Qt6::Core ${OpenCV_LIBS})
    add_test(NAME PaintStationTests COMMAND PaintStationTests)
endif()
