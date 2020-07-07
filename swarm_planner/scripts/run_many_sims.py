#!/usr/bin/env python
# coding=utf-8
# Jesus Tordesillas, jtorde@mit.edu
# date: June 2020

import math
import os
import sys
import time
import rospy
from snapstack_msgs.msg import State
import subprocess

def launchCommandAndWaitUntilFinish(command, type):
        session_name="untitled"
        os.system("tmux kill-session -t" + session_name)
        os.system("tmux new -d -s "+str(session_name)+" -x 300 -y 300")

        commands=[command]

        for i in range(len(commands)):
            print('splitting ',i)
            os.system('tmux split-window ; tmux select-layout tiled')

        for i in range(len(commands)):
            os.system('tmux send-keys -t '+str(session_name)+':0.'+str(i) +' "'+ commands[i]+'" '+' C-m')

        print("Commands sent")

        
        pos_string=""
        while (pos_string.find(' ')==-1):
            print "here!"
            try:
                print "echoing!"
                if(type=="RBP"):
                    pos_string =str(subprocess.check_output(['rostopic', 'echo', '/traj_coef/mav0', '-n', '1'])) #, '|grep', 'Results'
                else: 
                    pos_string =str(subprocess.check_output(['rostopic', 'echo', '/desired_trajectory/mav0', '-n', '1'])) #, '|grep', 'Results'
                #print(pos_string)
                print ("Sim hasn't finished yet")
            except:
                print("An Error occurred")

        time.sleep(3.0)
        print("Sim has finished")
        print("Killing the rest")
        os.system(kill_all)

def writeToFile(sentence):
        file_object = open('./results.txt', 'a')
        file_object.write(sentence);
        file_object.close()

if __name__ == '__main__':



    kill_all="tmux kill-server & killall -9 gazebo & killall -9 gzserver & pkill -f swarm_traj_planner & killall -9 gzclient & killall -9 roscore & killall -9 rosmaster & pkill faster_node & pkill -f dynamic_obstacles & pkill -f rosout & pkill -f behavior_selector_node & pkill -f rviz & pkill -f rqt_gui & pkill -f perfect_tracker & pkill -f faster_commands"

    #make sure ROS (and related stuff) is not running
    os.system(kill_all)

 
    writeToFile('\n\n\n\n============      RBP PLANNER     ================\n');
    writeToFile('==================================================\n\n');
    batch_sizes=[1,2,4,8];
    for i in range(len(batch_sizes)):
        writeToFile('============batch_size= '+str(batch_sizes[i])+'================\n');
        command="roslaunch swarm_planner plan_rbp_random_forest.launch plan_sequential:=true plan_batch_size:="+str(batch_sizes[i])+" runsim:=false log:=false "; #>> $(rospack find swarm_planner)/scripts/results.txt
        launchCommandAndWaitUntilFinish(command, "RBP");

    writeToFile('============      SCP PLANNER     ================\n');
    writeToFile('==================================================\n\n');
    step_sizes=[0.3,0.25,0.2,0.17];
    for i in range(len(step_sizes)):
        writeToFile('============step_size= '+str(step_sizes[i])+'================\n');
        for j in range(6):
            writeToFile('______\n');
            command="roslaunch swarm_planner plan_scp_empty_space.launch plan_time_step:="+str(step_sizes[i])+" runsim:=false log:=false "; #>> $(rospack find swarm_planner)/scripts/results.txt
            launchCommandAndWaitUntilFinish(command, "SCP");

