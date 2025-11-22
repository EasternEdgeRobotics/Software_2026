#include "waterwitch_constants.h"

// TERMINOLOGY
// for star -> forward right
// for port -> forward left
// aft star -> backward right
// aft port -> backward left
// star top -> upward right
// port top -> upward left


const int8_t THRUSTER_CONFIG_MATRIX[6][6] = {
    {-1,-1, 0, 0, 0, -1}, // for star
    {-1, 1, 0, 0, 0, 1}, // for port
    {1, -1, 0, 0, 0, 1}, // aft star
    {1, 1, 0, 0, 0, -1}, // aft port
    {0, 0, 1, 0, 1, 0}, // star top
    {0, 0, 1, 0, -1, 0} // port top
};

const std::string THRUSTER_NAMES[6] = {
    "for_star",
    "for_port",
    "aft_star",
    "aft_port",
    "star_top",
    "port_top"
};

const int RP2040_ADDRESS = 0x69;

const int SOFTWARE_TO_BOARD_COMMUNICATION_RATE = 100;

const int SERVO_ANGLE_INCREMENT = 30;

const int MIN_SERVO_ANGLE = 0;

const int MAX_SERVO_ANGLE = 270;

const int PILOT_COMMUNICATION_LOSS_THRUSTER_TIMEOUT_MS = 2000;

const int LED_BRIGHTNESS_INCREMENT = 85;
