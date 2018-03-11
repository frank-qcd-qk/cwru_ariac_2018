//ShipmentFiller class implementation
#include<shipment_filler/ShipmentFiller.h>

ShipmentFiller::ShipmentFiller(ros::NodeHandle* nodehandle) : nh_(*nodehandle), robotMove(nodehandle), orderManager(nodehandle) {
    conveyor_client_ = nh_.serviceClient<osrf_gear::ConveyorBeltControl>("/ariac/conveyor/control");
    //rosservice call /ariac/drone "shipment_type: order_0_shipment_0"

    drone_client_ = nh_.serviceClient<osrf_gear::DroneControl>("/ariac/drone");
    conveyor_svc_msg_GO_.request.power = 100.0;
    conveyor_svc_msg_STOP_.request.power = 0.0;
    ROS_INFO("warming up conveyor service: ");
    for (int i = 0; i < 3; i++) {//client.call(srv)
        conveyor_client_.call(conveyor_svc_msg_STOP_);
        //ROS_INFO_STREAM("response: "<<conveyor_svc_msg_STOP_);
        ros::Duration(0.1).sleep();
    }
    //ariac/box_camera_1
    box_camera_1_subscriber_ = nh_.subscribe("/ariac/box_camera_1", 1,
            &ShipmentFiller::box_camera_1_callback, this);
    box_cam_1_dist_to_go_ = 100.0; //init s.t. box is not yet under camera
    box_cam1_sees_box_ = false;
    //ariac/box_camera_2
    box_camera_2_subscriber_ = nh_.subscribe("/ariac/box_camera_2", 1,
            &ShipmentFiller::box_camera_2_callback, this);
    box_cam_2_dist_to_go_ = 100.0; //init s.t. box is not yet under camera
    box_cam2_sees_box_ = false;
    //ariac/box_camera_3
    //box_camera_3_subscriber_ = nh_.subscribe("/ariac/box_camera_3", 1,
    //        &ShipmentFiller::box_camera_3_callback, this);   
    //box_cam_3_dist_to_go_= 100.0; //init s.t. box is not yet under camera
    //box_cam3_sees_box_ = false;    

    quality_sensor_1_subscriber_ = nh_.subscribe("/ariac/quality_control_sensor_1", 1,
            &ShipmentFiller::quality_sensor_1_callback, this);
    qual_sensor_1_sees_faulty_part_ = false;

    quality_sensor_2_subscriber_ = nh_.subscribe("/ariac/quality_control_sensor_2", 1,
            &ShipmentFiller::quality_sensor_2_callback, this);
    qual_sensor_2_sees_faulty_part_ = false;

    drone_depot_laser_scan_subscriber_ = nh_.subscribe("/ariac/laser_profiler_drone_depot", 1,
            &ShipmentFiller::drone_depot_laser_scan_callback, this);

    drone_depot_sensor_sees_box_ = false;
    //  void drone_depot_prox_sensor_callback(const sensor_msgs::Range::ConstPtr & range_msg);

    orderManager.update_inventory();
    //rostopic info /ariac/proximity_sensor_drone_depot
    //Type: sensor_msgs/Range
}

void ShipmentFiller::update_inventory() {
    orderManager.update_inventory();
}

bool ShipmentFiller::choose_shipment(osrf_gear::Shipment &shipment) {
    return(orderManager.choose_shipment(shipment));
}

bool ShipmentFiller::current_order_has_been_filled() { //delete order from its vector
    return(orderManager.current_order_has_been_filled());
}

bool ShipmentFiller::current_shipment_has_been_filled() { //delete order from its vector
    return(orderManager.current_shipment_has_been_filled());
}


void ShipmentFiller::box_camera_1_callback(const osrf_gear::LogicalCameraImage::ConstPtr & image_msg) {
    box_cam_1_image_ = *image_msg;
    double box_y_val; //cam_y_val;
    //bool ShipmentFiller::find_box(osrf_gear::LogicalCameraImage cam_image,double &y_val, 
    //    geometry_msgs::Pose &cam_pose, geometry_msgs::Pose &box_pose)
    geometry_msgs::Pose cam_pose, box_pose;
    box_cam1_sees_box_ = find_box(box_cam_1_image_, box_y_val, cam_pose, box_pose);
    if (box_cam1_sees_box_) {
        //ROS_INFO("cam_y_val = %f",cam_y_val);
        box_cam_1_dist_to_go_ = -box_y_val;
        //compute the box pose w/rt world
        box_1_stamped_pose_ = compute_stPose(cam_pose, box_pose);
        //ROS_INFO("box_cam_1_dist_to_go_ = %f",box_cam_1_dist_to_go_);
    }
}

