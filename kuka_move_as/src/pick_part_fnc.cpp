//use this fnc to pick up a specified part fromm a bin
// it can be used by part-flipper and generic move(from,to)
// upon completion of "pick" robot will move to the pick-location's safe cruise pose, and it will return either:
//  NO_ERROR, or:  UNREACHABLE, PART_DROPPED, WRONG_PARAMETER, or GRIPPER_FAULT
//


// use "goal", but only need to populate the "sourcePart" component

unsigned short int KukaBehaviorActionServer::pick_part_from_bin(const kuka_move_as::RobotBehaviorGoalConstPtr &goal) {
    //unsigned short int errorCode = kuka_move_as::RobotBehaviorResult::NO_ERROR; //return this if ultimately successful
    trajectory_msgs::JointTrajectory transition_traj;
    inventory_msgs::Part part = goal->sourcePart;
    ROS_INFO("The part is %s; it should be fetched from location code %s ", part.name.c_str(),
            placeFinder_[part.location].c_str());
    ROS_INFO("part info: ");
    ROS_INFO_STREAM(part);

    //Eigen::VectorXd approx_jspace_pose;

    
    //unsigned short int KukaBehaviorActionServer::compute_bin_pickup_key_poses(inventory_msgs::Part part)
    errorCode_ = compute_bin_pickup_key_poses(part);   
    if (errorCode_ != kuka_move_as::RobotBehaviorResult::NO_ERROR) {
        return errorCode_;
    }

    //extract bin location from Part:
    int current_hover_code = location_to_pose_code_map[part.location];
    int current_cruise_code = location_to_cruise_code_map[part.location];
    
    //current_hover_code is the bin-far or bin-near hover code
    ROS_INFO("pose code from location code is: %d", current_hover_code);

    
    //MOVES START HERE
    ROS_INFO("moving to respective cruise pose...");
    if (!move_posecode1_to_posecode2(current_pose_code_, current_cruise_code)) {
        ROS_WARN("error with move between pose codes");
        errorCode_ = kuka_move_as::RobotBehaviorResult::PRECOMPUTED_TRAJ_ERR; //inform our client of error code
        return errorCode_;
    }
    //ROS_INFO("moving to respective hover pose");
    //if (!move_posecode1_to_posecode2(current_pose_code_, current_hover_code)) {

    //    ROS_WARN("error with move between pose codes");
    //    errorCode_ = kuka_move_as::RobotBehaviorResult::PRECOMPUTED_TRAJ_ERR; //inform our client of error code
    //    return errorCode_;
    //}

    //OS_INFO("ready to move to computed_bin_cruise_jspace_pose_; enter 1:");
    //cin>>ans;        
    move_to_jspace_pose(computed_bin_cruise_jspace_pose_, 0.5);             
            
    //ROS_INFO("ready to move to computed_bin_escape_jspace_pose_; enter 1:");
    //cin>>ans;    
    move_to_jspace_pose(computed_bin_escape_jspace_pose_, 1.0);       

//  move  to computed hover pose:
    //ROS_INFO("ready to move to computed_jspace_approach_; enter 1: ");
    //cin>>ans;    
    move_to_jspace_pose(computed_jspace_approach_, 1.0);    


    ROS_WARN(" DO PICKUP STEPS HERE...");

    //cout<<"moving to approach_pickup_jspace_pose_: ";
    //cin>>ans;


    //now move to pickup approach pose:
    ROS_INFO("moving to approach_pickup_jspace_pose_ ");
    move_to_jspace_pose(approach_pickup_jspace_pose_, 0.5); //try it this way instead
    //move_to_jspace_pose(APPROACH_DEPART_CODE, 1.0); //code implies qvec in member var
    //ros::Duration(1.0).sleep();

    //ROS_INFO("enabling gripper");
    gripperInterface_.grab(); //do this early, so grasp can occur at first contact
    is_attached_ =  false;
    cout<<"ready to descend to grasp; enter 1: ";
    cin>>ans;
    //now move to bin pickup pose:
    //ROS_INFO_STREAM("moving to pickup_jspace_pose_ " << std::endl << pickup_jspace_pose_.transpose());
    move_into_grasp(pickup_jspace_pose_, 1.5); //provide target pose
    cout<<"at computed grasp pose; "<<endl;    
    is_attached_ = gripperInterface_.waitForGripperAttach(1.0); //wait for grasp for a bit


    if (!is_attached_) {
        ROS_WARN("did not attach; attempting to descend further: ");
        //cin>>ans;
        if (!move_into_grasp(15.0)) {
            ROS_WARN("could not grasp part; giving up");
            move_to_jspace_pose(approach_pickup_jspace_pose_, 1.0); //

            ROS_INFO("moving to hover_jspace_pose_ ");//pickup_hover_pose_
            //move_to_jspace_pose(CURRENT_HOVER_CODE, 1.0);
            move_to_jspace_pose(pickup_hover_pose_, 1.0); //try it this way instead       
            current_pose_code_=current_hover_code; //establish code for recognized, key pose
            errorCode_ = kuka_move_as::RobotBehaviorResult::GRIPPER_FAULT;
            return errorCode_;
        }
    }
    //if here, part is attached to  gripper
    ROS_INFO("grasped part; moving to depart pose: ");
    move_to_jspace_pose(computed_jspace_approach_, 1.0);    

    
    //cout<<"ready to move to hover pose; enter 1: ";
    //cin>>ans;
    
    //ROS_INFO("moving to hover_jspace_pose_ ");

    //if (current_hover_code < Q1_HOVER_CODE) {
    //    ROS_INFO("withdrawing to nom cruise pose");
    //    ROS_INFO("from %d to %d ", current_hover_code, NOM_BIN_CRUISE);
    
    ROS_INFO("moving to computed_bin_escape_jspace_pose_");
    move_to_jspace_pose(computed_bin_escape_jspace_pose_, 1.0);  
    //modify J1-ang only to get to a cruise pose:

    ROS_INFO("moving to computed cruise pose");
    //cin>>ans;
    move_to_jspace_pose(computed_bin_cruise_jspace_pose_, 1.0);     
   ROS_INFO("moving to nom cruise pose");
    //cin>>ans;    
    move_to_jspace_pose(nom_bin_cruise_pose_, 1.0);     
    current_pose_code_ = NOM_BIN_CRUISE; //keep track of where we are, in terms of pose codes

    //check if part is still attached
    is_attached_ = gripperInterface_.isGripperAttached();
    if (!is_attached_) {
        ROS_WARN("dropped part!");
        errorCode_ = kuka_move_as::RobotBehaviorResult::PART_DROPPED; //debug--return error
        return errorCode_;
    }
    else { ROS_INFO("part is still  attached"); }
        
        errorCode_ = kuka_move_as::RobotBehaviorResult::NO_ERROR; //return success
        return errorCode_;
}




