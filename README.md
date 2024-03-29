# ROSflight

[![Build Status](https://travis-ci.org/rosflight/rosflight.svg?branch=master)](https://travis-ci.org/rosflight/rosflight)

This repo contains 3 extra topics. PID Torques is the output of the PID controllers added to the normal trim in terms of XYZ torques. Total Torque is the torques of th body being commanded to the mix of motors. Finally, there is a sub on the rosflight side called Added Torques, which get added to the torques, before Total Total Torques is calculated, but after PID Torques is calculated allowing for custom controllers as external ros nodes directly commanding delta torques to the vehicle.

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

This package contains additional supporting scripts and libraries that are not part of the core ROSflight package functionality, including visualization tools for the attitude estimate and magnetometer. This package also helps support the [ROSplane](https://github.com/byu-magicc/rosplane) and [ROScopter](https://github.com/byu-magicc/roscopter) projects.