void ShipmentFiller::box_camera_2_callback(const osrf_gear::LogicalCameraImage::ConstPtr & image_msg) {
    box_cam_2_image_ = *image_msg;
    double box_y_val; //cam_y_val;
    //ROS_INFO("box cam2 calling find_box");
    geometry_msgs::Pose cam_pose, box_pose;
    box_cam2_sees_box_ = find_box(box_cam_2_image_, box_y_val, cam_pose, box_pose);

    if (box_cam2_sees_box_) {
        //ROS_INFO("box_cam2_sees box at cam_y_val = %f", box_y_val);
        box_cam_2_dist_to_go_ = -box_y_val;
        //ROS_INFO("box_cam_2_dist_to_go_ = %f",box_cam_2_dist_to_go_);
        box_2_stamped_pose_ = compute_stPose(cam_pose, box_pose);
    }
}

//NOT IN USE:

/*
void ShipmentFiller::box_camera_3_callback(const osrf_gear::LogicalCameraImage::ConstPtr & image_msg) {
    box_cam_3_image_ = *image_msg;
    double box_y_val; //cam_y_val;
    box_cam3_sees_box_ = find_box(box_cam_3_image_, box_y_val);
    if (box_cam3_sees_box_) {
        //ROS_INFO("cam_y_val = %f",cam_y_val);
        box_cam_3_dist_to_go_ = -box_y_val;
        //ROS_INFO("box_cam_1_dist_to_go_ = %f",box_cam_1_dist_to_go_);
    }
}
*/

void ShipmentFiller::quality_sensor_1_callback(const osrf_gear::LogicalCameraImage::ConstPtr & image_msg) {
    qual_sensor_1_image_ = *image_msg;
    ROS_INFO("got Qsensor1 msg...");
    qual_sensor_1_sees_faulty_part_ = find_faulty_part_Q1(qual_sensor_1_image_, bad_part_Qsensor1_);
    got_new_Q1_image_ = true;
}

void ShipmentFiller::quality_sensor_2_callback(const osrf_gear::LogicalCameraImage::ConstPtr & image_msg) {
    qual_sensor_2_image_ = *image_msg;
    qual_sensor_2_sees_faulty_part_ = find_faulty_part_Q2(qual_sensor_2_image_, bad_part_Qsensor2_);
}

bool ShipmentFiller::find_faulty_part_Q1(const osrf_gear::LogicalCameraImage qual_sensor_image,
        inventory_msgs::Part &bad_part) {
    int num_bad_parts = qual_sensor_image.models.size();
    if (num_bad_parts == 0) return false;
    //if here, find a bad part and populate bad_part w/ pose in world coords
    osrf_gear::Model model = qual_sensor_image.models[0];
    geometry_msgs::Pose cam_pose, part_pose;
    geometry_msgs::PoseStamped stPose_part_wrt_world;
    cam_pose = qual_sensor_image.pose;
    part_pose = model.pose;
    bad_part.name = model.type;
    stPose_part_wrt_world = compute_stPose(cam_pose, part_pose);
    bad_part.pose = stPose_part_wrt_world;
    bad_part.location = inventory_msgs::Part::QUALITY_SENSOR_1;
    return true;
}

bool ShipmentFiller::get_bad_part_Q1(inventory_msgs::Part &bad_part) {
    got_new_Q1_image_ = false;
    bad_part = bad_part_Qsensor1_;
    return qual_sensor_1_sees_faulty_part_;
}

bool ShipmentFiller::find_faulty_part_Q2(const osrf_gear::LogicalCameraImage qual_sensor_image,
        inventory_msgs::Part &bad_part) {
    int num_bad_parts = qual_sensor_image.models.size();
    if (num_bad_parts == 0) return false;
    //if here, find a bad part and populate bad_part w/ pose in world coords
    osrf_gear::Model model = qual_sensor_image.models[0];
    geometry_msgs::Pose cam_pose, part_pose;
    geometry_msgs::PoseStamped stPose_part_wrt_world;
    cam_pose = qual_sensor_image.pose;
    part_pose = model.pose;
    bad_part.name = model.type;
    stPose_part_wrt_world = compute_stPose(cam_pose, part_pose);
    bad_part.pose = stPose_part_wrt_world;
    bad_part.location = inventory_msgs::Part::QUALITY_SENSOR_2;
    return true;
}

