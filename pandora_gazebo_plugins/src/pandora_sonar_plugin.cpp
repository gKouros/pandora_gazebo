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

#include <pandora_gazebo_plugins/pandora_sonar_plugin.h>

namespace gazebo
{

  // Register this plugin with the simulator
  GZ_REGISTER_SENSOR_PLUGIN(PandoraSonarPlugin)

  // Constructor
  PandoraSonarPlugin::PandoraSonarPlugin()
  {
  }

  // Destructor
  PandoraSonarPlugin::~PandoraSonarPlugin()
  {
    // Finalize the controller / Custom Callback Queue
    this->laser_queue_.clear();
    this->laser_queue_.disable();
    this->rosnode_->shutdown();
    this->callback_laser_queue_thread_.join();

    delete this->rosnode_;
  }

  // Load the controller
  void PandoraSonarPlugin::Load(sensors::SensorPtr _parent, sdf::ElementPtr _sdf)
  {
    // load plugin
    RayPlugin::Load(_parent, _sdf);

    // Get then name of the parent sensor
    this->parent_sensor_ = _parent;

    // Get the world name.
    std::string worldName = _parent->GetWorldName();
    this->world_ = physics::get_world(worldName);

    last_update_time_ = this->world_->GetSimTime();

    this->node_ = transport::NodePtr(new transport::Node());
    this->node_->Init(worldName);

    this->parent_ray_sensor_ = boost::dynamic_pointer_cast<sensors::RaySensor>(this->parent_sensor_);

    if (!this->parent_ray_sensor_)
    {
      gzthrow("PandoraSonarPlugin controller requires a Ray Sensor as its parent");
    }

    this->robot_namespace_ = "";
    if (_sdf->HasElement("robotNamespace"))
    {
      this->robot_namespace_ = _sdf->GetElement("robotNamespace")->Get<std::string>() + "/";
    }

    if (!_sdf->HasElement("frameName"))
    {
      ROS_INFO("Block laser plugin missing <frameName>, defaults to /world");
      this->frame_name_ = "/world";
    }
    else
    {
      this->frame_name_ = _sdf->GetElement("frameName")->Get<std::string>();
      if (this->parent_sensor_->GetScopedName().find("left") != std::string::npos)
      {
        this->frame_name_ = std::string("/left") + std::string("_") + this->frame_name_;
      }
      else
      {
        this->frame_name_ = std::string("/right") + std::string("_") + this->frame_name_;
      }
    }

    if (!_sdf->HasElement("topicName"))
    {
      ROS_INFO("Block laser plugin missing <topicName>, defaults to /world");
      this->topic_name_ = "/world";
    }
    else
    {
      this->topic_name_ = _sdf->GetElement("topicName")->Get<std::string>();

      if (this->parent_sensor_->GetScopedName().find("left") != std::string::npos)
      {
        if (this->parent_sensor_->GetScopedName().find("rear") != std::string::npos)
        {
          this->topic_name_ = this->topic_name_ + std::string("/rear_left");
        }
        else
        {
          this->topic_name_ = this->topic_name_ + std::string("/front_left");
        }
      }
      else
      {
        if (this->parent_sensor_->GetScopedName().find("rear") != std::string::npos)
        {
          this->topic_name_ = this->topic_name_ + std::string("/rear_right");
        }
        else
        {
          this->topic_name_ = this->topic_name_ + std::string("/front_right");
        }
      }
    }
    
    if (!_sdf->HasElement("gaussianNoise"))
    {
      ROS_INFO("Block laser plugin missing <gaussianNoise>, defaults to 0.0");
      this->gaussian_noise_ = 0;
    }
    else
    {
      this->gaussian_noise_ = _sdf->GetElement("gaussianNoise")->Get<double>();
    }

    this->laser_connect_count_ = 0;

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
    this->cloud_msg_.points.clear();
    this->cloud_msg_.channels.clear();
    this->cloud_msg_.channels.push_back(sensor_msgs::ChannelFloat32());

    if (this->topic_name_ != "")
    {
      // Custom Callback Queue
      ros::AdvertiseOptions ao = ros::AdvertiseOptions::create<sensor_msgs::Range>(
                                   this->topic_name_, 1,
                                   boost::bind(&PandoraSonarPlugin::LaserConnect, this),
                                   boost::bind(&PandoraSonarPlugin::LaserDisconnect, this),
                                   ros::VoidPtr(), &this->laser_queue_);
      this->pub_ = this->rosnode_->advertise(ao);
    }

    // sensor generation off by default
    this->parent_ray_sensor_->SetActive(false);

    // start custom queue for laser
    this->callback_laser_queue_thread_ = boost::thread(boost::bind(&PandoraSonarPlugin::LaserQueueThread, this));
  }