//must compute pickup_deeper_jspace_pose_ first!!
//calls for robot to move towards pickup_deeper_jspace_pose_, testing gripper status along the way
//aborts when attachment is detected
//internal func, NOT a behavior!
//does  NOT return status to client.  Need to do this separately
bool KukaBehaviorActionServer::move_into_grasp(double arrival_time) {
    ROS_INFO_STREAM("MOVE TO DEEP GRASP, pose = "<<endl<<pickup_deeper_jspace_pose_.transpose()<<endl);
    trajectory_msgs::JointTrajectory transition_traj;
    //move slowly--say 5 seconds to press into part
    transition_traj = jspace_pose_to_traj(pickup_deeper_jspace_pose_,arrival_time);
    errorCode_ = kuka_move_as::RobotBehaviorResult::NO_ERROR;
    ROS_INFO_STREAM("transition traj = "<<endl<<transition_traj<<endl);
    is_attached_ = false;
    send_traj_goal(transition_traj);  //start the motion
    while (!traj_goal_complete_ && !is_attached_) { //write a fnc for this: wait_for_goal_w_timeout
        //put timeout here...   possibly return falses
        ROS_INFO("waiting for grasp...");
        ros::Duration(0.1).sleep();
        ros::spinOnce();
        is_attached_ = gripperInterface_.isGripperAttached();
        if (is_attached_) traj_ctl_ac_.cancelGoal();
    }
    if (!is_attached_)  {
        errorCode_ = kuka_move_as::RobotBehaviorResult::GRIPPER_FAULT;
        return false;
    }
    ROS_WARN("part is attached");
    return true;

}