geometry_msgs::PoseStamped ShipmentFiller::compute_stPose(geometry_msgs::Pose cam_pose, geometry_msgs::Pose part_pose) {
    Eigen::Affine3d cam_wrt_world, part_wrt_cam, part_wrt_world;
    cam_wrt_world = xformUtils.transformPoseToEigenAffine3d(cam_pose);
    part_wrt_cam = xformUtils.transformPoseToEigenAffine3d(part_pose);
    part_wrt_world = cam_wrt_world*part_wrt_cam;
    geometry_msgs::Pose pose_part_wrt_world = xformUtils.transformEigenAffine3dToPose(part_wrt_world);
    geometry_msgs::PoseStamped part_pose_stamped;
    part_pose_stamped.header.stamp = ros::Time::now();
    part_pose_stamped.header.frame_id = "world";
    part_pose_stamped.pose = pose_part_wrt_world;
    return part_pose_stamped;
}

geometry_msgs::PoseStamped ShipmentFiller::compute_stPose_part_in_box_wrt_world(geometry_msgs::Pose pose_wrt_box, geometry_msgs::PoseStamped box_pose_wrt_world) {
    geometry_msgs::PoseStamped part_pose_stamped;
    Eigen::Affine3d box_wrt_world, part_wrt_box, part_wrt_world;
    box_wrt_world = xformUtils.transformPoseToEigenAffine3d(box_pose_wrt_world);
    part_wrt_box = xformUtils.transformPoseToEigenAffine3d(pose_wrt_box);
    part_wrt_world = box_wrt_world*part_wrt_box;
    geometry_msgs::Pose part_pose_wrt_world = xformUtils.transformEigenAffine3dToPose(part_wrt_world);
    part_pose_stamped.header.stamp = ros::Time::now();
    part_pose_stamped.header.frame_id = "world";
    part_pose_stamped.pose = part_pose_wrt_world;
    return part_pose_stamped;
}

void ShipmentFiller::drone_depot_laser_scan_callback(const sensor_msgs::LaserScan::ConstPtr & scan_msg) {
    double max_range = scan_msg->range_max;
    //double min_sensed_range = max_range;
    ROS_INFO("max_range = %f", max_range);
    drone_depot_sensor_sees_box_ = false;
    int num_rays = scan_msg->ranges.size();
    for (int iray = 0; iray < num_rays; iray++) {
        if (scan_msg->ranges[iray] < max_range - 0.01) {
            drone_depot_sensor_sees_box_ = true;
        }
    }
}

bool ShipmentFiller::find_box(osrf_gear::LogicalCameraImage cam_image, double &y_val,
        geometry_msgs::Pose &cam_pose, geometry_msgs::Pose &box_pose) {
    int num_models = cam_image.models.size();
    if (num_models == 0) return false;
    string box_name("shipping_box");
    //cam_y_val = cam_image.pose.position.y;
    osrf_gear::Model model;
    cam_pose = cam_image.pose;
    ROS_INFO("box cam sees %d models", num_models);
    for (int imodel = 0; imodel < num_models; imodel++) {
        model = cam_image.models[imodel];
        string model_name(model.type);
        if (model_name == box_name) {
            y_val = model.pose.position.y;
            box_pose = model.pose;
            ROS_INFO("found box at y_val = %f", y_val);
            return true;
        }
    }
    //if reach here, did not find shipping_box
    return false;
}

