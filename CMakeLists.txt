cmake_minimum_required(VERSION 3.5)

project(launch LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# probono: Set rpath
set(CMAKE_INSTALL_RPATH $ORIGIN/../lib)

find_package(Qt5 COMPONENTS Widgets DBus REQUIRED)

add_executable(launch
  src/main.cpp
)

target_link_libraries(launch PRIVATE Qt5::Widgets Qt5::DBus)

# probono: Make installable
install(TARGETS launch
        RUNTIME DESTINATION bin)