bool KukaBehaviorActionServer::move_into_grasp(Eigen::VectorXd pickup_jspace_pose, double arrival_time) {
    ROS_INFO_STREAM("MOVE TO GRASP, pose = "<<endl<<pickup_jspace_pose.transpose()<<endl);
    trajectory_msgs::JointTrajectory transition_traj;
    //move slowly--say 5 seconds to press into part
    transition_traj = jspace_pose_to_traj(pickup_jspace_pose,arrival_time);
    errorCode_ = kuka_move_as::RobotBehaviorResult::NO_ERROR;
    ROS_INFO_STREAM("transition traj = "<<endl<<transition_traj<<endl);
    is_attached_ = false;
    send_traj_goal(transition_traj);  //start the motion
    while (!traj_goal_complete_ && !is_attached_) { //write a fnc for this: wait_for_goal_w_timeout
        //put timeout here...   possibly return falses
        ROS_INFO("waiting for grasp...");
        ros::Duration(0.1).sleep();
        ros::spinOnce();
        is_attached_ = gripperInterface_.isGripperAttached();
        if (is_attached_) traj_ctl_ac_.cancelGoal();
    }
    if (!is_attached_)  {
        errorCode_ = kuka_move_as::RobotBehaviorResult::GRIPPER_FAULT;
        return false;
    }
    ROS_WARN("part is attached");
    return true;

}
//this version assumes the part is already grasped, and it should  be discarded
/*
unsigned short int KukaBehaviorActionServer::discard_grasped_part() {

    trajectory_msgs::JointTrajectory transition_traj;
    if (!move_posecode1_to_posecode2(current_pose_code_, Q1_DISCARD_CODE)) {
        ROS_WARN("discard_grasped_part:  error with move between pose codes %d and %d ", current_pose_code_, Q1_DISCARD_CODE);
        return errorCode_;
    }

    ROS_WARN("SHOULD DO PART RELEASE HERE");
    //cout<<"enter 1: ";
    //cin>>g_ans;

    ROS_INFO("from %d to %d ", Q1_DISCARD_CODE, Q1_CRUISE_CODE);
    if (!move_posecode1_to_posecode2(Q1_DISCARD_CODE, Q1_CRUISE_CODE)) {
        ROS_WARN("error with move between pose codes %d and %d ", Q1_DISCARD_CODE, Q1_CRUISE_CODE);
        return errorCode_;
    }


    errorCode_ = kuka_move_as::RobotBehaviorResult::NO_ERROR; //return success
    return errorCode_;
}*/
/*
//consult the "source" Part and compute if IK is realizable
unsigned short int RobotMoveActionServer::is_pickable(const kuka_move_as::RobotBehaviorGoalConstPtr &goal) {
unsigned short int errorCode = kuka_move_as::RobotBehaviorResult::NO_ERROR; //return this if ultimately successful

Part part = goal->sourcePart;
//ROS_INFO("The part is %s; it should be fetched from location code %s ", part.name.c_str(),
//         placeFinder_[part.location].c_str());
//ROS_INFO("part info: ");
//ROS_INFO_STREAM(part);
 
//compute the IK for the desired pickup pose: pickup_jspace_pose_
//first, get the equivalent desired affine of the vacuum gripper w/rt base_link;
//need to provide the Part info and the rail displacement
//Eigen::Affine3d RobotMoveActionServer::affine_vacuum_pickup_pose_wrt_base_link(Part part, double q_rail)
affine_vacuum_pickup_pose_wrt_base_link_ = affine_vacuum_pickup_pose_wrt_base_link(part,
                                                                                   bin_hover_jspace_pose_[1]);
Eigen::Vector3d O_pickup;
O_pickup = affine_vacuum_pickup_pose_wrt_base_link_.translation();
ROS_INFO_STREAM("O_pickup: "<<O_pickup.transpose());
int ans;

//provide desired gripper pose w/rt base_link, and choose soln closest to some reference jspace pose, e.g. hover pose
//if (!get_pickup_IK(cart_grasp_pose_wrt_base_link,approx_jspace_pose,&q_vec_soln);
if (!get_pickup_IK(affine_vacuum_pickup_pose_wrt_base_link_, bin_hover_jspace_pose_, pickup_jspace_pose_)) {
    ROS_WARN("could not compute IK soln for pickup pose!");
        cout<<"is_pickable: enter 1: ";
     cin>>ans;
    errorCode_ = kuka_move_as::RobotBehaviorResult::UNREACHABLE;
    return errorCode_;
}
//ROS_INFO_STREAM("pickup_jspace_pose_: " << pickup_jspace_pose_.transpose());

//compute approach_pickup_jspace_pose_:  for approximate Cartesian descent and depart
//compute_approach_IK(Eigen::Affine3d affine_vacuum_gripper_pose_wrt_base_link,Eigen::VectorXd approx_jspace_pose,double approach_dist,Eigen::VectorXd &q_vec_soln);
if (!compute_approach_IK(affine_vacuum_pickup_pose_wrt_base_link_, pickup_jspace_pose_, approach_dist_,
                         approach_pickup_jspace_pose_)) {
    ROS_WARN("could not compute IK soln for pickup approach pose!");
    errorCode_ = kuka_move_as::RobotBehaviorResult::UNREACHABLE;
    return errorCode_;
}
//ROS_INFO_STREAM("approach_pickup_jspace_pose_: " << approach_pickup_jspace_pose_.transpose());

errorCode = kuka_move_as::RobotBehaviorResult::NO_ERROR; //return success
return errorCode;
}
 */