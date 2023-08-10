cmake_minimum_required(VERSION 3.15)

project("test")

# Find the Qt5 package
find_package(Qt5 REQUIRED COMPONENTS Test Gui Widgets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src/)

# Find the Qt5 package and its components
find_package(Qt5 REQUIRED COMPONENTS Test Gui Widgets)

# Set up your project sources and headers
set(SOURCES
        testExecutable.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/Executable.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/Executable.cpp
        )

# Add the executable for your tests
add_executable(${PROJECT_NAME} ${SOURCES})

# Link against Qt libraries
target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::Test Qt5::Gui Qt5::Widgets)

# Define a CTest test
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME}_tests)