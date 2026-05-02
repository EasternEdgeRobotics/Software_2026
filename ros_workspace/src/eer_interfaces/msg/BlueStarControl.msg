float32[6] thrust # Six thrusters
int8[6] thruster_map # Address for each thruster
uint8[2] led_brightness
float32[2] dc_motors
int8[2] camera_servos # Two Camera Servos, either -1, 0, or 1
string[2] servo_ssh_targets # The ssh targets of the servos (ex. easternedge@picam0.local)