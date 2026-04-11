cmake_minimum_required(VERSION 3.10)
project(tello_gazebo)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

option(TELLO_GAZEBO_BUILD_PLUGIN "Build Gazebo Sim system plugin (requires gz dev packages)" ON)

find_package(ament_cmake REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(tello_msgs REQUIRED)

# Resolve gz packages with a conservative version fallback list
set(_gz_sim_vers 10 9 8 7 6)
set(_gz_math_vers 8 7 6)
set(_gz_plugin_vers 3 2 1)

set(GZ_SIM_TARGET "")
set(GZ_MATH_TARGET "")
set(GZ_PLUGIN_TARGET "")
set(GZ_PLUGIN_REGISTER_TARGET "")

# Try unversioned packages first (common in some binary installs), then versioned ones.
find_package(gz-sim QUIET)
if(TARGET gz-sim::gz-sim)
  set(GZ_SIM_TARGET gz-sim::gz-sim)
endif()
if(NOT GZ_SIM_TARGET)
  foreach(v IN LISTS _gz_sim_vers)
    find_package(gz-sim${v} QUIET)
    if(TARGET gz-sim${v}::gz-sim${v})
      set(GZ_SIM_TARGET gz-sim${v}::gz-sim${v})
      break()
    endif()
  endforeach()
endif()

find_package(gz-math QUIET)
if(TARGET gz-math::gz-math)
  set(GZ_MATH_TARGET gz-math::gz-math)
endif()
if(NOT GZ_MATH_TARGET)
  foreach(v IN LISTS _gz_math_vers)
    find_package(gz-math${v} QUIET)
    if(TARGET gz-math${v}::gz-math${v})
      set(GZ_MATH_TARGET gz-math${v}::gz-math${v})
      break()
    endif()
  endforeach()
endif()

find_package(gz-plugin QUIET COMPONENTS register)
if(TARGET gz-plugin::gz-plugin)
  set(GZ_PLUGIN_TARGET gz-plugin::gz-plugin)
endif()
if(TARGET gz-plugin::register)
  set(GZ_PLUGIN_REGISTER_TARGET gz-plugin::register)
endif()
if(NOT GZ_PLUGIN_TARGET)
  foreach(v IN LISTS _gz_plugin_vers)
    find_package(gz-plugin${v} QUIET COMPONENTS register)
    if(TARGET gz-plugin${v}::gz-plugin${v})
      set(GZ_PLUGIN_TARGET gz-plugin${v}::gz-plugin${v})
    endif()
    if(TARGET gz-plugin${v}::register)
      set(GZ_PLUGIN_REGISTER_TARGET gz-plugin${v}::register)
    endif()
    if(GZ_PLUGIN_TARGET)
      break()
    endif()
  endforeach()
endif()

if(NOT GZ_SIM_TARGET OR NOT GZ_MATH_TARGET OR NOT GZ_PLUGIN_TARGET)
  if(TELLO_GAZEBO_BUILD_PLUGIN)
    message(FATAL_ERROR
    "Could not find Gazebo Sim development packages for tello_gazebo.\n"
    "Resolved targets:\n"
    "  GZ_SIM_TARGET='${GZ_SIM_TARGET}'\n"
    "  GZ_MATH_TARGET='${GZ_MATH_TARGET}'\n"
    "  GZ_PLUGIN_TARGET='${GZ_PLUGIN_TARGET}'\n"
    "Install ros_gz + gz dev packages in your environment and retry.\n"
    "Typical fix on Ubuntu/ROS Humble:\n"
    "  sudo apt-get update\n"
    "  sudo apt-get install -y ros-humble-ros-gz-sim ros-humble-ros-gz-bridge ros-humble-ros-gz-image\n"
    "  sudo apt-get install -y libgz-sim7-dev libgz-math7-dev libgz-plugin2-dev")
  else()
    message(WARNING
      "Gazebo Sim development packages not found. "
      "Building tello_gazebo without TelloSystem plugin because TELLO_GAZEBO_BUILD_PLUGIN=OFF.")
  endif()
endif()

if(GZ_SIM_TARGET AND GZ_MATH_TARGET AND GZ_PLUGIN_TARGET)
  add_library(TelloSystem SHARED src/tello_system.cpp)
  target_include_directories(TelloSystem PRIVATE include)

  ament_target_dependencies(
    TelloSystem
    geometry_msgs
    rclcpp
    sensor_msgs
    std_msgs
    tello_msgs
  )

  target_link_libraries(
    TelloSystem
    ${GZ_SIM_TARGET}
    ${GZ_MATH_TARGET}
    ${GZ_PLUGIN_TARGET}
  )

  if(GZ_PLUGIN_REGISTER_TARGET)
    target_link_libraries(TelloSystem ${GZ_PLUGIN_REGISTER_TARGET})
  endif()
endif()

if(TARGET TelloSystem)
  install(
    TARGETS TelloSystem
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
  )
endif()

install(
  DIRECTORY models worlds launch
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
