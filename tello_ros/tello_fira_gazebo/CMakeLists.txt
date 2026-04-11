cmake_minimum_required(VERSION 3.5)
project(tello_fira_gazebo)

find_package(ament_cmake REQUIRED)

# Install launch, worlds, and models
install(
  DIRECTORY launch worlds models
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
