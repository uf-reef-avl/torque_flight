/*
 * Copyright (c) 2017 Daniel Koch, James Jackson and Gary Ellingson, BYU MAGICC Lab.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <rosflight_sim/sil_board.h>
#include <fstream>
#include <ros/ros.h>

#include <iostream>

namespace rosflight_sim {

SIL_Board::SIL_Board() :
  rosflight_firmware::UDPBoard()
{ }

void SIL_Board::init_board(void)
{
#if GAZEBO_MAJOR_VERSION >=8
  boot_time_ = world_->SimTime().Double();
#else
  boot_time_ = world_->GetSimTime().Double();
#endif
}

void SIL_Board::gazebo_setup(gazebo::physics::LinkPtr link, gazebo::physics::WorldPtr world,
                             gazebo::physics::ModelPtr model, ros::NodeHandle* nh, std::string mav_type)
{
  link_ = link;
  world_ = world;
  model_ = model;
  nh_ = nh;
  mav_type_ = mav_type;


  std::string bind_host = nh->param<std::string>("gazebo_host", "localhost");
  int bind_port = nh->param<int>("gazebo_port", 14525);
  std::string remote_host = nh->param<std::string>("ROS_host", "localhost");
  int remote_port = nh->param<int>("ROS_port", 14520);

  set_ports(bind_host, bind_port, remote_host, remote_port);

  // Get Sensor Parameters
  gyro_stdev_ = nh->param<double>("gyro_stdev", 0.13);
  gyro_bias_range_ = nh->param<double>("gyro_bias_range", 0.15);
  gyro_bias_walk_stdev_ = nh->param<double>("gyro_bias_walk_stdev", 0.001);

  acc_stdev_ = nh->param<double>("acc_stdev", 1.15);
  acc_bias_range_ = nh->param<double>("acc_bias_range", 0.15);
  acc_bias_walk_stdev_ = nh->param<double>("acc_bias_walk_stdev", 0.001);

  mag_stdev_ = nh->param<double>("mag_stdev", 1.15);
  mag_bias_range_ = nh->param<double>("mag_bias_range", 0.15);
  mag_bias_walk_stdev_ = nh->param<double>("mag_bias_walk_stdev", 0.001);

  baro_stdev_ = nh->param<double>("baro_stdev", 1.15);
  baro_bias_range_ = nh->param<double>("baro_bias_range", 0.15);
  baro_bias_walk_stdev_ = nh->param<double>("baro_bias_walk_stdev", 0.001);

  airspeed_stdev_ = nh_->param<double>("airspeed_stdev", 1.15);
  airspeed_bias_range_ = nh_->param<double>("airspeed_bias_range", 0.15);
  airspeed_bias_walk_stdev_ = nh_->param<double>("airspeed_bias_walk_stdev", 0.001);

  sonar_stdev_ = nh_->param<double>("sonar_stdev", 1.15);
  sonar_min_range_ = nh_->param<double>("sonar_min_range", 0.25);
  sonar_max_range_ = nh_->param<double>("sonar_max_range", 8.0);

  imu_update_rate_ = nh_->param<double>("imu_update_rate", 1000.0);
  imu_update_period_us_ = (uint64_t)(1e6/imu_update_rate_);

  // Calculate Magnetic Field Vector (for mag simulation)
  double inclination = nh_->param<double>("inclination", 1.14316156541);
  double declination = nh_->param<double>("declination", 0.198584539676);
#if GAZEBO_MAJOR_VERSION >= 8
  inertial_magnetic_field_.Z() = sin(-inclination);
  inertial_magnetic_field_.X() = cos(-inclination)*cos(-declination);
  inertial_magnetic_field_.Y() = cos(-inclination)*sin(-declination);

  // Get the desired altitude at the ground (for baro simulation)
  ground_altitude_ = nh->param<double>("ground_altitude", 1387.0);

  // Configure Noise
  random_generator_= std::default_random_engine(std::chrono::system_clock::now().time_since_epoch().count());
  normal_distribution_ = std::normal_distribution<double>(0.0, 1.0);
  uniform_distribution_ = std::uniform_real_distribution<double>(-1.0, 1.0);

  gravity_ = world_->Gravity();

  // Initialize the Sensor Biases
  gyro_bias_.X(gyro_bias_range_*uniform_distribution_(random_generator_));
  gyro_bias_.Y(gyro_bias_range_*uniform_distribution_(random_generator_));
  gyro_bias_.Z(gyro_bias_range_*uniform_distribution_(random_generator_));
  acc_bias_.X(acc_bias_range_*uniform_distribution_(random_generator_));
  acc_bias_.Y(acc_bias_range_*uniform_distribution_(random_generator_));
  acc_bias_.Z(acc_bias_range_*uniform_distribution_(random_generator_));
  mag_bias_.X(mag_bias_range_*uniform_distribution_(random_generator_));
  mag_bias_.Y(mag_bias_range_*uniform_distribution_(random_generator_));
  mag_bias_.Z(mag_bias_range_*uniform_distribution_(random_generator_));
  baro_bias_ = baro_bias_range_*uniform_distribution_(random_generator_);
  airspeed_bias_ = airspeed_bias_range_*uniform_distribution_(random_generator_);

  prev_vel_1_ = link_->RelativeLinearVel();
  prev_vel_2_ = link_->RelativeLinearVel();
  prev_vel_3_ = link_->RelativeLinearVel();
  last_time_ = world_->SimTime();
  next_imu_update_time_us_ = 0;
#else
  inertial_magnetic_field_.z = sin(-inclination);
  inertial_magnetic_field_.x = cos(-inclination)*cos(-declination);
  inertial_magnetic_field_.y = cos(-inclination)*sin(-declination);

  // Get the desired altitude at the ground (for baro simulation)
  ground_altitude_ = nh->param<double>("ground_altitude", 1387.0);

  // Configure Noise
  random_generator_= std::default_random_engine(std::chrono::system_clock::now().time_since_epoch().count());
  normal_distribution_ = std::normal_distribution<double>(0.0, 1.0);
  uniform_distribution_ = std::uniform_real_distribution<double>(-1.0, 1.0);

  gravity_ = world_->GetPhysicsEngine()->GetGravity();

  // Initialize the Sensor Biases
  gyro_bias_.x = gyro_bias_range_*uniform_distribution_(random_generator_);
  gyro_bias_.y = gyro_bias_range_*uniform_distribution_(random_generator_);
  gyro_bias_.z = gyro_bias_range_*uniform_distribution_(random_generator_);
  acc_bias_.x = acc_bias_range_*uniform_distribution_(random_generator_);
  acc_bias_.y = acc_bias_range_*uniform_distribution_(random_generator_);
  acc_bias_.z = acc_bias_range_*uniform_distribution_(random_generator_);
  mag_bias_.x = mag_bias_range_*uniform_distribution_(random_generator_);
  mag_bias_.y = mag_bias_range_*uniform_distribution_(random_generator_);
  mag_bias_.z = mag_bias_range_*uniform_distribution_(random_generator_);
  baro_bias_ = baro_bias_range_*uniform_distribution_(random_generator_);
  airspeed_bias_ = airspeed_bias_range_*uniform_distribution_(random_generator_);

  prev_vel_1_ = link_->GetRelativeLinearVel();
  prev_vel_2_ = link_->GetRelativeLinearVel();
  prev_vel_3_ = link_->GetRelativeLinearVel();
  last_time_ = world_->GetSimTime();
  next_imu_update_time_us_ = 0;
#endif
}

void SIL_Board::board_reset(bool bootloader)
{
}

// clock

uint32_t SIL_Board::clock_millis()
{
#if GAZEBO_MAJOR_VERSION >=8
  return (uint32_t)((world_->SimTime().Double() - boot_time_)*1e3);
#else
  return (uint32_t)((world_->GetSimTime().Double() - boot_time_)*1e3);
#endif
}

uint64_t SIL_Board::clock_micros()
{
#if GAZEBO_MAJOR_VERSION >=8
  return (uint64_t)((world_->SimTime().Double() - boot_time_)*1e6);
#else
  return (uint64_t)((world_->GetSimTime().Double() - boot_time_)*1e6);
#endif
}

void SIL_Board::clock_delay(uint32_t milliseconds)
{
}

// sensors
/// TODO these sensors have noise, no bias
/// noise params are hard coded
void SIL_Board::sensors_init()
{
#if GAZEBO_MAJOR_VERSION >= 8
  // Initialize the Biases
  gyro_bias_.X(gyro_bias_range_*uniform_distribution_(random_generator_));
  gyro_bias_.Y(gyro_bias_range_*uniform_distribution_(random_generator_));
  gyro_bias_.Z(gyro_bias_range_*uniform_distribution_(random_generator_));
  acc_bias_.X(acc_bias_range_*uniform_distribution_(random_generator_));
  acc_bias_.Y(acc_bias_range_*uniform_distribution_(random_generator_));
  acc_bias_.Z(acc_bias_range_*uniform_distribution_(random_generator_));

  // Gazebo coordinates is NWU and Earth's magnetic field is defined in NED, hence the negative signs
  double inclination_ = 1.14316156541;
  double declination_ = 0.198584539676;
  inertial_magnetic_field_.Z(sin(-inclination_));
  inertial_magnetic_field_.X(cos(-inclination_)*cos(-declination_));
  inertial_magnetic_field_.Y(cos(-inclination_)*sin(-declination_));
#else
  // Initialize the Biases
  gyro_bias_.x = gyro_bias_range_*uniform_distribution_(random_generator_);
  gyro_bias_.y = gyro_bias_range_*uniform_distribution_(random_generator_);
  gyro_bias_.z = gyro_bias_range_*uniform_distribution_(random_generator_);
  acc_bias_.x = acc_bias_range_*uniform_distribution_(random_generator_);
  acc_bias_.y = acc_bias_range_*uniform_distribution_(random_generator_);
  acc_bias_.z = acc_bias_range_*uniform_distribution_(random_generator_);

  // Gazebo coordinates is NWU and Earth's magnetic field is defined in NED, hence the negative signs
  double inclination_ = 1.14316156541;
  double declination_ = 0.198584539676;
  inertial_magnetic_field_.z = sin(-inclination_);
  inertial_magnetic_field_.x = cos(-inclination_)*cos(-declination_);
  inertial_magnetic_field_.y = cos(-inclination_)*sin(-declination_);
#endif
}

uint16_t SIL_Board::num_sensor_errors(void)
{
  return 0;
}

bool SIL_Board::new_imu_data()
{
  uint64_t now_us = clock_micros();
  if (now_us >= next_imu_update_time_us_)
  {
    next_imu_update_time_us_ = now_us + imu_update_period_us_;
    return true;
  }
  else
  {
    return false;
  }
}

bool SIL_Board::imu_read(float accel[3], float* temperature, float gyro[3], uint64_t* time_us)
{
#if GAZEBO_MAJOR_VERSION >= 8
  ignition::math::Quaterniond q_I_NWU = link_->WorldPose().Rot();
  ignition::math::Vector3d current_vel = link_->RelativeLinearVel();
  ignition::math::Vector3d y_acc;

  // this is James' egregious hack to overcome wild imu while sitting on the ground
  if (current_vel.Length() < 0.05)
    y_acc = q_I_NWU.RotateVectorReverse(-gravity_);
  else
    y_acc = q_I_NWU.RotateVectorReverse(link_->WorldLinearAccel() - gravity_);

  // Apply normal noise (only if armed, because most of the noise comes from motors
  if (motors_spinning())
  {
    y_acc.X(y_acc.X() + acc_stdev_*normal_distribution_(random_generator_));
    y_acc.Y(y_acc.Y() + acc_stdev_*normal_distribution_(random_generator_));
    y_acc.Z(y_acc.Z() + acc_stdev_*normal_distribution_(random_generator_));
  }

  // Perform Random Walk for biases
  acc_bias_.X(acc_bias_.X() + acc_bias_walk_stdev_*normal_distribution_(random_generator_));
  acc_bias_.Y(acc_bias_.Y() + acc_bias_walk_stdev_*normal_distribution_(random_generator_));
  acc_bias_.Z(acc_bias_.Z() + acc_bias_walk_stdev_*normal_distribution_(random_generator_));

  // Add constant Bias to measurement
  y_acc.X(y_acc.X() + acc_bias_.X());
  y_acc.Y(y_acc.Y() + acc_bias_.Y());
  y_acc.Z(y_acc.Z() + acc_bias_.Z());

  // Convert to NED for output
  accel[0] = y_acc.X();
  accel[1] = -y_acc.Y();
  accel[2] = -y_acc.Z();

  ignition::math::Vector3d y_gyro = link_->RelativeAngularVel();

  // Normal Noise from motors
  if (motors_spinning())
  {
    y_gyro.X(y_gyro.X() + gyro_stdev_*normal_distribution_(random_generator_));
    y_gyro.Y(y_gyro.Y() + gyro_stdev_*normal_distribution_(random_generator_));
    y_gyro.Z(y_gyro.Z() + gyro_stdev_*normal_distribution_(random_generator_));
  }

  // Random Walk for bias
  gyro_bias_.X(gyro_bias_.X() + gyro_bias_walk_stdev_*normal_distribution_(random_generator_));
  gyro_bias_.Y(gyro_bias_.Y() + gyro_bias_walk_stdev_*normal_distribution_(random_generator_));
  gyro_bias_.Z(gyro_bias_.Z() + gyro_bias_walk_stdev_*normal_distribution_(random_generator_));

  // Apply Constant Bias
  y_gyro.X(y_gyro.X() + gyro_bias_.X());
  y_gyro.Y(y_gyro.Y() + gyro_bias_.Y());
  y_gyro.Z(y_gyro.Z() + gyro_bias_.Z());

  // Convert to NED for output
  gyro[0] = y_gyro.X();
  gyro[1] = -y_gyro.Y();
  gyro[2] = -y_gyro.Z();
#else
  gazebo::math::Quaternion q_I_NWU = link_->GetWorldPose().rot;
  gazebo::math::Vector3 current_vel = link_->GetRelativeLinearVel();
  gazebo::math::Vector3 y_acc;

  // this is James' egregious hack to overcome wild imu while sitting on the ground
  if (current_vel.GetLength() < 0.05)
    y_acc = q_I_NWU.RotateVectorReverse(-gravity_);
  else
    y_acc = q_I_NWU.RotateVectorReverse(link_->GetWorldLinearAccel() - gravity_);

  // Apply normal noise (only if armed, because most of the noise comes from motors
  if (motors_spinning())
  {
    y_acc.x += acc_stdev_*normal_distribution_(random_generator_);
    y_acc.y += acc_stdev_*normal_distribution_(random_generator_);
    y_acc.z += acc_stdev_*normal_distribution_(random_generator_);
  }

  // Perform Random Walk for biases
  acc_bias_.x += acc_bias_walk_stdev_*normal_distribution_(random_generator_);
  acc_bias_.y += acc_bias_walk_stdev_*normal_distribution_(random_generator_);
  acc_bias_.z += acc_bias_walk_stdev_*normal_distribution_(random_generator_);

  // Add constant Bias to measurement
  y_acc.x += acc_bias_.x;
  y_acc.y += acc_bias_.y;
  y_acc.z += acc_bias_.z;

  // Convert to NED for output
  accel[0] = y_acc.x;
  accel[1] = -y_acc.y;
  accel[2] = -y_acc.z;

  gazebo::math::Vector3 y_gyro = link_->GetRelativeAngularVel();

  // Normal Noise from motors
  if (motors_spinning())
  {
    y_gyro.x += gyro_stdev_*normal_distribution_(random_generator_);
    y_gyro.y += gyro_stdev_*normal_distribution_(random_generator_);
    y_gyro.z += gyro_stdev_*normal_distribution_(random_generator_);
  }

  // Random Walk for bias
  gyro_bias_.x += gyro_bias_walk_stdev_*normal_distribution_(random_generator_);
  gyro_bias_.y += gyro_bias_walk_stdev_*normal_distribution_(random_generator_);
  gyro_bias_.z += gyro_bias_walk_stdev_*normal_distribution_(random_generator_);

  // Apply Constant Bias
  y_gyro.x += gyro_bias_.x;
  y_gyro.y += gyro_bias_.y;
  y_gyro.z += gyro_bias_.z;

  // Convert to NED for output
  gyro[0] = y_gyro.x;
  gyro[1] = -y_gyro.y;
  gyro[2] = -y_gyro.z;
#endif

  (*temperature) = 27.0;
  (*time_us) = clock_micros();
  return true;
}

void SIL_Board::imu_not_responding_error(void)
{
  ROS_ERROR("[gazebo_rosflight_sil] imu not responding");
}

void SIL_Board::mag_read(float mag[3])
{
#if GAZEBO_MAJOR_VERSION >= 8
  ignition::math::Pose3d I_to_B = link_->WorldPose();
  ignition::math::Vector3d noise;
  noise.X(mag_stdev_*normal_distribution_(random_generator_));
  noise.Y(mag_stdev_*normal_distribution_(random_generator_));
  noise.Z(mag_stdev_*normal_distribution_(random_generator_));

  // Random Walk for bias
  mag_bias_.X(mag_bias_.X() + mag_bias_walk_stdev_*normal_distribution_(random_generator_));
  mag_bias_.Y(mag_bias_.Y() + mag_bias_walk_stdev_*normal_distribution_(random_generator_));
  mag_bias_.Z(mag_bias_.Z() + mag_bias_walk_stdev_*normal_distribution_(random_generator_));

  // combine parts to create a measurement
  ignition::math::Vector3d y_mag = I_to_B.Rot().RotateVectorReverse(inertial_magnetic_field_) + mag_bias_ + noise;

  // Convert measurement to NED
  mag[0] = y_mag.X();
  mag[1] = -y_mag.Y();
  mag[2] = -y_mag.Z();
#else
  gazebo::math::Pose I_to_B = link_->GetWorldPose();
  gazebo::math::Vector3 noise;
  noise.x = mag_stdev_*normal_distribution_(random_generator_);
  noise.y = mag_stdev_*normal_distribution_(random_generator_);
  noise.z = mag_stdev_*normal_distribution_(random_generator_);

  // Random Walk for bias
  mag_bias_.x += mag_bias_walk_stdev_*normal_distribution_(random_generator_);
  mag_bias_.y += mag_bias_walk_stdev_*normal_distribution_(random_generator_);
  mag_bias_.z += mag_bias_walk_stdev_*normal_distribution_(random_generator_);

  // combine parts to create a measurement
  gazebo::math::Vector3 y_mag = I_to_B.rot.RotateVectorReverse(inertial_magnetic_field_) + mag_bias_ + noise;

  // Convert measurement to NED
  mag[0] = y_mag.x;
  mag[1] = -y_mag.y;
  mag[2] = -y_mag.z;;
#endif
}

bool SIL_Board::mag_check(void)
{
  return true;
}

bool SIL_Board::baro_check()
{
  return true;
}

void SIL_Board::baro_read(float *pressure, float *temperature)
{
#if GAZEBO_MAJOR_VERSION >= 8
  // pull z measurement out of Gazebo
  ignition::math::Pose3d current_state_NWU = link_->WorldPose();

  // Invert measurement model for pressure and temperature
  double alt = current_state_NWU.Pos().Z() + ground_altitude_;
#else
  // pull z measurement out of Gazebo
  gazebo::math::Pose current_state_NWU = link_->GetWorldPose();

  // Invert measurement model for pressure and temperature
  double alt = current_state_NWU.pos.z + ground_altitude_;
#endif

  // Convert to the true pressure reading
  double y_baro = 101325.0f*(float)pow((1-2.25694e-5 * alt), 5.2553);

  // Add noise
  y_baro += baro_stdev_*normal_distribution_(random_generator_);

  // Perform random walk
  baro_bias_ += baro_bias_walk_stdev_*normal_distribution_(random_generator_);

  // Add random walk
  y_baro += baro_bias_;

  (*pressure) = (float)y_baro;
  (*temperature) = 27.0f;
}

bool SIL_Board::diff_pressure_check(void)
{
  if(mav_type_ == "fixedwing")
    return true;
  else
    return false;
}

void SIL_Board::diff_pressure_read(float *diff_pressure, float *temperature)
{
  static double rho_ = 1.225;
  // Calculate Airspeed
#if GAZEBO_MAJOR_VERSION >=8
  ignition::math::Vector3d vel = link_->RelativeLinearVel();

  double Va = vel.Length();
#else
  gazebo::math::Vector3 vel = link_->GetRelativeLinearVel();

  double Va = vel.GetLength();
#endif

  // Invert Airpseed to get sensor measurement
  double y_as = rho_*Va*Va/2.0; // Page 130 in the UAV Book

  // Add noise
  y_as += airspeed_stdev_*normal_distribution_(random_generator_);
  airspeed_bias_ += airspeed_bias_walk_stdev_*normal_distribution_(random_generator_);
  y_as += airspeed_bias_;

  *diff_pressure = y_as;
  *temperature = 27.0;
}

bool SIL_Board::sonar_check(void)
{
  return true;
}

float SIL_Board::sonar_read(void)
{
#if GAZEBO_MAJOR_VERSION >= 8
  ignition::math::Pose3d current_state_NWU = link_->WorldPose();
  double alt = current_state_NWU.Pos().Z();
#else
  gazebo::math::Pose current_state_NWU = link_->GetWorldPose();
  double alt = current_state_NWU.pos.z;
#endif

  if (alt < sonar_min_range_)
  {
    return sonar_min_range_;
  }
  else if (alt > sonar_max_range_)
  {
    return sonar_max_range_;
  }
  else
    return alt + sonar_stdev_*normal_distribution_(random_generator_);
}

// PWM
void SIL_Board::pwm_init(bool cppm, uint32_t refresh_rate, uint16_t idle_pwm)
{
  rc_received_ = false;
  latestRC_.values[0] = 1500; // x
  latestRC_.values[1] = 1500; // y
  latestRC_.values[3] = 1500; // z
  latestRC_.values[2] = 1000; // F
  latestRC_.values[4] = 1000; // attitude override
  latestRC_.values[5] = 1000; // arm

  for (size_t i = 0; i < 14; i++)
    pwm_outputs_[i] = 1000;

  rc_sub_ = nh_->subscribe("RC", 1, &SIL_Board::RCCallback, this);
}

uint16_t SIL_Board::pwm_read(uint8_t channel)
{
  if(rc_sub_.getNumPublishers() > 0)
  {
    return latestRC_.values[channel];
  }

  //no publishers, set throttle low and center everything else
  if(channel == 2)
    return 1000;

  return 1500;
}

void SIL_Board::pwm_write(uint8_t channel, uint16_t value)
{
  pwm_outputs_[channel] = value;
}

bool SIL_Board::pwm_lost()
{
  return !rc_received_;
}

// non-volatile memory
void SIL_Board::memory_init(void) {}

bool SIL_Board::memory_read(void * dest, size_t len)
{
  std::string directory = "rosflight_memory" + nh_->getNamespace();
  std::ifstream memory_file;
  memory_file.open(directory + "/mem.bin", std::ios::binary);

  if(!memory_file.is_open())
  {
    ROS_ERROR("Unable to load rosflight memory file %s/mem.bin", directory.c_str());
    return false;
  }

  memory_file.read((char*) dest, len);
  memory_file.close();
  return true;
}

bool SIL_Board::memory_write(const void * src, size_t len)
{
  std::string directory = "rosflight_memory" + nh_->getNamespace();
  std::string mkdir_command = "mkdir -p " + directory;
  const int dir_err = system(mkdir_command.c_str());

  if (dir_err == -1)
  {
    ROS_ERROR("Unable to write rosflight memory file %s/mem.bin", directory.c_str());
    return false;
  }

  std::ofstream memory_file;
  memory_file.open(directory + "/mem.bin", std::ios::binary);
  memory_file.write((char*) src, len);
  memory_file.close();
  return true;
}

bool SIL_Board::motors_spinning()
{
  if(pwm_outputs_[2] > 1100)
      return true;
  else
    return false;
}

// LED

void SIL_Board::led0_on(void) { }
void SIL_Board::led0_off(void) { }
void SIL_Board::led0_toggle(void) { }

void SIL_Board::led1_on(void) { }
void SIL_Board::led1_off(void) { }
void SIL_Board::led1_toggle(void) { }

void SIL_Board::RCCallback(const rosflight_msgs::RCRaw& msg)
{
  rc_received_ = true;
  latestRC_ = msg;
}

} // namespace rosflight_sim
