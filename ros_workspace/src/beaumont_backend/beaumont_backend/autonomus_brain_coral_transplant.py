# #!/usr/bin/env python3
# # Imports needed to initiate a complex ROS2 action node
# import rclpy
# from rclpy.action import ActionServer, CancelResponse, GoalResponse
# from rclpy.callback_groups import ReentrantCallbackGroup
# from rclpy.executors import MultiThreadedExecutor
# from rclpy.node import Node
# import threading

# # Import the custom ROS2 interfaces (messages) used by EER
# from eer_interfaces.action import BeaumontAutoMode 
# from eer_interfaces.msg import ThrusterMultipliers, PilotInput
# from eer_interfaces.srv import Config

# # Import libraries for MJPEG stream viewing and colour filtering
# import cv2 
# import numpy as np
# import urllib.request

# # Other libraries used
# from time import time, sleep
# import json

# # Out of the 4 cameras on Beaumont, the brain coral transplant action node will use the downward-looking camera
# BOTTOM_CAMERA_INDEX = 3

# # The required heave time after picking up the brain coral may differ between simulation and real life
# HEAVE_TIME_SIMULATION = 10 
# HEAVE_TIME = 10

# xy_FINE_TUNING_SIMULATION = 1
# xy_FINE_TUNING = 5

# HEAVE_SPEED_SIMULATION = 10
# HEAVE_SPEED = 30

# class AutonomusBrainCoralTransplant(Node):
#     """
#     Action node used for the autonomus brain coral transplant task.

#     This node recieves a goal from an action server to deliver the brain coral to the desired location. It achieves 
#     the goal using colour filtering and notifies the pilot of the progress by publishing regular feedback.

#     At all times, the pilot has the ability to cancel the goal and take over manually.
#     """

#     def __init__(self):
#         super().__init__('autonomus_brain_coral_transplant')

#         # Initiate the ROS2 action server
#         self._goal_handle = None
#         self._goal_lock = threading.Lock()
#         self._action_server = ActionServer(
#             self,
#             BeaumontAutoMode, # Uses the custom EER BeaumontAutoMode interface
#             'autonomus_brain_coral_transplant', # Topic on which the action server and client communicate 
#             execute_callback=self.execute_callback, 
#             goal_callback=self.goal_callback, 
#             handle_accepted_callback=self.handle_accepted_callback,
#             cancel_callback=self.cancel_callback,
#             callback_group=ReentrantCallbackGroup()) # Allows for the goal to be cancelled while being executed by making the action server multi-threaded
        
#         # The following publishers publish inputs to which both the simulation and real ROV respond 
#         self.pilot_publisher = self.create_publisher(PilotInput, "pilot_input", 1)
#         self.copilot_publisher = self.create_publisher(ThrusterMultipliers, "thruster_multipliers", 1)

#         # Client to fetch the bottom camera MJPEG stream url saved the database
#         self.camera_urls_client = self.create_client(Config, 'camera_urls')

#         self.fetch_tooling_camera_stream_url()
    
#     def fetch_tooling_camera_stream_url(self):
        
#         while True:
#             camera_url_config_request = Config.Request()

#             # State == 1 means GET urls from database
#             camera_url_config_request.state = 1 

#             # This field is not used for fetch requests
#             camera_url_config_request.data = ""

#             # Ensure the database server is up before continuing 
#             self.camera_urls_client.wait_for_service()
            
#             future = self.camera_urls_client.call_async(camera_url_config_request)
            
#             rclpy.spin_until_future_complete(self,future) # Wait for result

#             # If the length of the MJPEG stream urls list is 0, the client should loop until the URLS are assigned (done in the topsides browser-based graphical user interface) 
#             if len(json.loads(future.result().result)) > 0:
#                 self.bottom_camera_stream_url = json.loads(future.result().result)[BOTTOM_CAMERA_INDEX]
#                 self.get_logger().info(f"Autonomus Brain Coral Transplant Node Fetched {self.bottom_camera_stream_url}")
#                 break
#             else:
#                 self.get_logger().info(f"Cannot get bottom camera MJPEG stream url, will try again in 1 second")