bool ShipmentFiller::fill_shipment(osrf_gear::Shipment shipment) {
    ROS_INFO("attempting to fill shipment");
    ROS_INFO_STREAM("shipment: " << shipment);
    inventory_msgs::Part pick_part, place_part;
    int num_parts = shipment.products.size();
    int bin_num;
    bool robot_move_ok;
    osrf_gear::Product product;
    geometry_msgs::PoseStamped bin_part_pose_stamped, box_part_pose_stamped;
    geometry_msgs::Pose pose_wrt_box;
    ROS_INFO("moving to bin3 cruise pose");
    robotMove.toPredefinedPose(robot_move_as::RobotMoveGoal::BIN3_CRUISE_POSE);

    ROS_INFO("updating inventory");
    orderManager.update_inventory();
    bool move_status;
    bool destination_close_to_near_box_edge=false; //if too close, may need wrist flip
    for (int ipart = 0; ipart < num_parts; ipart++) {
        product = shipment.products[ipart];
        string part_name(product.type);
        ROS_INFO_STREAM("part " << ipart << " name is: " << part_name << endl);
        if (!orderManager.find_part(part_name, bin_num, bin_part_pose_stamped)) {
            ROS_WARN_STREAM("part " << part_name << " not in inventory!");
        } else { //if here, have found part in inventory; grab it
            //populate a "Part" message
            pick_part.location = bin_num; // don't really care about the rest for move_cruise fnc
            pick_part.name = part_name.c_str();
            pick_part.pose = bin_part_pose_stamped;
            ROS_INFO_STREAM("found part in bin " << bin_num << " at location " << bin_part_pose_stamped << endl);
            if (!pick_part_fnc(pick_part)) {
                ROS_WARN("pick_part failed");
            } else {
                ros::spinOnce(); //update view of box1
                //populate a destination Part object
                //compute the destination location:
                //  geometry_msgs::PoseStamped ShipmentFiller::compute_stPose_part_in_box_wrt_world(geometry_msgs::Pose pose_wrt_box,geometry_msgs::PoseStamped box_pose_wrt_world) {
                pose_wrt_box = product.pose;
                destination_close_to_near_box_edge = test_pose_close_to_near_box_edge(pose_wrt_box);
                box_part_pose_stamped = compute_stPose_part_in_box_wrt_world(pose_wrt_box, box_1_stamped_pose_);

                place_part = pick_part;
                place_part.location = inventory_msgs::Part::QUALITY_SENSOR_1; //CODE FOR Q1 LOCATION; 
                place_part.pose = box_part_pose_stamped;
                place_part.use_wrist_far = destination_close_to_near_box_edge; //choose wrist near vs wrist far soln
                ROS_INFO_STREAM("part destination: " << place_part << endl);
                //bool release_placed_part(double timeout=2.0);
                //move_status = place_part_no_release(place_part);
                
                if (!place_part_no_release(place_part)) {
                    ROS_WARN("place_part failed");
                } else { //release the part
                    if (!robotMove.release_placed_part()) {
                        ROS_WARN("trouble releasing part");

                    }
                }

            }


        }
        orderManager.update_inventory();
    }

    return true;
}

bool ShipmentFiller::pick_part_fnc(inventory_msgs::Part part) {
    bool robot_move_ok;
    ROS_INFO("moving to bin cruise pose ");
    robot_move_ok = robotMove.move_cruise_pose(part,4.0);
    ros::Duration(2.0).sleep();
    if (!robot_move_ok) {
        ROS_WARN("problem moving to bin cruise pose");
        return false;
    }
    ROS_INFO("acquiring part");
    robot_move_ok = robotMove.pick(part);
    //this ends up in hover pose
    if (!robot_move_ok) {
        ROS_WARN("problem picking part");
        return false;
    }

    ROS_INFO("moving to bin cruise pose");
    robot_move_ok = robotMove.move_cruise_pose(part,4.0);
    ros::Duration(2.0).sleep();
    if (!robot_move_ok) {
        ROS_WARN("problem moving to bin cruise pose");
        return false;
    }

    return true;
}

//    geometry_msgs::Pose pose_wrt_box;
bool ShipmentFiller::test_pose_close_to_near_box_edge(geometry_msgs::Pose pose_wrt_box) {
    return true;
}


//assumes starts from bin cruise pose

bool ShipmentFiller::place_part_no_release(inventory_msgs::Part destination_part) {
    bool robot_move_ok;
    ROS_INFO("moving to box-fill cruise pose ");
    robot_move_ok = robotMove.move_cruise_pose(destination_part,4.0);
    ros::Duration(2.0).sleep();
    if (!robot_move_ok) {
        ROS_WARN("problem moving to box-fill cruise pose");
        return false;
    }
    ROS_INFO("attempting part placement");
    robot_move_ok = robotMove.place_part_no_release(destination_part, 5.0);
    ros::Duration(2.0).sleep();
    return robot_move_ok;
}

bool ShipmentFiller::replace_faulty_parts_inspec1(osrf_gear::Shipment shipment) {
    ROS_INFO("dummy: replace faulty parts");
    ros::Duration(2.0).sleep();
    return true;
}

