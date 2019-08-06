# ROSflight

[![Build Status](http://build.ros.org/buildStatus/icon?job=Kdev__rosflight__ubuntu_xenial_amd64)](http://build.ros.org/job/Kdev__rosflight__ubuntu_xenial_amd64)

This repository contains the ROS stack for interfacing with an autopilot running the ROSflight firmware. For more information on the ROSflight autopilot firmware stack, visit http://rosflight.org.

## REEF Edits

`torque_flight` has two extra data streams coming from `rosflight`. The first is `total_torque` which is the output torque commands from the controller on the autopilot. These torques go through a simple transformation matrix to produce raw PWM values. The second stream is `added_torque` which is the "after PID" additive input to the output torques just described.

Note that the default branches are `torque` and `torques` depending on the repo.



The following sections describe each of the packages contained in this stack.

## rosflight_pkgs

This is a metapackage for grouping the other packages into a ROS stack.

## rosflight_msgs

This package contains the ROSflight message and service definitions.

## rosflight

This package contains the `rosflight_io` node, which provides the core functionality for interfacing an onboard computer with the autopilot. This node streams autopilot sensor and status data to the onboard computer, streams control setpoints to the autopilot, and provides an interface for configuring the autopilot.

## rosflight_utils

This package contains additional supporting scripts and libraries that are not part of the core ROSflight package functionality. This package also helps support the [ros_plane](https://github.com/byu-magicc/ros_plane) and [ros_copter](https://github.com/byu-magicc/ros_copter) projects.
