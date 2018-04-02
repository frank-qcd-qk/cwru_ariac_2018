//
//  wsn 3/2018
//

#include <kuka_move_as/RobotBehaviorInterface.h>
RobotBehaviorInterface::RobotBehaviorInterface(ros::NodeHandle* nodehandle) : nh_(*nodehandle),ac("robot_behavior_server", true) {
    ROS_INFO("constructor of RobotBehaviorInterface");

    errorCodeFinder.insert(pair<int8_t, string>(kuka_move_as::RobotBehaviorResult::NO_ERROR, "NO_ERROR"));
    errorCodeFinder.insert(pair<int8_t, string>(kuka_move_as::RobotBehaviorResult::CANCELLED, "CANCELLED"));
    errorCodeFinder.insert(pair<int8_t, string>(kuka_move_as::RobotBehaviorResult::WRONG_PARAMETER, "WRONG_PARAMETER"));
    errorCodeFinder.insert(pair<int8_t, string>(kuka_move_as::RobotBehaviorResult::TIMEOUT, "TIMEOUT"));
    errorCodeFinder.insert(pair<int8_t, string>(kuka_move_as::RobotBehaviorResult::UNREACHABLE, "UNREACHABLE"));
    errorCodeFinder.insert(pair<int8_t, string>(kuka_move_as::RobotBehaviorResult::GRIPPER_FAULT, "GRIPPER_FAULT"));
    errorCodeFinder.insert(pair<int8_t, string>(kuka_move_as::RobotBehaviorResult::COLLISION, "COLLISION"));
    errorCodeFinder.insert(pair<int8_t, string>(kuka_move_as::RobotBehaviorResult::PART_DROPPED, "PART_DROPPED"));  
    errorCodeFinder.insert(pair<int8_t, string>(kuka_move_as::RobotBehaviorResult::PRECOMPUTED_TRAJ_ERR, "PRECOMPUTED_TRAJ_ERR"));  

    //    {kuka_move_as::RobotBehaviorResult::PRECOMPUTED_TRAJ_ERR, "PRECOMPUTED_TRAJ_ERR"},

    ROS_INFO("waiting for robot_behavior_server: ");
    bool server_exists = ac.waitForServer(ros::Duration(1.0));
    while (!server_exists) {
        ROS_WARN("waiting robot_behavior_server...");
        ros::spinOnce();
        ros::Duration(1.0).sleep();
        server_exists = ac.waitForServer(ros::Duration(1.0));
    }
    ROS_INFO("connected to robot_behavior_server"); // if here, then we connected to the server;   

    //gripper = nh.serviceClient<osrf_gear::VacuumGripperControl>("/ariac/gripper/control");
    //gripperStateSubscriber = nh.subscribe("/ariac/gripper/state", 10, &RobotInterface::gripperStateCallback, this);
    /*
    called = false;
    attached = false;
    while(!called && ros::ok()) {
        //ROS_INFO("Waiting for joint feedback...");
        ros::spinOnce();
        ros::Duration(0.02).sleep();
    }
    if (!gripper.exists()) {
        gripper.waitForExistence();
    }
    attach.request.enable = 1;
    detach.request.enable = 0;
    arrivalTime = 0.5;
     * */
}
//    return( sendGoal(goal_type,part,timeout));
bool RobotBehaviorInterface::sendGoal(unsigned short int goal_type, double timeout) {
    behaviorServerGoal_.type = goal_type;
    behaviorServerGoal_.timeout = timeout;
    goal_start_time_ = ros::Time::now();    
    action_server_returned_ = false;
    ac.sendGoal(behaviorServerGoal_, boost::bind(&RobotBehaviorInterface::doneCb, this, _1, _2), boost::bind(&RobotBehaviorInterface::activeCb, this), boost::bind(&RobotBehaviorInterface::feedbackCb, this, _1));
    //BLOCKING!!
    return(wrap_up());    
}

//    bool sendGoal(unsigned short int goal_type, Part part, double timeout = MAX_ACTION_SERVER_WAIT_TIME);

