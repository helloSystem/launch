cmake_minimum_required(VERSION 3.5)

project(launch LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# TODO: Make everything compile under Qt6
find_package(QT NAMES Qt5 REQUIRED COMPONENTS Widgets DBus Core Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets DBus Core Sql)

# Do not put qDebug() into Release builds
if(NOT CMAKE_BUILD_TYPE STREQUAL Debug)
    add_definitions(-DQT_NO_DEBUG_OUTPUT)
endif()

# Set rpath
set(CMAKE_INSTALL_RPATH $ORIGIN/../lib)

add_executable(launch
  src/launch.cpp
  src/dbmanager.h
  src/dbmanager.cpp
)

add_executable(bundle-thumbnailer
  src/bundle-thumbnailer.cpp
  src/dbmanager.h
  src/dbmanager.cpp
)

target_link_libraries(launch Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::DBus Qt${QT_VERSION_MAJOR}::Sql)

target_link_libraries(bundle-thumbnailer Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Sql)

# Allow for 'make install'
install(TARGETS launch bundle-thumbnailer
        RUNTIME DESTINATION bin)
