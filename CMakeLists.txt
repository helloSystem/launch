# To build without debug output, run:
# cmake .. -DCMAKE_BUILD_TYPE=Release

cmake_minimum_required(VERSION 3.5)

# Add the tests subdirectory
add_subdirectory(tests)

# Do not print deprecated warnings for Qt5 or KF5
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")

project(launch LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# TODO: Make everything compile under Qt6
find_package(QT NAMES Qt5 REQUIRED COMPONENTS Widgets DBus Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets DBus Core)
find_package(KF5WindowSystem REQUIRED)

# Do not put qDebug() into Release builds
if(NOT CMAKE_BUILD_TYPE STREQUAL Debug)
    add_definitions(-DQT_NO_DEBUG_OUTPUT)
endif()

# Set rpath
set(CMAKE_INSTALL_RPATH $ORIGIN/../lib)


# FIXME: Instead of building the same source code three times
# under different names, find a way to install symlinks to the
# 'launch' binary in different paths
add_executable(launch
  src/launch.cpp
        src/DbManager.h
        src/DbManager.cpp
        src/ApplicationInfo.h
        src/ApplicationInfo.cpp
        src/AppDiscovery.h
        src/AppDiscovery.cpp
  src/extattrs.h
  src/extattrs.cpp
  src/launcher.h
  src/launcher.cpp
        src/ApplicationSelectionDialog.h
        src/ApplicationSelectionDialog.cpp
        src/ApplicationSelectionDialog.ui
  src/Executable.cpp
  src/Executable.h
)

add_executable(open
  src/launch.cpp
        src/DbManager.h
        src/DbManager.cpp
        src/ApplicationInfo.h
        src/ApplicationInfo.cpp
        src/AppDiscovery.h
        src/AppDiscovery.cpp
  src/extattrs.h
  src/extattrs.cpp
  src/launcher.h
  src/launcher.cpp
        src/ApplicationSelectionDialog.h
        src/ApplicationSelectionDialog.cpp
        src/ApplicationSelectionDialog.ui
  src/Executable.cpp
  src/Executable.h
)

add_executable(xdg-open
  src/launch.cpp
        src/DbManager.h
        src/DbManager.cpp
        src/ApplicationInfo.h
        src/ApplicationInfo.cpp
        src/AppDiscovery.h
        src/AppDiscovery.cpp
  src/extattrs.h
  src/extattrs.cpp
  src/launcher.h
  src/launcher.cpp
        src/ApplicationSelectionDialog.h
        src/ApplicationSelectionDialog.cpp
        src/ApplicationSelectionDialog.ui
  src/Executable.cpp
  src/Executable.h
)

add_executable(bundle-thumbnailer
  src/bundle-thumbnailer.cpp
        src/DbManager.h
        src/DbManager.cpp
  src/extattrs.h
  src/extattrs.cpp
)

if (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
target_link_libraries(launch   Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::DBus KF5::WindowSystem procstat)
target_link_libraries(open     Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::DBus KF5::WindowSystem procstat)
target_link_libraries(xdg-open Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::DBus KF5::WindowSystem procstat)
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
target_link_libraries(launch   Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::DBus KF5::WindowSystem)
target_link_libraries(open     Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::DBus KF5::WindowSystem)
target_link_libraries(xdg-open Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::DBus KF5::WindowSystem)
endif()

ADD_CUSTOM_TARGET(link_target ALL
                  COMMAND ${CMAKE_COMMAND} -E create_symlink launch open)

target_link_libraries(bundle-thumbnailer Qt${QT_VERSION_MAJOR}::Widgets)

# Allow for 'make install'
install(TARGETS launch open bundle-thumbnailer
        RUNTIME DESTINATION bin)

# On most systems, sbin has priority on the $PATH over bin
install(TARGETS xdg-open
        RUNTIME DESTINATION sbin)