  // Increment count
  void PandoraSonarPlugin::LaserConnect()
  {
    this->laser_connect_count_++;
    this->parent_ray_sensor_->SetActive(true);
  }

  // Decrement count
  void PandoraSonarPlugin::LaserDisconnect()
  {
    this->laser_connect_count_--;

    if (this->laser_connect_count_ == 0)
    {
      this->parent_ray_sensor_->SetActive(false);
    }
  }

  // Update the controller
  void PandoraSonarPlugin::OnNewLaserScans()
  {
    if (this->topic_name_ != "")
    {
      common::Time sensor_update_time = this->parent_sensor_->GetLastUpdateTime();
      if (last_update_time_ < sensor_update_time)
      {
        this->PutLaserData(sensor_update_time);
        last_update_time_ = sensor_update_time;
      }
    }
    else
    {
      ROS_INFO("gazebo_ros_block_laser topic name not set");
    }
  }

  // Put laser data to the interface
  void PandoraSonarPlugin::PutLaserData(common::Time& _updateTime)
  {
    int i, hja, hjb;
    int j, vja, vjb;
    double vb, hb;
    int    j1, j2, j3, j4;  // four corners indices
    double r1, r2, r3, r4, r;  // four corner values + interpolated range
    double intensity;

    this->parent_ray_sensor_->SetActive(false);

    math::Angle maxAngle = this->parent_ray_sensor_->GetAngleMax();
    math::Angle minAngle = this->parent_ray_sensor_->GetAngleMin();

    double maxRange = this->parent_ray_sensor_->GetRangeMax();
    double minRange = this->parent_ray_sensor_->GetRangeMin();
    int rayCount = this->parent_ray_sensor_->GetRayCount();
    int rangeCount = this->parent_ray_sensor_->GetRangeCount();

    int verticalRayCount = this->parent_ray_sensor_->GetVerticalRayCount();
    int verticalRangeCount = this->parent_ray_sensor_->GetVerticalRangeCount();
    math::Angle verticalMaxAngle = this->parent_ray_sensor_->GetVerticalAngleMax();
    math::Angle verticalMinAngle = this->parent_ray_sensor_->GetVerticalAngleMin();

    double yDiff = maxAngle.Radian() - minAngle.Radian();
    double pDiff = verticalMaxAngle.Radian() - verticalMinAngle.Radian();

    // set size of cloud message everytime!
    // int r_size = rangeCount * verticalRangeCount;
    this->cloud_msg_.points.clear();
    this->cloud_msg_.channels.clear();
    this->cloud_msg_.channels.push_back(sensor_msgs::ChannelFloat32());

    // point scan from laser
    boost::mutex::scoped_lock sclock(this->lock);
    // Add Frame Name
    this->sonar_msg_.header.frame_id = this->frame_name_;
    this->sonar_msg_.header.stamp.sec = _updateTime.sec;
    this->sonar_msg_.header.stamp.nsec = _updateTime.nsec;
    this->sonar_msg_.min_range = minRange;
    this->sonar_msg_.max_range = maxRange;
    this->sonar_msg_.field_of_view = yDiff;
    this->sonar_msg_.radiation_type = sensor_msgs::Range::ULTRASOUND;

    this->sonar_msg_.range = maxRange;

    for (j = 0; j < verticalRangeCount; j++)
    {
      // interpolating in vertical direction
      vb = (verticalRangeCount == 1) ? 0 : static_cast<double>(j) * (verticalRayCount - 1) / (verticalRangeCount - 1);
      vja = static_cast<int>(floor(vb));
      vjb = std::min(vja + 1, verticalRayCount - 1);
      vb = vb - floor(vb);  // fraction from min

      assert(vja >= 0 && vja < verticalRayCount);
      assert(vjb >= 0 && vjb < verticalRayCount);

      for (i = 0; i < rangeCount; i++)
      {
        // Interpolate the range readings from the rays in horizontal direction
        hb = (rangeCount == 1) ? 0 : static_cast<double>(i) * (rayCount - 1) / (rangeCount - 1);
        hja = static_cast<int>(floor(hb));
        hjb = std::min(hja + 1, rayCount - 1);
        hb = hb - floor(hb);  // fraction from min

        assert(hja >= 0 && hja < rayCount);
        assert(hjb >= 0 && hjb < rayCount);

        // indices of 4 corners
        j1 = hja + vja * rayCount;
        j2 = hjb + vja * rayCount;
        j3 = hja + vjb * rayCount;
        j4 = hjb + vjb * rayCount;
        // range readings of 4 corners
        r1 = std::min(this->parent_ray_sensor_->GetLaserShape()->GetRange(j1), maxRange - minRange);
        r2 = std::min(this->parent_ray_sensor_->GetLaserShape()->GetRange(j2), maxRange - minRange);
        r3 = std::min(this->parent_ray_sensor_->GetLaserShape()->GetRange(j3), maxRange - minRange);
        r4 = std::min(this->parent_ray_sensor_->GetLaserShape()->GetRange(j4), maxRange - minRange);

        // Range is linear interpolation if values are close,
        // and min if they are very different
        r = (1 - vb) * ((1 - hb) * r1 + hb * r2)
            +   vb * ((1 - hb) * r3 + hb * r4);

        // Intensity is averaged
        intensity = 0.25 * (this->parent_ray_sensor_->GetLaserShape()->GetRetro(j1) +
                            this->parent_ray_sensor_->GetLaserShape()->GetRetro(j2) +
                            this->parent_ray_sensor_->GetLaserShape()->GetRetro(j3) +
                            this->parent_ray_sensor_->GetLaserShape()->GetRetro(j4));


        // get angles of ray to get xyz for point
        double yAngle = 0.5 * (hja + hjb) * yDiff / (rayCount - 1) + minAngle.Radian();
        double pAngle = 0.5 * (vja + vjb) * pDiff / (verticalRayCount - 1) + verticalMinAngle.Radian();

        // point scan from laser
        if (r == maxRange - minRange)
        {
          // no noise if at max range
          geometry_msgs::Point32 point;
          point.x = (r + minRange) * cos(pAngle) * cos(yAngle);
          point.y = -(r + minRange) * sin(yAngle);
          point.z = (r + minRange) * sin(pAngle) * cos(yAngle);

          // pAngle is rotated by yAngle:
          point.x = (r + minRange) * cos(pAngle) * cos(yAngle);
          point.y = -(r + minRange) * cos(pAngle) * sin(yAngle);
          point.z = (r + minRange) * sin(pAngle);

          this->cloud_msg_.points.push_back(point);
        }
        else
        {
          geometry_msgs::Point32 point;
          point.x = (r + minRange) * cos(pAngle) * cos(yAngle) + this->GaussianKernel(0, this->gaussian_noise_);
          point.y = -(r + minRange) * sin(yAngle) + this->GaussianKernel(0, this->gaussian_noise_);
          point.z = (r + minRange) * sin(pAngle) * cos(yAngle) + this->GaussianKernel(0, this->gaussian_noise_);
          // pAngle is rotated by yAngle:
          point.x = (r + minRange) * cos(pAngle) * cos(yAngle) + this->GaussianKernel(0, this->gaussian_noise_);
          point.y = -(r + minRange) * cos(pAngle) * sin(yAngle) + this->GaussianKernel(0, this->gaussian_noise_);
          point.z = (r + minRange) * sin(pAngle) + this->GaussianKernel(0, this->gaussian_noise_);
          this->cloud_msg_.points.push_back(point);
          if (point.x < sonar_msg_.range)
          {
            sonar_msg_.range = point.x;
          }
        }  // only 1 channel

        this->cloud_msg_.channels[0].values.push_back(intensity + this->GaussianKernel(0, this->gaussian_noise_));
      }
    }

    this->parent_ray_sensor_->SetActive(true);

    // send data out via ros message
    this->pub_.publish(this->sonar_msg_);
  }

