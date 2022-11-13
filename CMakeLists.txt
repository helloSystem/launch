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
  src/dbmanager.h
  src/dbmanager.cpp
  src/applicationinfo.h
  src/applicationinfo.cpp
  src/appdiscovery.h
  src/appdiscovery.cpp
  src/extattrs.h
  src/extattrs.cpp
)

add_executable(open
  src/launch.cpp
  src/dbmanager.h
  src/dbmanager.cpp
  src/applicationinfo.h
  src/applicationinfo.cpp
  src/appdiscovery.h
  src/appdiscovery.cpp
  src/extattrs.h
  src/extattrs.cpp
)

add_executable(xdg-open
  src/launch.cpp
  src/dbmanager.h
  src/dbmanager.cpp
  src/applicationinfo.h
  src/applicationinfo.cpp
  src/appdiscovery.h
  src/appdiscovery.cpp
  src/extattrs.h
  src/extattrs.cpp
)

add_executable(bundle-thumbnailer
  src/bundle-thumbnailer.cpp
  src/dbmanager.h
  src/dbmanager.cpp
  src/extattrs.h
  src/extattrs.cpp
)

if (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
target_link_libraries(launch Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::DBus Qt${QT_VERSION_MAJOR}::Sql KF5::WindowSystem procstat)
target_link_libraries(open Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::DBus Qt${QT_VERSION_MAJOR}::Sql KF5::WindowSystem procstat)
target_link_libraries(xdg-open Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::DBus Qt${QT_VERSION_MAJOR}::Sql KF5::WindowSystem procstat)
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
target_link_libraries(launch Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::DBus Qt${QT_VERSION_MAJOR}::Sql KF5::WindowSystem)
target_link_libraries(open Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::DBus Qt${QT_VERSION_MAJOR}::Sql KF5::WindowSystem)
target_link_libraries(xdg-open Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::DBus Qt${QT_VERSION_MAJOR}::Sql KF5::WindowSystem)
endif()

ADD_CUSTOM_TARGET(link_target ALL
                  COMMAND ${CMAKE_COMMAND} -E create_symlink launch open)

target_link_libraries(bundle-thumbnailer Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Sql)

# Allow for 'make install'
install(TARGETS launch open bundle-thumbnailer
        RUNTIME DESTINATION bin)

# On most systems, sbin has priority on the $PATH over bin
install(TARGETS xdg-open
        RUNTIME DESTINATION sbin)
