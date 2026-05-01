float32[6] thrust # Six thrusters
int8[6] thruster_map # Address for each thruster
uint8 led_brightness_1
uint8 led_brightness_2 # Eventually should make this an array but im lazy -PC
int8[2] camera_servos # Two Camera Servos, either -1, 0, or 1
string[2] servo_ssh_targets # The ssh targets of the servos (ex. easternedge@picam0.local)