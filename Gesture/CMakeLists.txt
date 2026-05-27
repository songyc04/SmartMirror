cmake_minimum_required(VERSION 3.10)
project(smart_mirror_gesture)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 멀티스레딩(pthread) 지원 활성화
find_package(Threads REQUIRED)

# 최적화 플래그 (젯슨 보드 성능 극대화)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native")
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# OpenCV 찾기
find_package(OpenCV REQUIRED)
if(NOT OpenCV_FOUND)
    message(FATAL_ERROR "OpenCV를 찾을 수 없습니다.")
endif()

message(STATUS "OpenCV 버전: ${OpenCV_VERSION}")

# 실행 파일 생성 (C++ 서버와 제스처 소스 함께 빌드)
add_executable(gesture_mirror_server
    server_opencv.cpp
    gesture_recognition.cpp
)

# 헤더 경로 지정 및 라이브러리 링크
target_include_directories(gesture_mirror_server PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(gesture_mirror_server PRIVATE 
    ${OpenCV_LIBS}
    Threads::Threads
)

message(STATUS "빌드 타입: ${CMAKE_BUILD_TYPE}")
