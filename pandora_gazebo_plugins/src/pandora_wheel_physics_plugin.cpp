#include "pandora_gazebo_plugins/pandora_wheel_physics_plugin.h"


/* Implementations */
namespace gazebo
{
  
  void PhysicsReconfigure::Load(physics::ModelPtr _model, sdf::ElementPtr _sdf)
  {
    //Load Parameters 
    this->model_ = _model;
    this->sdf = _sdf;

    //Load SDF Parameters
    if (!LoadParameters())
    {
      ROS_FATAL("Loading Parameters failed :( ");
    }

    //Initiallize NodeHandle for Dynamic Reconfigure Namespace
    ros::NodeHandle n("~physics");

    //Set Up dynamic Reconfgure Server
    reconfig_server = new dynamic_reconfigure::Server<pandora_gazebo_plugins::WheelPhysicsConfig>(n);
    f = boost::bind(&PhysicsReconfigure::reconfigCallback, this, _1, _2);
    reconfig_server->setCallback(f);

  }

  bool PhysicsReconfigure::LoadParameters(){

    uint32_t zero = 0;


    /*############################ LOAD NAMESPACE (from sdf) ############################*/
    this ->robot_namespace_ = "" ; 

    if (!ros::isInitialized())
    {
      ROS_FATAL_STREAM("A ROS node for Gazebo has not been initialized, unable to load plugin. "
                  << "Load the Gazebo system plugin 'libgazebo_ros_api_plugin.so' in the gazebo_ros package)");
     return false;
    }

    //Load Namespace from sdf
    if ( this ->sdf ->HasElement ( "robotNamespace" ) ) 
      this ->robot_namespace_ = this ->sdf ->Get < std ::string > ( "robotNamespace" ) + "/" ; 



    /*######################### LEFT FRONT WHEEL #########################*/
    if ( ! this ->sdf ->HasElement ( "leftFrontWheelLink" ) ) { 

      ROS_FATAL ( "Physics Plugin missing <leftFrontWheelLink>." ) ;   
      return false ; 
    
    }

    else {
      // Load Link
      this ->left_front_wheel_link_ = this ->model_ ->
                                      GetLink ( this ->sdf ->Get < std ::string >( "leftFrontWheelLink" ) ) ; 

      //Load Surface Parameters
      this ->left_front_wheel_params_ = this ->left_front_wheel_link_
                                                                      -> GetCollision(zero)
                                                                                          ->GetSurface(); 
    }
  
    /*######################### LEFT REAR WHEEL #########################*/
    if ( ! this ->sdf ->HasElement ( "leftRearWheelLink" ) ) { 
  
      ROS_FATAL ( "Physics Plugin missing <leftRearWheelLink>." ) ; 
      return false ; 
    
    }
  
    else { 
    
      //Load Link
      this ->left_rear_wheel_link_ = 
      this ->model_ ->GetLink ( this ->sdf ->Get < std ::string > 
                                             ( "leftRearWheelLink" ) ) ;

      //Load Surface Parameters
      this ->left_rear_wheel_params_ = this ->left_rear_wheel_link_
                                                                    -> GetCollision(zero)
                                                                                        ->GetSurface();
    
    }
  
    /*######################### RIGHT FRONT WHEEL #########################*/
    if ( ! this ->sdf ->HasElement ( "rightFrontWheelLink" ) ) { 
  
      ROS_FATAL ( "Physics Plugin missing <rightFrontWheelLink>." ) ; 
      return false ; 
    
    }
      
    else { 
    
      //Load Link
      this ->right_front_wheel_link_ = 
      this ->model_ ->GetLink ( this ->sdf ->Get < std ::string > 
                                             ( "rightFrontWheelLink" ) ) ;

      //Load Surface Parameters
      this ->right_front_wheel_params_ = this ->right_front_wheel_link_
                                                                    -> GetCollision(zero)
                                                                                        ->GetSurface();
    
    }
  
    /*######################### RIGHT REAR WHEEL #########################*/
    if ( ! this ->sdf ->HasElement ( "rightRearWheelLink" ) ) { 
  
      ROS_FATAL ( "Physics Plugin missing <rightRearWheelLink>." ) ;  
      return false ; 
    
    }
  
    else {

      //Load Link
      this ->right_rear_wheel_link_ = 
      this ->model_ ->GetLink ( this ->sdf ->Get < std ::string > 
                                             ( "rightRearWheelLink" ) );

      //Load Surface Parameters
      this ->right_rear_wheel_params_ = this ->right_rear_wheel_link_
                                                                    -> GetCollision(zero)
                                                                                        ->GetSurface();
    }

    return true;
  }

  void PhysicsReconfigure::reconfigCallback(pandora_gazebo_plugins::WheelPhysicsConfig &config, uint32_t level)
  {
    //mu1
    right_rear_wheel_params_->mu1 = config.mu1;
    right_front_wheel_params_->mu1 = config.mu1;
    left_rear_wheel_params_->mu1 = config.mu1;
    left_front_wheel_params_->mu1 = config.mu1;

    //mu2
    right_rear_wheel_params_->mu2 = config.mu2;
    right_front_wheel_params_->mu2 = config.mu2;
    left_rear_wheel_params_->mu2 = config.mu2;
    left_front_wheel_params_->mu2 = config.mu2;

    
    //slip1
    right_rear_wheel_params_->slip1 = config.slip1;
    right_front_wheel_params_->slip1 = config.slip1;
    left_rear_wheel_params_->slip1 = config.slip1;
    left_front_wheel_params_->slip1 = config.slip1;

    //slip2
    right_rear_wheel_params_->slip2 = config.slip2;
    right_front_wheel_params_->slip2 = config.slip2;
    left_rear_wheel_params_->slip2 = config.slip2;
    left_front_wheel_params_->slip2 = config.slip2;

    
    //kp
    right_rear_wheel_params_->kp = config.kp;
    right_front_wheel_params_->kp = config.kp;
    left_rear_wheel_params_->kp = config.kp;
    left_front_wheel_params_->kp = config.kp;

    //kd
    right_rear_wheel_params_->kd = config.kd;
    right_front_wheel_params_->kd = config.kd;
    left_rear_wheel_params_->kd = config.kd;
    left_front_wheel_params_->kd = config.kd;
    
    //minDepth
    right_rear_wheel_params_->minDepth = config.minDepth;
    right_front_wheel_params_->minDepth = config.minDepth;
    left_rear_wheel_params_->minDepth = config.minDepth;
    left_front_wheel_params_->minDepth = config.minDepth;

    //maxVel
    right_rear_wheel_params_->maxVel = config.maxVel;
    right_front_wheel_params_->maxVel = config.maxVel;
    left_rear_wheel_params_->maxVel = config.maxVel;
    left_front_wheel_params_->maxVel = config.maxVel;

    //bounce
    right_rear_wheel_params_->bounce = config.bounce;
    right_front_wheel_params_->bounce = config.bounce;
    left_rear_wheel_params_->bounce = config.bounce;
    left_front_wheel_params_->bounce = config.bounce;

    //bounceThreshold
    right_rear_wheel_params_->bounceThreshold = config.bounceThreshold;
    right_front_wheel_params_->bounceThreshold = config.bounceThreshold;
    left_rear_wheel_params_->bounceThreshold = config.bounceThreshold;
    left_front_wheel_params_->bounceThreshold = config.bounceThreshold;
  }

}