  // Utility for adding noise
  double PandoraSonarPlugin::GaussianKernel(double mu, double sigma)
  {
    unsigned int seed;
    // using Box-Muller transform to generate two independent standard normally disbributed normal variables
    // normalized uniform random variable
    double U = static_cast<double>(rand_r(&seed)) / static_cast<double>(RAND_MAX);
    // normalized uniform random variable
    double V = static_cast<double>(rand_r(&seed)) / static_cast<double>(RAND_MAX);
    double X = sqrt(-2.0 *::log(U)) * cos(2.0 * M_PI * V);
    // double Y = sqrt(-2.0 *::log(U)) * sin( 2.0*M_PI * V); // the other indep. normal variable
    // we'll just use X
    // scale to our mu and sigma
    X = sigma * X + mu;
    return X;
  }

  // custom callback queue thread
  void PandoraSonarPlugin::LaserQueueThread()
  {
    static const double timeout = 0.01;

    while (this->rosnode_->ok())
    {
      this->laser_queue_.callAvailable(ros::WallDuration(timeout));
    }
  }

  void PandoraSonarPlugin::OnStats(const boost::shared_ptr<msgs::WorldStatistics const>& _msg)
  {
    this->sim_time_  = msgs::Convert(_msg->sim_time());

    math::Pose pose;
    pose.pos.x = 0.5 * sin(0.01 * this->sim_time_.Double());
    gzdbg << "plugin simTime [" << this->sim_time_.Double() << "] update pose [" << pose.pos.x << "]\n";
  }
}  // namespace gazebo