#             sleep(1)

#     def destroy(self):
#         self._action_server.destroy()
#         super().destroy_node()

#     def goal_callback(self, goal_request):
#         """Accept or reject a client request to begin an action."""
#         self.get_logger().info('Received goal request')
#         return GoalResponse.ACCEPT

#     def handle_accepted_callback(self, goal_handle):
#         with self._goal_lock:
#             # This server only allows one goal at a time
#             if self._goal_handle is not None and self._goal_handle.is_active:
#                 self.get_logger().info('Aborting previous goal')
#                 # Abort the existing goal
#                 self._goal_handle.abort()
#             self._goal_handle = goal_handle

#         goal_handle.execute()

#     def cancel_callback(self, goal):
#         """
#         Accept or reject a client request to cancel an action.
#         Note that cancel requests are handeled explicitly in the enter_autonomus_mode method.
#         """
#         self.get_logger().info('Received cancel request')
#         return CancelResponse.ACCEPT
    
#     def execute_callback(self, goal_handle):
#         """Execute the goal."""
#         self.get_logger().info('Executing goal...')

#         self.enter_autonomus_mode(goal_handle)

#         # If execution gets here, it means that the action is either done or cancelled
#         result = BeaumontAutoMode.Result()

#         if goal_handle.is_cancel_requested:
#             goal_handle.canceled()
#         elif goal_handle.is_active:
#             goal_handle.succeed()

#             # Populate result message
#             result.success = True

#         return result
#     # Since pilot will have no control, this open claw command will not timeout
#     def enter_autonomus_mode(self, goal_handle):
         
#         # Initialize the feedback message 
#         feedback_msg = BeaumontAutoMode.Feedback()

#         # Set Beaumont to full power
#         full_power_multipliers = ThrusterMultipliers()
#         full_power_multipliers.power = 100
#         full_power_multipliers.surge = 100
#         full_power_multipliers.sway = 100
#         full_power_multipliers.heave = 100
#         full_power_multipliers.pitch = 100
#         full_power_multipliers.yaw = 100
#         self.copilot_publisher.publish(full_power_multipliers)

#         # Open the MJPEG camera stream
#         stream = urllib.request.urlopen(self.bottom_camera_stream_url)
#         bytes = b''

#         # Predefine variables before entering loop
#         heave_stage = True
#         time_since_action_start = time()
#         time_since_pilot_input_publishing = time()

#         ##################### Calculation Reduction #####################

#         first_iteration = True # Will be set to false after the first iteration

#         # Will inialize as many variables as possible here to avoid recalculating the same values
#         camera_stream_area = 0

#         camera_stream_area_for_biggest_contour_detection = 0
#         desired_x_location = 0
#         desired_y_location = 0

#         percent_5_of_x_resolution = 0
#         percent_5_of_y_resolution = 0

#         alignment_adjustment_value_pos = xy_FINE_TUNING_SIMULATION if goal_handle.request.is_for_sim else xy_FINE_TUNING
#         alignment_adjustment_value_neg = -xy_FINE_TUNING_SIMULATION if goal_handle.request.is_for_sim else -xy_FINE_TUNING

#         heave_speed = HEAVE_SPEED_SIMULATION if goal_handle.request.is_for_sim else HEAVE_SPEED

#         #################################################################

#         # Action loop
#         while True:
            
#             current_time = time()

#             # The following boolean will be set to True if the region is found
#             brain_coral_area_found = False

#             # Exit action loop as soon as pilot cancels the goal
#             # Can be re-entered later
#             if not goal_handle.is_active or goal_handle.is_cancel_requested:
#                 self.get_logger().info('Goal aborted or canceled')
#                 break

#             # Initialize the pilot_input message 
#             pilot_input = PilotInput()

#             pilot_input.is_autonomous = True

#             # Convert the MJPEG stream into a format recognizable by OpenCV
#             bytes += stream.read(1024)

#             # Check if feed is valid
#             a = bytes.find(b'\xff\xd8')
#             b = bytes.find(b'\xff\xd9')

