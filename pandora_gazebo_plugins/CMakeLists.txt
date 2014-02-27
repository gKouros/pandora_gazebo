cmake_minimum_required(VERSION 2.8.3)
project(pandora_gazebo_plugins)

## Find catkin macros and libraries
## if COMPONENTS list like find_package(catkin REQUIRED COMPONENTS xyz)
## is used, also find other catkin packages
find_package(catkin REQUIRED 
      roscpp
      tf
      std_msgs
      geometry_msgs
      sensor_msgs
      gazebo_ros
      gazebo_plugins
      controllers_and_sensors_communications
)

include_directories(include
  ${catkin_INCLUDE_DIRS}
)

find_package(gazebo REQUIRED)
find_package(Boost REQUIRED COMPONENTS thread)

include_directories(include ${Boost_INCLUDE_DIRS} ${catkin_INCLUDE_DIRS} ${GAZEBO_INCLUDE_DIRS})

link_directories(${GAZEBO_LIBRARY_DIRS} ${catkin_LIBRARY_DIRS})
include_directories(${Boost_INCLUDE_DIR} ${catkin_INCLUDE_DIRS} ${GAZEBO_INCLUDE_DIRS} )

catkin_package(
  INCLUDE_DIRS include
  CATKIN_DEPENDS
    roscpp
    tf
    std_msgs
    geometry_msgs
    sensor_msgs
    gazebo_ros
    gazebo_plugins
    controllers_and_sensors_communications
#  DEPENDS system_lib
  LIBRARIES
    motion_plugin
    pandora_sonar_plugin
    pandora_imu_stabilizer_plugin
    pandora_thermal_plugin
    pandora_co2_plugin
    pandora_microphone_plugin
)

add_library(motion_plugin src/motion_plugin.cpp  )
target_link_libraries(motion_plugin 
      ${catkin_LIBRARIES}
      ${GAZEBO_LIBRARIES}	
)

add_library(pandora_sonar_plugin src/pandora_sonar_plugin.cpp)
target_link_libraries(pandora_sonar_plugin 
    gazebo_ros_camera_utils 
    RayPlugin 
    ${GAZEBO_LIBRARIES} 
    ${catkin_LIBRARIES}
)

add_library(pandora_imu_stabilizer_plugin src/pandora_imu_stabilizer_plugin.cpp)
target_link_libraries(pandora_imu_stabilizer_plugin 
    ${GAZEBO_LIBRARIES} 
    ${SDFormat_LIBRARIES} 
    ${catkin_LIBRARIES} 
    ${Boost_LIBRARIES}
)

add_library(pandora_thermal_plugin src/pandora_thermal_plugin.cpp)
add_dependencies(pandora_thermal_plugin controllers_and_sensors_communications_gencpp)
target_link_libraries(pandora_thermal_plugin
    CameraPlugin
    ${GAZEBO_LIBRARIES}
    ${catkin_LIBRARIES}
)

add_library(pandora_co2_plugin src/pandora_co2_plugin.cpp)
add_dependencies(pandora_co2_plugin controllers_and_sensors_communications_gencpp)
target_link_libraries(pandora_co2_plugin
    CameraPlugin
    ${GAZEBO_LIBRARIES}
    ${catkin_LIBRARIES}
)

add_library(pandora_microphone_plugin src/pandora_microphone_plugin.cpp)
add_dependencies(pandora_microphone_plugin controllers_and_sensors_communications_gencpp)
target_link_libraries(pandora_microphone_plugin
    CameraPlugin
    ${GAZEBO_LIBRARIES}
    ${catkin_LIBRARIES}
)


add_executable(twist_publisher src/twist_publisher.cpp  )
target_link_libraries(twist_publisher ${catkin_LIBRARIES})

add_executable(pandora_teleop_key src/pandora_teleop_key.cpp )
target_link_libraries(pandora_teleop_key ${catkin_LIBRARIES})