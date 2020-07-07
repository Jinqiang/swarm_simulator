// clang-format off
// ROS
#include <ros/ros.h>

// Octomap
#include <octomap_msgs/Octomap.h>
#include <octomap_msgs/conversions.h>
#include <octomap/OcTree.h>
#include <dynamicEDT3D/dynamicEDTOctomap.h>

// Parameters
#include <param.hpp>
#include <mission.hpp>
#include <timer.hpp>

// Submodules
#include <ecbs_planner.hpp>
#include <rbp_corridor.hpp>
#include <rbp_planner.hpp>
#include <rbp_publisher.hpp>

#include <fstream>

using namespace SwarmPlanning;

bool has_octomap = false;
bool has_path = false;
std::shared_ptr<octomap::OcTree> octree_obj;

void octomapCallback(const octomap_msgs::Octomap& octomap_msg)
{
    if(has_octomap)
        return;

    octree_obj.reset(dynamic_cast<octomap::OcTree*>(octomap_msgs::fullMsgToMap(octomap_msg)));

    has_octomap = true;
}

int main(int argc, char* argv[]) {
    ROS_INFO("Swarm Trajectory Planner");
    ros::init (argc, argv, "swarm_traj_planner_rbp");
    ros::NodeHandle nh( "~" );
    ros::Subscriber octomap_sub = nh.subscribe( "/octomap_full", 1, octomapCallback );

    // Mission
    Mission mission;
    if(!mission.setMission(nh)){
        return -1;
    }

    // ROS Parameters
    Param param;
    if(!param.setROSParam(nh)){
        return -1;
    }
    param.setColor(mission.qn);

    // Submodules
    SwarmPlanning::PlanResult planResult;
    std::shared_ptr<DynamicEDTOctomap> distmap_obj;
    std::shared_ptr<InitTrajPlanner> initTrajPlanner_obj;
    std::shared_ptr<Corridor> corridor_obj;
    std::shared_ptr<RBPPlanner> RBPPlanner_obj;
    std::shared_ptr<RBPPublisher> RBPPublisher_obj;

    // Main Loop
    ros::Rate rate(20);
    Timer timer_total;
    Timer timer_step;
    double start_time, current_time;
    while (ros::ok()) {
        if (has_octomap && !has_path) {
            timer_total.reset();

            // Build 3D Euclidean Distance Field
            timer_step.reset();
            {
                float maxDist = 1;
                octomap::point3d min_point3d(param.world_x_min, param.world_y_min, param.world_z_min);
                octomap::point3d max_point3d(param.world_x_max, param.world_y_max, param.world_z_max);
                distmap_obj.reset(new DynamicEDTOctomap(maxDist, octree_obj.get(), min_point3d, max_point3d, false));
                distmap_obj.get()->update();
            }
            timer_step.stop();
            ROS_INFO_STREAM("distmap runtime: " << timer_step.elapsedSeconds());

            // Step 1: Plan Initial Trajectory
            timer_step.reset();
            {
                initTrajPlanner_obj.reset(new ECBSPlanner(distmap_obj, mission, param));
                if (!initTrajPlanner_obj.get()->update(param.log, &planResult)) {
                    return -1;
                }
            }
            timer_step.stop();
            ROS_INFO_STREAM("Initial Trajectory Planner runtime: " << timer_step.elapsedSeconds());

            // Step 2: Generate SFC, RSFC
            timer_step.reset();
            {
                corridor_obj.reset(new Corridor(distmap_obj, mission, param));
                if (!corridor_obj.get()->update(param.log, &planResult)) {
                    return -1;
                }
            }
            timer_step.stop();
            ROS_INFO_STREAM("BoxGenerator runtime: " << timer_step.elapsedSeconds());

            // Step 3: Formulate QP problem and solving it to generate trajectory for quadrotor swarm
            timer_step.reset();
            {
                RBPPlanner_obj.reset(new RBPPlanner(mission, param));
                if (!RBPPlanner_obj.get()->update(param.log, &planResult)) {
                    return -1;
                }
            }
            timer_step.stop();
            //ROS_INFO_STREAM("SwarmPlanner runtime: " << timer_step.elapsedSeconds());

            timer_total.stop();       
            ROS_INFO_STREAM("Results: Overall Computation time: " << timer_total.elapsedSeconds());

            //////////////////////////////////
            std::ofstream outfile;
            outfile.open("/home/jtorde/Desktop/ws/src/swarm_simulator/swarm_planner/scripts/results.txt", std::ios::out | std::ios::app); // append instead of overwrite
            outfile << "Results: Overall Computation time: " << std::right << std::setw(50) <<timer_total.elapsedSeconds()<<"\n"; 
            outfile.close();
            /////////////////////////////////////

            // for (auto t_i:planResult.T){
            //     std::cout<<"t_i= "<<t_i<<std::endl;
            // }

            // Plot Planning Result
            RBPPublisher_obj.reset(new RBPPublisher(nh, planResult, mission, param));
            RBPPublisher_obj->plot(param.log);

            start_time = ros::Time::now().toSec();
            has_path = true;
        }
        if(has_path) {
            // Publish Swarm Trajectory
            current_time = ros::Time::now().toSec() - start_time;
            RBPPublisher_obj.get()->update(current_time);
            RBPPublisher_obj.get()->publish();
            // ros::spin();
            // return 0;
        }
        ros::spinOnce();
        rate.sleep();
    }

    return 0;
}