bool RobotBehaviorInterface::sendGoal(unsigned short int goal_type, Part part, double timeout) {
    behaviorServerGoal_.type = goal_type;
    behaviorServerGoal_.sourcePart = part;
    behaviorServerGoal_.timeout = timeout;
    goal_start_time_ = ros::Time::now();    
    action_server_returned_ = false;
    ac.sendGoal(behaviorServerGoal_, boost::bind(&RobotBehaviorInterface::doneCb, this, _1, _2), boost::bind(&RobotBehaviorInterface::activeCb, this), boost::bind(&RobotBehaviorInterface::feedbackCb, this, _1));
    //BLOCKING!!
    return(wrap_up());    
}

void RobotBehaviorInterface::doneCb(const actionlib::SimpleClientGoalState &state, const kuka_move_as::RobotBehaviorResultConstPtr &result) {
    action_server_returned_ = true;
    goal_success_ = result->success;
    errorCode_ = result->errorCode;
    //currentRobotState = result->robotState;
    //error_code_name_map
    //    ROS_INFO("attempting move from %s  to %s ", map_pose_code_to_name[posecode_start].c_str(),
    //         map_pose_code_to_name[posecode_goal].c_str());
    ROS_INFO("Action finished  with result error code: %s",  errorCodeFinder[errorCode_].c_str());
//    ROS_INFO("Gripper position is: %f, %f, %f\n",
//             currentRobotState.gripperPose.pose.position.x, currentRobotState.gripperPose.pose.position.y,
//             currentRobotState.gripperPose.pose.position.z);
//    showJointState(currentRobotState.jointNames, currentRobotState.jointStates);
}

void RobotBehaviorInterface::feedbackCb(const kuka_move_as::RobotBehaviorFeedbackConstPtr &feedback) {
    //do nothing
    //currentRobotState = feedback->robotState;
}

void RobotBehaviorInterface::cancel() {
    ac.cancelGoal();
}

void RobotBehaviorInterface::activeCb() {
    // ROS_INFO("Goal sent");
}

//blocking function!!
bool RobotBehaviorInterface::pick(Part part, double timeout) {
    ROS_INFO("pick fnc called");
    short unsigned  int goal_type = kuka_move_as::RobotBehaviorGoal::PICK;
    return( sendGoal(goal_type,part,timeout));
}
//    bool discard_grasped_part(double timeout=0);

bool RobotBehaviorInterface::discard_grasped_part(double timeout) {
    ROS_INFO("discard fnc called");
    short unsigned  int goal_type = kuka_move_as::RobotBehaviorGoal::DISCARD_GRASPED_PART_Q1;
    return( sendGoal(goal_type,timeout));
}
/*
void RobotInterface::gripperStateCallback(const osrf_gear::VacuumGripperState::ConstPtr &msg) {
    currentGripperState = *msg;
    attached = msg->attached;
}

osrf_gear::VacuumGripperState RobotInterface::getGripperState() {
    ros::spinOnce();
    return currentGripperState;
}

bool RobotInterface::isGripperAttached() {
    ros::spinOnce();
    return attached;
}

bool RobotInterface::waitForGripperAttach(double timeout) {
    timeout = timeout <= 0? FLT_MAX:timeout;
    ros::spinOnce();
    while((!attached) && timeout > 0 && ros::ok()) {
        ROS_INFO("Retry grasp");
        release();
        ros::Duration(0.2).sleep();
        ros::spinOnce();
        grab();
        ros::Duration(0.8).sleep();
        ros::spinOnce();
        timeout -= 1.0;
    }
    return attached;
}




void RobotInterface::grab() {
    //ROS_INFO("enable gripper");
    gripper.call(attach);
}

void RobotInterface::release() {
    //ROS_INFO("release gripper");
    gripper.call(detach);
}

 * */
bool RobotBehaviorInterface::wrap_up() {
    while (!action_server_returned_) {
        ROS_INFO("waiting on action  server");
        ros::Duration(0.5).sleep();
        ros::spinOnce();
    }
    if (errorCode_==kuka_move_as::RobotBehaviorResult::NO_ERROR) {
      return true;
    }
    return false;
}