#             if a != -1 and b != -1:

#                 # Grab the JPG encoded binary string
#                 jpg = bytes[a:b+2]
#                 bytes = bytes[b+2:]
                
#                 # Convert to an rgb (red, green, blue) image
#                 rgb_image = cv2.imdecode(np.fromstring(jpg, dtype=np.uint8), cv2.IMREAD_COLOR)

#                 # Use the rgb image to convert to an hsv (hue, shade, value) image
#                 hsv_image = cv2.cvtColor(rgb_image, cv2.COLOR_BGR2HSV)

#                 # Apply a mask on the image based on the lower and upper boards of the brain coral area colour.
#                 # This is passed in by the action client when requesting the goal. 
#                 mask = cv2.inRange(hsv_image, np.array(goal_handle.request.lower_hsv_bound), np.array(goal_handle.request.upper_hsv_bound))

#                 if first_iteration:

#                     # Perform the following calculations only on the first loop iteration

#                     camera_stream_area_for_biggest_contour_detection = mask.shape[0] * mask.shape[1] * 0.04

#                     desired_x_location = mask.shape[0]*0.5 
#                     desired_y_location = mask.shape[1]*0.1

#                     percent_5_of_x_resolution = mask.shape[0] * 0.05
#                     percent_5_of_y_resolution = mask.shape[1] * 0.05

#                     first_iteration = False

#                 # Grab the contours of the filtered region
#                 contours = self.get_contours(mask)

#                 # Filter out all red regions for the one with the biggest area
#                 biggest_contour = np.ndarray(shape= (1,1,1))
#                 biggest_contour_area = 0
#                 for contour in contours:
#                     area = cv2.contourArea(contour)
#                     if area > biggest_contour_area:
#                         biggest_contour_area = area
#                         biggest_contour = contour

#                 # If there is one decently sized contour, proceed to center atop it. 
#                 if (len(contours) > 0) and (biggest_contour_area > camera_stream_area_for_biggest_contour_detection): 
#                     brain_coral_area_center_location_x, brain_coral_area_center_location_y = self.get_contour_center(biggest_contour)
#                     brain_coral_area_found = True
#                 else:
#                     brain_coral_area_center_location_x, brain_coral_area_center_location_y = 0, 0
#                     brain_coral_area_found = False
                    
#                 # The following algoritm depends on whether or not the brain coral area was found
#                 if brain_coral_area_found:
                    
#                     heave_stage = False

#                     feedback_msg.status = "Found brain coral area: "

#                     aligned_x = False
#                     aligned_y = False

#                     # If the brain coral region is not within 5% of the desired area, continue centering on it
#                     if abs(brain_coral_area_center_location_x - desired_x_location) > percent_5_of_x_resolution: 
#                         aligned_x = False
#                         if brain_coral_area_center_location_x < desired_x_location:
#                             pilot_input.sway = alignment_adjustment_value_neg
#                             feedback_msg.status += "Swaying left "
#                         else:
#                             pilot_input.sway = alignment_adjustment_value_pos
#                             feedback_msg.status += "Swaying right "
#                         self.get_logger().info(f"{(brain_coral_area_center_location_x, desired_x_location)}")
#                     else: 
#                         pilot_input.sway = 0
#                         aligned_x = True

#                     # Also center in the y
#                     if abs(brain_coral_area_center_location_y - desired_y_location) > percent_5_of_y_resolution: 
#                         aligned_y = False
#                         if brain_coral_area_center_location_y < desired_y_location:
#                             pilot_input.surge = alignment_adjustment_value_pos
#                             feedback_msg.status += "Surging forward"
#                         else:
#                             pilot_input.surge = alignment_adjustment_value_neg
#                             feedback_msg.status += "Surging back"
#                     else:
#                         pilot_input.surge = 0
#                         aligned_y = True

#                     # Once aligned, heave down on brain coral area
#                     if aligned_x and aligned_y:

#                         if (biggest_contour_area < (camera_stream_area * 0.5)):
#                             pilot_input.heave = -heave_speed

