cmake_minimum_required(VERSION 3.28)
project(MissionPlanner4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)

qt_standard_project_setup()

qt_add_executable(MissionPlanner main.cpp)

target_link_libraries(MissionPlanner PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)

#add_executable(MissionPlanner4 main.cpp)