bool ShipmentFiller::replace_faulty_parts_inspec2(osrf_gear::Shipment shipment) {
    ROS_INFO("dummy: replace faulty parts2");
    ros::Duration(2.0).sleep();
    return true;
}

bool ShipmentFiller::adjust_shipment_part_locations(osrf_gear::Shipment shipment) {
    ROS_INFO("dummy: adjust part locations");
    ros::Duration(2.0).sleep();
    return true;
}

bool ShipmentFiller::correct_dropped_part(osrf_gear::Shipment shipment) {
    return true;
}

void ShipmentFiller::set_drone_shipment_name(osrf_gear::Shipment shipment) {
    droneControl_.request.shipment_type = shipment.shipment_type;
}

bool ShipmentFiller::report_shipment_to_drone() {
    //rosservice call /ariac/drone "shipment_type: order_0_shipment_0"
    //droneControl_.shipment_type =  SHOULD ALREADY BE SET
    ROS_INFO("informing drone of ready shipment");
    //rosservice call /ariac/drone "shipment_type: order_0_shipment_0"
    droneControl_.response.success = false;
    while (!droneControl_.response.success) {
        drone_client_.call(droneControl_);
        ros::Duration(0.5).sleep();
    }
    return true;
}

bool ShipmentFiller::advance_shipment_on_conveyor(int location_code) {
    //$ rosservice call /ariac/conveyor/control "power: 100"
    ROS_INFO("advancing conveyor to location code %d", location_code);
    bool conveyor_ok =false;
    switch (location_code) {
        case BOX_INSPECTION1_LOCATION_CODE:
            while (!conveyor_ok) {
               conveyor_client_.call(conveyor_svc_msg_GO_);
               conveyor_ok = conveyor_svc_msg_GO_.response.success;
               ros::spinOnce();
            }
            ROS_INFO("conveyor commanded to move");
            if (!box_cam1_sees_box_) ROS_INFO("box not yet seen at cam1 station");
            while (!box_cam1_sees_box_) {
                ROS_INFO("box not yet seen at cam1 station");
                conveyor_client_.call(conveyor_svc_msg_GO_);
                ros::spinOnce();
                ros::Duration(0.5).sleep();
            }
            ROS_INFO("now see box at box_cam_1_dist_to_go_ = %f", box_cam_1_dist_to_go_);
            while (box_cam_1_dist_to_go_ > 0) {
                ros::spinOnce();
                ros::Duration(0.1).sleep();
            }
            ROS_INFO("stopping conveyor");
            conveyor_client_.call(conveyor_svc_msg_STOP_);
            ROS_INFO("now see box at box_cam_1_dist_to_go_ = %f", box_cam_1_dist_to_go_);
            return true;
            break;

        case BOX_INSPECTION2_LOCATION_CODE:
            ROS_INFO("advancing box to first inspection station at box cam2");
            conveyor_client_.call(conveyor_svc_msg_GO_);
            if (!box_cam2_sees_box_) ROS_INFO("box not yet seen at inspection 1 station");
            while (!box_cam2_sees_box_) {
                ROS_INFO("box not yet seen at cam2 station");
                ros::spinOnce();
                ros::Duration(0.5).sleep();
            }
            ROS_INFO("now see box at box_cam_2_dist_to_go_ = %f", box_cam_2_dist_to_go_);
            while (box_cam_2_dist_to_go_ > 0) {
                ros::spinOnce();
                ros::Duration(0.1).sleep();
            }
            ROS_INFO("stopping conveyor");
            conveyor_client_.call(conveyor_svc_msg_STOP_);
            ROS_INFO("now see box at box_cam_2_dist_to_go_ = %f", box_cam_2_dist_to_go_);
            return true;
            break;

        case DRONE_DOCK_LOCATION_CODE:
            ROS_INFO("advancing box to drone depot");
            conveyor_client_.call(conveyor_svc_msg_GO_);
            while (!drone_depot_sensor_sees_box_) {
                ros::Duration(0.5).sleep();
                ros::spinOnce();
                ROS_INFO("scanner does  not see box at drone depot");
            }
            ROS_INFO("prox sensor sees box!");
            conveyor_client_.call(conveyor_svc_msg_STOP_);
            return true;
            break;

        default: ROS_WARN("advance_shipment_on_conveyor: location code not recognized");
            return false;
            break;
    }

    return true;
}