#                             feedback_msg.status += "Aligned! Heaving Down!"

#                         else:
#                             pilot_input.open_claw = True
#                             pilot_input.heave = -10

                            
#                             claw_opening_start_time = time()

#                             while (time()-claw_opening_start_time < 5):
#                                 if time()-time_since_pilot_input_publishing > 0.1:
                                    
#                                     feedback_msg.status = "Opening Claw!"

#                                     time_since_pilot_input_publishing = time()

#                                     goal_handle.publish_feedback(feedback_msg) 
#                                     self.pilot_publisher.publish(pilot_input)

                            
#                             feedback_msg.status = "Autonomus task done!"

#                             goal_handle.publish_feedback(feedback_msg) 

#                             break # Break out of loop to end autonomus task

#                     else:

#                         # Ensure brain coral is being gripped until it is time to let go 
#                         pilot_input.close_claw = True

                    

#                     goal_handle.publish_feedback(feedback_msg) 

#                 else: 
                    
#                     # Initially, Beaumont will heave after the pilot initates autonomus mode
#                     if heave_stage:

#                         pilot_input.heave = heave_speed*3
#                         pilot_input.close_claw = True

#                         feedback_msg.status = "Heaving up"
#                         goal_handle.publish_feedback(feedback_msg)

#                         if current_time - time_since_action_start > (HEAVE_TIME_SIMULATION if goal_handle.request.is_for_sim else HEAVE_TIME):
#                             heave_stage = False

#                     # Surge forward until the brain coral region is detected. Pilot will be facing the coral reef when autonomus mode is initalized
#                     else:

#                         pilot_input.surge = heave_speed
#                         pilot_input.close_claw = True
#                         pilot_input.heave = 0

#                         feedback_msg.status = "Surging forward"
#                         goal_handle.publish_feedback(feedback_msg)
                
                
#                 if time()-time_since_pilot_input_publishing > 0.1:
#                     time_since_pilot_input_publishing = time()
#                     self.pilot_publisher.publish(pilot_input)


#         # Action loop is exited. Ensure to switch over control to pilot by resetting the multipliers to their inital value and publishing empty pilot input
#         pilot_input = PilotInput()

#         restored_power_multipliers = ThrusterMultipliers()
#         restored_power_multipliers.power = goal_handle.request.starting_power
#         restored_power_multipliers.surge = goal_handle.request.starting_surge
#         restored_power_multipliers.sway = goal_handle.request.starting_sway
#         restored_power_multipliers.heave = goal_handle.request.starting_heave
#         restored_power_multipliers.pitch = goal_handle.request.starting_pitch
#         restored_power_multipliers.yaw = goal_handle.request.starting_yaw


#         feedback_msg.status = "Giving control back to the pilot"

#         goal_handle.publish_feedback(feedback_msg)
#         self.copilot_publisher.publish(restored_power_multipliers)
#         self.pilot_publisher.publish(pilot_input)
    
#     def get_contours(self, mask):
#         """
#         Given a black and white image where white represents pixels that have met a threshold, this function
#         uses OpenCV's findContours to locate the contours or 'edges' of the white areas and it returns these OpenCV
#         contour objects in a list.
#         """
#         contours, hirearchy = cv2.findContours(mask.copy(), cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)
#         return contours
    
#     def get_contour_center(self, contour):
#         """
#         Given a contour, this function uses the OpenCV moments function to
#         find the center of area of that contour.
#         """
#         M = cv2.moments(contour)
#         cx = -1
#         cy = -1
#         if (M['m00'] != 0):
#             cx= round(M['m10']/M['m00'])
#             cy= round(M['m01']/M['m00'])
#         return cx, cy



# def main(args=None):
#     rclpy.init(args=args)

#     action_server = AutonomusBrainCoralTransplant()

#     # We use a MultiThreadedExecutor to handle incoming goal requests concurrently
#     executor = MultiThreadedExecutor()
#     rclpy.spin(action_server, executor=executor)

#     action_server.destroy()
#     rclpy.shutdown()


# if __name__ == '__main__':
#     main()