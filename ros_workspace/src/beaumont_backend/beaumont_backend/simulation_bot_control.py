#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from rclpy.action import ActionClient

from eer_interfaces.msg import PilotInput
from eer_interfaces.action import BeaumontAutoMode 
from eer_interfaces.srv import HSVColours

from rclpy.callback_groups import ReentrantCallbackGroup
from rclpy.executors import MultiThreadedExecutor

from geometry_msgs.msg import Wrench, Twist
from std_msgs.msg import String, Int32, Float64

from math import sqrt, cos, pi

import random

SIMULAITON_PERCISION = 0.1 # 10 Hz

MAX_VALUE = {
    "for-port-top": 0,
    "for-star-top": 0,
    "aft-port-top": 0,
    "aft-star-top": 0,
    "for-port-bot": 0,
    "for-star-bot": 0,
    "aft-port-bot": 0,
    "aft-star-bot": 0
}

# Set to true to test backend publishers
TEST_BACKEND_PUBLISHERS = True

class SimulationBotControl(Node):

    def __init__(self):
        super().__init__('simulation_bot_control')

        self.pilot_listener = self.create_subscription(PilotInput, 'pilot_input', self.pilot_listener_callback, 1)
        
        self.diagnostics_data_publisher_1 = self.create_publisher(String, "diagnostics_data_1", 10)
        self.diagnostics_data_publisher_2 = self.create_publisher(String, "diagnostics_data_2", 10)

        # Thruster publishers
        self.for_star_top_publisher = self.create_publisher(Int32, "/beaumont/for_star_top", 10)
        self.for_port_top_publisher = self.create_publisher(Int32, "/beaumont/for_port_top", 10)
        self.aft_port_top_publisher = self.create_publisher(Int32, "/beaumont/aft_port_top", 10)
        self.aft_star_top_publisher = self.create_publisher(Int32, "/beaumont/aft_star_top", 10)
        self.for_port_bot_publisher = self.create_publisher(Int32, "/beaumont/for_port_bot", 10)
        self.for_star_bot_publisher = self.create_publisher(Int32, "/beaumont/for_star_bot", 10)
        self.aft_port_bot_publisher = self.create_publisher(Int32, "/beaumont/aft_port_bot", 10)
        self.aft_star_bot_publisher = self.create_publisher(Int32, "/beaumont/aft_star_bot", 10)

        # Claw publisher
        self.claws_publisher = self.create_publisher(Float64, "/Beaumont/claws/cmd_vel", 10) 

        # Autonomus movement node
        self._action_client = ActionClient(self, BeaumontAutoMode, 'autonomus_brain_coral_transplant')
        self.autonomus_mode_active = False

        # Client to fetch the hsv colour values camera saved on the task_manager database
        self.brain_coral_hsv_colour_bounds_client = self.create_client(HSVColours, 'set_color', callback_group=ReentrantCallbackGroup())

        # The default bounds filter for the colour RED
        self.brain_coral_hsv_colour_bounds = {
            "upper_hsv":[10,255,140],
            "lower_hsv":[0,242,60]
        }

        # Debugger publisher
        # from std_msgs.msg import String
        # self.debugger = self.create_publisher(String, 'debugger', 10) 

        self.power_multiplier = 0
        self.surge_multiplier = 0
        self.sway_multiplier = 0
        self.heave_multiplier = 0
        self.pitch_multiplier = 0
        self.yaw_multiplier = 0


        # The differnce between the planar move plugin and the force plugin is that the planar move plugin acts relative to the bot, while the force plugin acts relative to the world
        
        self.net_force_array = {"surge":0,
                                       "sway":0,
                                       "heave":0,
                                       "pitch":0,
                                       "yaw":0}

        self.velocity_array = {"surge":0,
                                    "sway":0,
                                    "heave":0,
                                    "pitch":0,
                                    "yaw":0} 

        self.surface_area_for_drag = {"surge":0.1813,
                                        "sway":0.2035,
                                        "heave":0.2695,
                                        "pitch":0.2,
                                        "yaw":0.2} 
        

        self.bot_mass = 23 #23kg

        # The max thruster force is estimated to be 110 Newtons
        self.max_thruster_force = 110 

        # Thruster distance from center of mass is assumed to be around 0.35m
        self.thruster_distance_from_COM = 0.35

        self.fluid_mass_density = 1000 # 1000kg/m^3
        
        # This value has been tuned. The higher it is, the faster the bot reaches terminal velocity
        self.drag_coefficient = 0.1 

        # Assuming the terminal velocity of the bot in water is 0.25 m/s... the force adjustment factor should be 150
        self.gazebo_simulation_velocity_z_adjustment_factor = 150 

        self.gazebo_simulation_velocity_xy_adjustment_factor = 0.04

        self.gazebo_simulation_velocity_pitch_adjustment_factor = 30

        self.gazebo_simulation_velocity_yaw_adjustment_factor = 0.05

        self.bot_yaw_to_claw_yaw_factor = 90

        # prevent unused variable warning
        self.pilot_listener
    

    def pilot_listener_callback(self, msg):  
        '''Called when new controller input from pilot is recieved'''

        self.power_multiplier = float(msg.power_multiplier/100)
        self.surge_multiplier = float(msg.surge_multiplier/100)
        self.sway_multiplier = float(msg.sway_multiplier/100)
        self.heave_multiplier = float(msg.heave_multiplier/100)
        self.pitch_multiplier = float(msg.pitch_multiplier/100)
        self.yaw_multiplier = float(msg.yaw_multiplier/100)
        
        if not self.autonomus_mode_active or (self.autonomus_mode_active):
            thruster_forces = self.simulation_rov_math(msg)
            # TODO: Cleanup this file now that thruster functionality has changed
            for_star_top = Int32()
            for_star_top.data = int(thruster_forces["for-star-top"] * 127 + 127)
            self.for_star_top_publisher.publish(for_star_top)

            for_port_top = Int32()
            for_port_top.data = int(thruster_forces["for-port-top"] * 127 + 127)
            self.for_port_top_publisher.publish(for_port_top)

            aft_port_top = Int32()
            aft_port_top.data = int(thruster_forces["aft-port-top"] * 127 + 127)
            self.aft_port_top_publisher.publish(aft_port_top)

            aft_star_top = Int32()
            aft_star_top.data = int(thruster_forces["aft-star-top"] * 127 + 127)
            self.aft_star_top_publisher.publish(aft_star_top)

            for_port_bot = Int32()
            for_port_bot.data = int(thruster_forces["for-port-bot"] * 127 + 127)
            self.for_port_bot_publisher.publish(for_port_bot)

            for_star_bot = Int32()
            for_star_bot.data = int(thruster_forces["for-star-bot"] * 127 + 127)
            self.for_star_bot_publisher.publish(for_star_bot)

            aft_port_bot = Int32()
            aft_port_bot.data = int(thruster_forces["aft-port-bot"] * 127 + 127)
            self.aft_port_bot_publisher.publish(aft_port_bot)

            aft_star_bot = Int32()
            aft_star_bot.data = int(thruster_forces["aft-star-bot"] * 127 + 127)
            self.aft_star_bot_publisher.publish(aft_star_bot)

            # from std_msgs.msg import String
            # velocity = String()
            # velocity.data = str(self.velocity_array)
            # self.debugger.publish(velocity)

            self.simulation_tooling(msg)
            
            if TEST_BACKEND_PUBLISHERS:
                self.simulate_backend_publishers(msg)
        
        # March 2025: Auto mode does not work anymore due to a series of changes
        # This code is commented out to prevent i2c_master.py from crashing or getting stuck

        # if msg.enter_auto_mode:
        #     if not self.autonomus_mode_active:
        #         self.autonomus_mode_active = True
        #         self.send_autonomus_mode_goal()
        #     else:
        #         # self._action_client.wait_for_server()

        #         future = self.goal_handle.cancel_goal_async()
        #         future.add_done_callback(self.cancel_done)

    def simulate_backend_publishers(self, controller_inputs):

        diagnostics_data = String()

        diagnostics_data.data += f"adc_48v_bus: {round(random.uniform(40,48.5), 4)}\n"
        diagnostics_data.data += f"adc_12v_bus: {round(random.uniform(11.5,12.1), 4)}\n"
        diagnostics_data.data += f"adc_5v_bus: {round(random.uniform(4.8,5.2), 4)}\n"
        diagnostics_data.data += f"temperature: {round(random.uniform(30,55), 4)}\n"
        diagnostics_data.data += f"acceleration: {[round(random.uniform(11.5,12.1), 4), round(random.uniform(11.5,12.1), 4), round(random.uniform(11.5,12.1), 4)]}\n"
        diagnostics_data.data += f"magnetic: {[round(random.uniform(11.5,12.1), 4), round(random.uniform(11.5,12.1), 4), round(random.uniform(11.5,12.1), 4)]}\n"
        diagnostics_data.data += f"euler: {[round(random.uniform(11.5,12.1), 4), round(random.uniform(11.5,12.1), 4), round(random.uniform(11.5,12.1), 4)]}\n"
        diagnostics_data.data += f"linear_acceleration: {[round(random.uniform(11.5,12.1), 4), round(random.uniform(11.5,12.1), 4), round(random.uniform(11.5,12.1), 4)]}\n"
        diagnostics_data.data += f"power_board_u8: {round(random.uniform(4.8,5.2), 4)}\n"
        diagnostics_data.data += f"power_board_u9: {round(random.uniform(4.8,5.2), 4)}\n"
        diagnostics_data.data += f"power_board_u10: {round(random.uniform(4.8,5.2), 4)}\n"
        diagnostics_data.data += f"mega_board_ic2: {round(random.uniform(4.8,5.2), 4)}\n"
        diagnostics_data.data += f"power_board_u11: {round(random.uniform(4.8,5.2), 4)}\n"
        diagnostics_data.data += f"mega_board_ic1: {round(random.uniform(4.8,5.2), 4)}\n"
        diagnostics_data.data += f"outside_temperature_probe_data: {round(random.uniform(10,15), 4)}\n"
        
        self.diagnostics_data_publisher_1.publish(diagnostics_data)

    def simulation_tooling(self, controller_inputs):
        """Controls the movement of the simulation claw. """

        if not (controller_inputs.open_claw ^ controller_inputs.close_claw):

            claws_velocity = Float64()

            claws_velocity.data = float(-0.00001)

            self.claws_publisher.publish(claws_velocity)
        
        elif controller_inputs.open_claw:

            claws_velocity = Float64()

            claws_velocity.data = float(0.1)

            self.claws_publisher.publish(claws_velocity)

        elif controller_inputs.close_claw:

            claws_velocity = Float64()

            claws_velocity.data = float(-0.1)

            self.claws_publisher.publish(claws_velocity)

    # def send_autonomus_mode_goal(self):

    #     self.fetch_brain_coral_hsv_colour_bounds()
    #     goal_msg = BeaumontAutoMode.Goal()
    #     goal_msg.is_for_sim = True

    #     # HSV (hue,shade,value) bounds for filtering brain coral area
    #     goal_msg.lower_hsv_bound = self.brain_coral_hsv_colour_bounds["lower_hsv"]
    #     goal_msg.upper_hsv_bound = self.brain_coral_hsv_colour_bounds["upper_hsv"]

    #     goal_msg.starting_power = int(self.power_multiplier * 100)
    #     goal_msg.starting_surge = int(self.surge_multiplier * 100)
    #     goal_msg.starting_sway = int(self.sway_multiplier * 100)
    #     goal_msg.starting_heave = int(self.heave_multiplier * 100)
    #     goal_msg.starting_pitch = int(self.pitch_multiplier * 100)
    #     goal_msg.starting_yaw = int(self.yaw_multiplier * 100) 

    #     self._action_client.wait_for_server()

    #     self._send_goal_future = self._action_client.send_goal_async(goal_msg, feedback_callback=self.feedback_callback)

    #     self._send_goal_future.add_done_callback(self.goal_response_callback)

    # def goal_response_callback(self, future):
    #     self.goal_handle = future.result()
    #     if not self.goal_handle.accepted:
    #         self.get_logger().info('Goal rejected :(')
    #         return

    #     self.get_logger().info('Goal accepted :)')

    #     self._get_result_future = self.goal_handle.get_result_async()
    #     self._get_result_future.add_done_callback(self.get_result_callback)


    # def get_result_callback(self, future):
    #     result = future.result().result
    #     autonomous_mode_status = String()
    #     autonomous_mode_status.data = "Autonomous Mode off, {0}".format("Mission Success" if result.success else "Mission Failed")
    #     self.diagnostics_data_publisher_2.publish(autonomous_mode_status)
    #     self.autonomus_mode_active = False

    # def feedback_callback(self, feedback_msg):
    #     feedback = feedback_msg.feedback
    #     autonomous_mode_status = String()
    #     autonomous_mode_status.data = "Autonomous Mode on, {0}".format(feedback.status)
    #     self.diagnostics_data_publisher_2.publish(autonomous_mode_status)
    
    # def cancel_done(self, future):
    #     cancel_response = future.result()
    #     if len(cancel_response.goals_canceling) > 0:
    #         autonomous_mode_status = String()
    #         autonomous_mode_status.data = 'Auto mode successfully canceled'
    #         self.diagnostics_data_publisher_2.publish(autonomous_mode_status)
    #     else:
    #         autonomous_mode_status = String()
    #         autonomous_mode_status.data = 'Auto mode failed to cancel'
    #         self.diagnostics_data_publisher_2.publish(autonomous_mode_status)

    # def fetch_brain_coral_hsv_colour_bounds(self):

    #     hsv_colour_bounds_request = HSVColours.Request()

    #     # load_to_database = False indicates loading FROM database
    #     hsv_colour_bounds_request.load_to_database = False 

    #     # Ensure the database server is up before continuing 
    #     self.brain_coral_hsv_colour_bounds_client.wait_for_service()
            
    #     future = self.brain_coral_hsv_colour_bounds_client.call(hsv_colour_bounds_request)
        
    #     if future.success: # Means that HSV colours were stored in the database at this time
    #         self.brain_coral_hsv_colour_bounds["upper_hsv"] = future.upper_hsv
    #         self.brain_coral_hsv_colour_bounds["lower_hsv"] = future.lower_hsv
    #     else:
    #         self.get_logger().info("No HSV colour bounds stored in database. Will keep using default.")



    def simulation_rov_math(self, controller_inputs):
        """Builds upon the real Rov Math Method in i2c_master.py and calculates net forces on Bot."""

        thruster_values = {}

        surge = controller_inputs.surge * self.power_multiplier * self.surge_multiplier * 0.01
        sway = controller_inputs.sway * self.power_multiplier * self.sway_multiplier * 0.01
        yaw = controller_inputs.yaw * self.power_multiplier * self.yaw_multiplier * 0.01

        if controller_inputs.heave_up or controller_inputs.heave_down:
            controller_inputs.heave = (100 if controller_inputs.heave_up else 0) + (-100 if controller_inputs.heave_down else 0)
            
        heave = controller_inputs.heave * self.power_multiplier * self.heave_multiplier * 0.01   

        # if controller_inputs.pitch_up or controller_inputs.pitch_down:
        #     controller_inputs.pitch = (100 if controller_inputs.pitch_up else 0) + (-100 if controller_inputs.pitch_down else 0)
        
        pitch = controller_inputs.pitch * self.power_multiplier * self.pitch_multiplier * 0.01  

        sum_of_magnitudes_of_pilot_input = abs(surge) + abs(sway) + abs(heave) + abs(pitch) + abs(yaw)

        # These adjustment factors determine how much to decrease power in each thruster due to multipliers.
        # The first mutliplication term determines the total % to remove of the inital thruster direction,
        # The second term scales that thruster direction down based on what it makes up of the sum of pilot input, and also applies a sign
        surge_adjustment = (1 - (self.power_multiplier * self.surge_multiplier * abs(controller_inputs.surge) * 0.01)) * (surge/sum_of_magnitudes_of_pilot_input if sum_of_magnitudes_of_pilot_input else 0)  
        sway_adjustment = (1 - (self.power_multiplier * self.sway_multiplier * abs(controller_inputs.sway) * 0.01)) * (sway/sum_of_magnitudes_of_pilot_input if sum_of_magnitudes_of_pilot_input else 0) 
        heave_adjustment = (1 - (self.power_multiplier * self.heave_multiplier * abs(controller_inputs.heave) * 0.01)) * (heave/sum_of_magnitudes_of_pilot_input if sum_of_magnitudes_of_pilot_input else 0) 
        pitch_adjustment = (1 - (self.power_multiplier * self.pitch_multiplier * abs(controller_inputs.pitch) * 0.01)) * (pitch/sum_of_magnitudes_of_pilot_input if sum_of_magnitudes_of_pilot_input else 0) 
        yaw_adjustment = (1 - (self.power_multiplier * self.yaw_multiplier * abs(controller_inputs.yaw) * 0.01)) * (yaw/sum_of_magnitudes_of_pilot_input if sum_of_magnitudes_of_pilot_input else 0) 

        thruster_scaling_coefficient = 1 / sum_of_magnitudes_of_pilot_input if sum_of_magnitudes_of_pilot_input else 0

        # Calculations below are based on thruster positions:

        # First term:
        # The net pilot input based on how it applies to the specifc thruster (some are reveresed) is calculated.
        # Then, this is scaled down by the thruster scaling coefficent such that the max absolute value it can attain is 1.
        # This will properly activate distribute load and direction among thrusters such that the desired movement is reached,
        # but the first term cancels out the effect of the thruster multipliers 

        # Directional adjustment factors:
        # These adjustment factors will never increase the power going to a single thruster.
        # They will only serve to proportionally decrease it in order to reduce power in a certain direction. 
         
        thruster_values["for-port-bot"] = (((-surge)+(sway)+(heave)+(pitch)+(yaw)) * thruster_scaling_coefficient) + surge_adjustment - sway_adjustment - heave_adjustment - pitch_adjustment - yaw_adjustment
        thruster_values["for-star-bot"] = (((-surge)+(-sway)+(heave)+(pitch)+(-yaw)) * thruster_scaling_coefficient) + surge_adjustment + sway_adjustment - heave_adjustment - pitch_adjustment + yaw_adjustment
        thruster_values["aft-port-bot"] = (((surge)+(sway)+(heave)+ (-pitch)+(-yaw)) * thruster_scaling_coefficient) - surge_adjustment - sway_adjustment - heave_adjustment + pitch_adjustment + yaw_adjustment
        thruster_values["aft-star-bot"] = (((surge)+(-sway)+(heave)+(-pitch)+(yaw)) * thruster_scaling_coefficient) - surge_adjustment + sway_adjustment - heave_adjustment + pitch_adjustment - yaw_adjustment
        thruster_values["for-port-top"] = (((-surge)+(sway)+(-heave)+(-pitch)+(yaw)) * thruster_scaling_coefficient) + surge_adjustment - sway_adjustment + heave_adjustment + pitch_adjustment - yaw_adjustment
        thruster_values["for-star-top"] = (((-surge)+(-sway)+(-heave)+(-pitch)+(-yaw)) * thruster_scaling_coefficient) + surge_adjustment + sway_adjustment + heave_adjustment + pitch_adjustment + yaw_adjustment
        thruster_values["aft-port-top"] = (((surge)+(sway)+(-heave)+(pitch)+(-yaw)) * thruster_scaling_coefficient) - surge_adjustment - sway_adjustment + heave_adjustment - pitch_adjustment + yaw_adjustment
        thruster_values["aft-star-top"] = (((surge)+(-sway)+(-heave)+(pitch)+(yaw)) * thruster_scaling_coefficient) - surge_adjustment + sway_adjustment + heave_adjustment - pitch_adjustment - yaw_adjustment

        ####################################################################
        ############################## DEBUG ###############################
        ####################################################################

        # for thruster_position in MAX_VALUE:
        #     if MAX_VALUE[thruster_position] < thruster_values[thruster_position]:
        #         MAX_VALUE[thruster_position] = thruster_values[thruster_position]

        
        # from std_msgs.msg import String
        # msg = String()
        # msg.data = str(thruster_values)
        # self.debugger.publish(msg) 

        #################################################################################
        ############################## SIMULATION PORTION ###############################
        #################################################################################

        net_surge = self.max_thruster_force * cos(pi/3)*((-thruster_values["for-port-bot"]) + (-thruster_values["for-star-bot"]) + (thruster_values["aft-port-bot"]) + (thruster_values["aft-star-bot"]) + (-thruster_values["for-port-top"]) + (-thruster_values["for-star-top"]) + (thruster_values["aft-port-top"]) + (thruster_values["aft-star-top"]))
        net_sway = self.max_thruster_force * cos(pi/3)*((thruster_values["for-port-bot"]) + (-thruster_values["for-star-bot"]) + (thruster_values["aft-port-bot"]) + (-thruster_values["aft-star-bot"]) + (thruster_values["for-port-top"]) + (-thruster_values["for-star-top"]) + (thruster_values["aft-port-top"]) + (-thruster_values["aft-star-top"]))
        net_heave = self.max_thruster_force * cos(pi/3)*((thruster_values["for-port-bot"]) + (thruster_values["for-star-bot"]) + (thruster_values["aft-port-bot"]) + (thruster_values["aft-star-bot"]) + (-thruster_values["for-port-top"]) + (-thruster_values["for-star-top"]) + (-thruster_values["aft-port-top"]) + (-thruster_values["aft-star-top"]))
        
        net_pitch = self.max_thruster_force * self.thruster_distance_from_COM * cos(pi/4)*((thruster_values["for-port-bot"]) + (thruster_values["for-star-bot"]) + (-thruster_values["aft-port-bot"]) + (-thruster_values["aft-star-bot"]) + (-thruster_values["for-port-top"]) + (-thruster_values["for-star-top"]) + (thruster_values["aft-port-top"]) + (thruster_values["aft-star-top"]))
        net_yaw = self.max_thruster_force * self.thruster_distance_from_COM * cos(pi/4)*((thruster_values["for-port-bot"]) + (-thruster_values["for-star-bot"]) + (-thruster_values["aft-port-bot"]) + (thruster_values["aft-star-bot"]) + (thruster_values["for-port-top"]) + (-thruster_values["for-star-top"]) + (-thruster_values["aft-port-top"]) + (thruster_values["aft-star-top"]))
    
        return thruster_values
        
        ##################################################################################
        ##################################################################################
        ##################################################################################



def main(args=None):
    rclpy.init(args=args)

    simulation_bot_control = SimulationBotControl()

    # We use a MultiThreadedExecutor to handle incoming goal requests concurrently
    executor = MultiThreadedExecutor()
    rclpy.spin(simulation_bot_control, executor=executor)

    rclpy.spin(simulation_bot_control)

    simulation_bot_control.destroy_node()


if __name__ == "__main__":
    main()
