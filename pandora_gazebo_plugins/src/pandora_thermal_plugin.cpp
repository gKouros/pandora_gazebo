/*********************************************************************
 *
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2015, P.A.N.D.O.R.A. Team.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the P.A.N.D.O.R.A. Team nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Geromichalos Dimitrios Panagiotis <geromidg@gmail.com>
 *********************************************************************/

#include "pandora_gazebo_plugins/pandora_thermal_plugin.h"

namespace gazebo
{

  // Load the controller
  void PandoraThermalPlugin::Load(sensors::SensorPtr _parent, sdf::ElementPtr _sdf)
  {
    // load plugin
    CameraPlugin::Load(_parent, _sdf);

    // Get then name of the parent sensor
    this->parent_sensor_ = _parent;

    // Get the world name.
    std::string worldName = _parent->GetWorldName();
    this->world_ = physics::get_world(worldName);

    last_update_time_ = this->world_->GetSimTime();

    this->node_ = transport::NodePtr(new transport::Node());
    this->node_->Init(worldName);

    this->parent_camera_sensor_ = boost::dynamic_pointer_cast<sensors::CameraSensor>(this->parent_sensor_);

    if (!this->parent_camera_sensor_)
    {
      gzthrow("PandoraThermalPlugin controller requires a Camera Sensor as its parent");
    }

    this->robot_namespace_ = "";
    if (_sdf->HasElement("robotNamespace"))
    {
      this->robot_namespace_ = _sdf->GetElement("robotNamespace")->Get<std::string>() + "/";
    }

    if (!_sdf->HasElement("frameName"))
    {
      ROS_INFO("Thermal plugin missing <frameName>, defaults to /world");
      this->frame_name_ = "/world";
    }
    else
    {
      this->frame_name_ = _sdf->GetElement("frameName")->Get<std::string>();
    }

    if (!_sdf->HasElement("topicName"))
    {
      ROS_INFO("Thermal plugin missing <topicName>, defaults to /world");
      this->topic_name_ = "/world";
    }
    else
    {
      this->topic_name_ = _sdf->GetElement("topicName")->Get<std::string>();
    }

    this->camera_connect_count_ = 0;

    // Make sure the ROS node for Gazebo has already been initialized
    if (!ros::isInitialized())
    {
      ROS_FATAL_STREAM("A ROS node for Gazebo has not been initialized, unable to load plugin. "
                       << "Load the Gazebo system plugin 'libgazebo_ros_api_plugin.so' in the gazebo_ros package)");
      return;
    }

    this->rosnode_ = new ros::NodeHandle(this->robot_namespace_);

    // resolve tf prefix
    std::string prefix;
    this->rosnode_->getParam(std::string("tf_prefix"), prefix);
    this->frame_name_ = tf::resolve(prefix, this->frame_name_);

    // set size of cloud message, starts at 0!! FIXME: not necessary
    // this->cloud_msg_.points.clear();
    // this->cloud_msg_.channels.clear();
    // this->cloud_msg_.channels.push_back(sensor_msgs::ChannelFloat32());

    if (this->topic_name_ != "")
    {
      // Custom Callback Queue
      ros::AdvertiseOptions ao = ros::AdvertiseOptions::create<sensor_msgs::Image>(
                                   this->topic_name_, 1,
                                   boost::bind(&PandoraThermalPlugin::CameraConnect, this),
                                   boost::bind(&PandoraThermalPlugin::CameraDisconnect, this),
                                   ros::VoidPtr(), &this->camera_queue_);
      this->pub_ = this->rosnode_->advertise(ao);
    }

    // sensor generation off by default
    this->parent_camera_sensor_->SetActive(false);

    // start custom queue for laser
    this->callback_camera_queue_thread_ = boost::thread(boost::bind(&PandoraThermalPlugin::CameraQueueThread, this));
  }

  // Increment count
  void PandoraThermalPlugin::CameraConnect()
  {
    this->camera_connect_count_++;
    this->parent_camera_sensor_->SetActive(true);
  }

