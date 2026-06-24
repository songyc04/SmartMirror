cmake_minimum_required(VERSION 3.10)
project(smart_mirror)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 자동 처리
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 최적화
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native")
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Qt6 (없으면 Qt5 사용)
find_package(Qt6 COMPONENTS Widgets Network QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 COMPONENTS Widgets Network REQUIRED)
    set(QT_LIBS Qt5::Widgets Qt5::Network)
else()
    set(QT_LIBS Qt6::Widgets Qt6::Network)
endif()

# OpenCV
find_package(OpenCV REQUIRED)

# 멀티스레딩
find_package(Threads REQUIRED)

message(STATUS "OpenCV 버전: ${OpenCV_VERSION}")
message(STATUS "빌드 타입: ${CMAKE_BUILD_TYPE}")

# 실행 파일
add_executable(smart_mirror
    main.cpp
    mainwindow.cpp
    mainwindow.h
    gesture_recognition.cpp
    gesture_recognition.hpp
)

target_include_directories(smart_mirror PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(smart_mirror PRIVATE
    ${QT_LIBS}
    ${OpenCV_LIBS}
    Threads::Threads
)
