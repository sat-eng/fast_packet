cmake_minimum_required(VERSION 3.20)
project(FastPacket VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# If Qt6 is not on PATH, set CMAKE_PREFIX_PATH in CLion's CMake settings:
#   macOS Homebrew:  /opt/homebrew/opt/qt6
#   Windows vcpkg:   <vcpkg-root>/installed/x64-windows
find_package(Qt6 COMPONENTS Core Widgets Network REQUIRED)

add_executable(FastPacket
    src/main.cpp
    src/MainWindow.cpp
    src/PacketSender.cpp
    src/PacketReceiver.cpp
    src/LossGraph.cpp
)

target_link_libraries(FastPacket PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
)

if(WIN32)
    set_target_properties(FastPacket PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
