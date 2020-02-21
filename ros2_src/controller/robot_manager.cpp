#include "robot_manager.h"
#include "articulation/sapien_articulation.h"
#include "articulation/sapien_joint.h"
#include "sapien_controllable_articulation.h"
#include "scene_manager.h"

namespace sapien::ros2 {

RobotManager::RobotManager(SControllableArticulationWrapper *wrapper, const std::string &nameSpace,
                           const std::string &robotName, rclcpp::Clock::SharedPtr clock)
    : mWrapper(wrapper), mClock(std::move(clock)), mNameSpace(nameSpace + "/" + robotName) {

  mNode = rclcpp::Node::make_shared(robotName, nameSpace);

  // Create Initial Joint States
  mJointStates = std::make_unique<sensor_msgs::msg::JointState>();
  auto jointName = wrapper->getDriveJointNames();
  mJointNum = jointName.size();
  mJointStates->name = jointName;
  mJointStates->position.resize(jointName.size());
  mJointStates->velocity.resize(jointName.size());

  // Load robot description from remote node, do not use node to access parameters
  // SAPIEN convention: the remote node name must be "$/{robotName}_config"
  // Note that you must add a "/" before the name of the node, otherwise it do not exist
  const std::string robotURDFName = "robot_description";
  const std::string robotSRDFName = "robot_description_semantic";
  const std::string robotConfigNodeName = "/" + robotName + "_config";

  rclcpp::SyncParametersClient paramClient(mNode.get(), robotConfigNodeName);
  while (!paramClient.wait_for_service(1s)) {
    if (!rclcpp::ok()) {
      RCLCPP_ERROR(mNode->get_logger(), "Interrupted while waiting for the service. Exiting.");
      rclcpp::shutdown();
    }
    RCLCPP_INFO(mNode->get_logger(), "service not available, waiting again...");
  }
  assert(paramClient.wait_for_service(20s));

  if (paramClient.has_parameter(robotURDFName) && paramClient.has_parameter(robotSRDFName)) {
    // Load robot model
    // With current convention, a specific robot will have one kinematics parameters
    // No matter how many time it is instantiated
    const auto URDFString = paramClient.get_parameter<std::string>(robotURDFName);
    const auto SRDFString = paramClient.get_parameter<std::string>(robotSRDFName);
    robot_model_loader::RobotModelLoader::Options opt(URDFString, SRDFString);
    mRobotLoader = std::make_unique<robot_model_loader::RobotModelLoader>(mNode, opt);

    // Load robot and robot state
    mRobotModel = mRobotLoader->getModel();
    RCLCPP_INFO(mNode->get_logger(), "Load ROS robot model %s, base frame: %s",
                robotURDFName.c_str(), mRobotModel->getModelFrame().c_str());
    mRobotState = std::make_unique<robot_state::RobotState>(mRobotModel);

    // Build up index for transformation from simulation to robot state
    auto variableNames = mRobotState->getVariableNames();
    if (variableNames.size() != mJointNum) {
      RCLCPP_ERROR(mNode->get_logger(), "Robot State has different dof from robot articulation");
      exit(0);
    }
    for (auto &variableName : variableNames) {
      auto iter = std::find(jointName.begin(), jointName.end(), variableName);
      if (iter == jointName.end()) {
        RCLCPP_ERROR(mNode->get_logger(),
                     "Robot State variable name %s not found in articulation joint names",
                     variableName.c_str());
        exit(0);
      }
      uint32_t index = iter - jointName.begin();
      mJointIndex.push_back(index);
    }
    mLoadRobot = true;
  } else {
    RCLCPP_WARN(mNode->get_logger(),
                "No parameter %s found, inverse kinematics and motion planning features will "
                "be disabled!",
                robotURDFName.c_str());
    mLoadRobot = false;
  }
}

void RobotManager::setDriveProperty(float stiffness, float damping, float forceLimit,
                                    const std::vector<uint32_t> &jointIndex) {
  std::vector<uint32_t> index(jointIndex);
  if (jointIndex.empty()) {
    index.resize(mWrapper->mJoints.size());
    std::iota(std::begin(index), std::end(index), 0);
  }

  for (unsigned int i : index) {
    assert(i < mWrapper->mArticulation->dof());
    auto joint = mWrapper->mJoints[i];
    joint->setDriveProperty(stiffness, damping, forceLimit);
  }
}

void RobotManager::balancePassiveForce(bool gravity, bool coriolisAndCentrifugal, bool external) {
  auto balanceForce =
      mWrapper->mArticulation->computePassiveForce(gravity, coriolisAndCentrifugal, external);
  mWrapper->mArticulation->setQf(balanceForce);
}
void RobotManager::updateStates(const std::vector<float> &jointPosition,
                                const std::vector<float> &jointVelocity) {
  // Update joint state message
  mJointStates->position.assign(jointPosition.begin(), jointPosition.begin() + mJointNum);
  mJointStates->velocity.assign(jointVelocity.begin(), jointVelocity.begin() + mJointNum);
  mJointStates->header.stamp = mClock->now();

  // Update robot state
  std::vector<double> robotStateVariable(mJointNum);
  for (size_t i = 0; i < mJointNum; ++i) {
    robotStateVariable[i] = double(jointPosition[mJointIndex[i]]);
  }
  mRobotState->setVariablePositions(robotStateVariable);
}
void RobotManager::step(bool timeStepChange, float newTimeStep) {
  updateStates(mWrapper->mJointPositions.read(), mWrapper->mJointVelocities.read());
  for (auto &mCartesianVelocityController : mCartesianVelocityControllers) {
    if (timeStepChange) {
      mCartesianVelocityController->mTimeStep = double(newTimeStep);
    }
    if (mCartesianVelocityController->mCurrentStepCalled) {
      mCartesianVelocityController->mLastStepCalled = true;
      mCartesianVelocityController->mCurrentStepCalled = false;
    } else if (!mCartesianVelocityController->mCurrentStepCalled &&
               mCartesianVelocityController->mLastStepCalled) {
      mCartesianVelocityController->mLastStepCalled = false;
    }
  }
}

// Create controller and publisher
void RobotManager::createJointPublisher(double pubFrequency) {
  if (mJointPublisher) {
    RCLCPP_WARN(mNode->get_logger(),
                "Joint Pub Node has already been created for this Robot Manager");
    RCLCPP_WARN(mNode->get_logger(), "Robot Manager will use the original joint state pub node");
    return;
  }

  mJointPublisher = std::make_unique<JointPublisher>(mNameSpace, mNode->shared_from_this(), mClock,
                                                     mJointStates.get(), pubFrequency);
}

std::weak_ptr<JointVelocityController>
RobotManager::buildJointVelocityController(const std::vector<std::string> &jointNames,
                                           const std::string &serviceName) {
  auto controller = std::make_shared<JointVelocityController>(mNameSpace, mNode, mClock, mWrapper,
                                                              jointNames, serviceName);
  mJointVelocityControllers.push_back(controller);
  return std::weak_ptr<JointVelocityController>(controller);
}

std::weak_ptr<CartesianVelocityController>
RobotManager::buildCartesianVelocityController(const std::string &groupName,
                                               const std::string &serviceName) {
  if (!mLoadRobot) {
    RCLCPP_ERROR(mNode->get_logger(),
                 "No robot load from parameter server, fail to build cartesian controller!");
    assert(mLoadRobot);
  }
  auto controller = std::make_shared<CartesianVelocityController>(
      mNameSpace, mNode, mClock, mWrapper, mRobotState.get(), groupName, serviceName);
  controller->mTimeStep = mSceneManager->mTimeStep;
  mCartesianVelocityControllers.push_back(controller);
  return std::weak_ptr<CartesianVelocityController>(controller);
}
} // namespace sapien::ros2