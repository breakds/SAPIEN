#pragma once

#include <gazebo_msgs/GetModelState.h>
#include <ros/ros.h>
#include <ros/timer.h>
#include <rosgraph_msgs/Clock.h>
#include <thread>
#include <utility>

#include "event_system/event_system.h"
#include "robot_loader.h"

namespace sapien {
class SScene;
class SArticulation;
class SActorBase;

namespace ros1 {

class RobotManager;
class RobotLoader;
class SControllableArticulationWrapper;
class RobotDescriptor;
class CameraPublisher;

class SceneManager : public IEventListener<EventStep> {
  friend RobotManager;
  friend RobotLoader;

protected:
  // Main handle
  SScene *mScene;
  std::string mNameSpace;
  ros::NodeHandlePtr mNode;

  // Cache
  std::vector<std::unique_ptr<RobotManager>> mRobotManagers;
  std::vector<std::unique_ptr<SControllableArticulationWrapper>> mArticulationWrappers;

  // Clock and Time Manage
  ros::Publisher mClockPub;
  ros::Time mTime;

  // Thread and spin
  ros::MultiThreadedSpinner mSpinner;
  std::thread mThread;

  // Publishers
  std::vector<std::unique_ptr<CameraPublisher>> mCameraPub;
  std::unique_ptr<ros::ServiceServer> mGetModelService = nullptr;
  std::vector<std::string> mModelNames;
  std::vector<SActorBase *> mModelActors;

protected:
  RobotManager *buildRobotManager(SArticulation *articulation, ros::NodeHandlePtr node,
                                  const std::string &robotName, double frequency);
  void onEvent(EventStep &event) override;

  bool onGetModelService(gazebo_msgs::GetModelStateRequest &req,
                         gazebo_msgs::GetModelStateResponse &res);

public:
  explicit SceneManager(SScene *scene, const std::string &name, uint8_t numThread = 4);
  ~SceneManager() override;

  void startAllCamera(double frequency, float mean = -0.005, float std = 0.008,
                      float cloudCutOff = 0.25, float cloudCutOffMax = 10);
  void startGetModelService(const std::string &serviceName, std::vector<SActorBase *> &actors);

  inline std::unique_ptr<RobotLoader> createRobotLoader() {
    return std::make_unique<RobotLoader>(this);
  };

  /* ROS1 communication spin loop*/
  void start();
  inline void stop() { mThread.detach(); };
};

} // namespace ros1
} // namespace sapien