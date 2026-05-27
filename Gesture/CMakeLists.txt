cmake_minimum_required(VERSION 3.10)
project(smart_mirror_gesture)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 최적화 플래그
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native")
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# OpenCV 찾기
find_package(OpenCV REQUIRED)

if(NOT OpenCV_FOUND)
    message(FATAL_ERROR "OpenCV를 찾을 수 없습니다. 설치 후 다시 시도하세요.")
endif()

message(STATUS "OpenCV 버전: ${OpenCV_VERSION}")
message(STATUS "OpenCV 경로: ${OpenCV_INCLUDE_DIRS}")

# 실행 파일 생성
add_executable(gesture_mirror
    main.cpp
    gesture_recognition.cpp
)

# 헤더 및 라이브러리 링크
target_include_directories(gesture_mirror PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(gesture_mirror PRIVATE ${OpenCV_LIBS})

message(STATUS "빌드 타입: ${CMAKE_BUILD_TYPE}")