  // Decrement count
  void PandoraThermalPlugin::CameraDisconnect()
  {
    this->camera_connect_count_--;

    if (this->camera_connect_count_ == 0)
    {
      this->parent_camera_sensor_->SetActive(false);
    }
  }

  void PandoraThermalPlugin::CameraQueueThread()
  {
    static const double timeout = 0.01;

    while (this->rosnode_->ok())
    {
      this->camera_queue_.callAvailable(ros::WallDuration(timeout));
    }
  }

  void PandoraThermalPlugin::OnNewFrame(const unsigned char* _image,
                                        unsigned int _width, unsigned int _height,
                                        unsigned int _depth, const std::string& _format)
  {
    if (this->topic_name_ != "")
    {
      common::Time sensor_update_time = this->parent_sensor_->GetLastUpdateTime();
      if (last_update_time_ < sensor_update_time)
      {
        this->PutThermalData(sensor_update_time);
        last_update_time_ = sensor_update_time;
      }
    }
    else
    {
      ROS_INFO("gazebo_ros_thermal topic name not set");
    }
  }

  void PandoraThermalPlugin::PutThermalData(common::Time& _updateTime)
  {
    imageMsg_.header.stamp = ros::Time::now();
    imageMsg_.header.frame_id = this->frame_name_;

    imageMsg_.height = 8;
    imageMsg_.width = 8;
    imageMsg_.step = 8;
    imageMsg_.encoding = sensor_msgs::image_encodings::MONO8;

    imageMsg_.data.clear();

    unsigned int cameraWidth = this->parent_camera_sensor_
                              ->GetImageWidth();

    unsigned int sensorWidth = imageMsg_.width;

    unsigned int divWidth = (cameraWidth / sensorWidth);

    unsigned int cameraHeight = this->parent_camera_sensor_
                               ->GetImageHeight();

    unsigned int sensorHeight = imageMsg_.height;

    unsigned int divHeight = (cameraHeight / sensorHeight);

    const unsigned char* data = this->parent_camera_sensor_
                                ->GetImageData();

    if (data == NULL)
    {
      return;
    }

    double ambientTemp = 25.0;

    double maxTemp = 17.0;

    for (unsigned int i = 0; i < sensorWidth; i++)
    {
      for (unsigned int j = 0; j < sensorHeight; j++)
      {
        double meanTemp = 0;

        for (unsigned int k = 0; k < divWidth; k++)
        {
          for (unsigned int l = 0; l < divHeight; l++)
          {
            double currentTemp = 0;

            double R = data [((k + i * divWidth) * cameraHeight +
                              (l + j * divHeight)) * 3 + 0 ];
            double G = data [((k + i * divWidth) * cameraHeight +
                              (l + j * divHeight)) * 3 + 1 ];
            double B = data [((k + i * divWidth) * cameraHeight +
                              (l + j * divHeight)) * 3 + 2 ];

            // temperature is represented by red
            double R1 = (R - G);
            double R2 = (R - B);

            double positiveDiff = 0;

            if (R1 > 0)
            {
              currentTemp += pow(R1, 2);

              ++positiveDiff;
            }

            if (R2 > 0)
            {
              currentTemp += pow(R2, 2);

              ++positiveDiff;
            }

            currentTemp = sqrt(currentTemp);

            if (positiveDiff == 1)
            {
              currentTemp /= 255.0;
            }
            else if (positiveDiff == 2)
            {
              currentTemp /= sqrt(pow(255.0, 2)
                                  + pow(255.0, 2));
            }

            meanTemp += currentTemp;
          }
        }

        meanTemp /= (divWidth * divHeight);

        double temp = ambientTemp + meanTemp * maxTemp;
        imageMsg_.data.push_back(static_cast<char>(temp));
      }
    }

    this->pub_.publish(imageMsg_);

    usleep(100000);
  }
}  // namespace gazebo
