# Thruster inputs and power multipliers
int8 surge
int8 sway
int8 heave
int8 pitch
int8 yaw
int8 roll
uint8 power_multiplier
uint8 surge_multiplier
uint8 sway_multiplier
uint8 heave_multiplier
uint8 pitch_multiplier
uint8 yaw_multiplier
uint8 roll_multiplier
bool heave_up
bool heave_down
bool pitch_up
bool pitch_down
bool roll_cw
bool roll_ccw

# Allows for configuration of thruster mappings
bool configuration_mode
int8 configuration_mode_thruster_number

# Thruster acceleration and no-input-timeout configuration in firmware
bool set_thruster_acceleration
uint8 thruster_acceleration
bool set_thruster_timeout
uint16 thruster_timeout

# Brushed DC motor control
uint8[4] motor_speed    
bool[4] motor_direction

# Servo control
bool[4] set_servo_angle
uint8[4] servo_angle
bool[4] turn_servo_ccw
bool[4] turn_servo_cw

# LED control
bool[2] brighten_led
bool[2] dim_led

# Brushed DC motor precision camera control configuration in firmware
bool[2] set_precision_control_dc_motor_parameters
uint8[2] associated_dc_motor_number # Choose which one of the 4 DC motors is being used for precision camera control
uint8[2] control_loop_period
float32[2] proportional_gain
float32[2] integral_gain
float32[2] derivative_gain

# Brushed DC motor precision camera control angle
bool[2] set_precision_control_dc_motor_angle
uint8[2] precision_control_dc_motor_angle
bool[2] turn_precision_control_dc_motor_ccw
bool[2] turn_precision_control_dc_motor_cw


