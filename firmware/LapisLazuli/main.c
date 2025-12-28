#include <pico/stdlib.h>
#include <pico/multicore.h>
#include <pico/mutex.h>
#include <hardware/uart.h>
#include <hardware/irq.h>
#include <hardware/pwm.h>
#include <hardware/clocks.h>
#include "minihdlc/minihdlc.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#define PWM_FREQ_HZ 80
#define PWM_WRAP 32000
#define UINT8_DUTY_CYCLE_TO_PWM_COUNT(x) (((x)*32000)/255)

// Divide clock frequency by 1 MHz (80 * 32000)
// Now, every `CLOCK_DIV` clock cycles, 1 us passes 
#define CLOCK_DIV (clock_get_hz(clk_sys) / (PWM_FREQ_HZ * PWM_WRAP)) 

#define UART_ID uart0
#define UART_TX_PIN 16
#define UART_RX_PIN 17
#define RS485_SEND_MODE_PIN 18
#define UART_BAUD_RATE 115200

uint8_t thrusterAcceleration = 1;
uint16_t thrusterTimeoutMS = 2500;
const uint8_t thrusterPins[6] = {0, 1, 2, 3, 4, 5};
uint8_t targetThrust[6] = {127, 127, 127, 127, 127, 127};
uint8_t currentThrust[6] = {127, 127, 127, 127, 127, 127};
mutex_t thrusterMutex;

// LED Pins have different positions on the Pico v.s. LapisLazuli (tooling board) 
#ifdef LAPIS_LAZULI
const uint8_t ledPins[2] = {14, 15};
#else 
const uint8_t ledPins[2] = {25, 15};
#endif 
#define LED_BIT_OFFSET 0x06

const uint8_t servoPins[4] = {6, 7, 8, 9};
#define SERVO_BIT_OFFSET 0x08

const uint8_t motorPins[4] = {10, 11, 12, 13};
const uint8_t motorDirectionPins[4] = {26, 27, 28, 29};
#define MOTOR_BIT_OFFSET 0x0C

uint64_t thrusterLastInputTime = 0;


int sgn(int x) {
    if (x > 0) return 1;
    else if (x < 0) return -1;
    else return 0;
}

// Unused at the moment since we aren't sending anything back to topsides
void send_char(uint8_t data) {
    uart_putc_raw(UART_ID, data);
}

// FRAMING:
//  SetThrust:
//      Byte 0: 0x00 to 0x05 (maps to thruster number 0 to 5)
//      Byte 1: Thruster speed (maps to 1100us to 1900us)
//  SetLedBrightness:
//      Byte 0: 0x06 or 0x07 (maps to LED 0 and 1)
//      Byte 1: LED brightness (maps to 0% to 100% duty cycle)
//  SetServoPosition:
//      Byte 0: 0x08 to 0x0B (maps to servo number 0 to 3)
//      Byte 1: Servo position (map to 500us to 2500us)
//  SetMotorSpeed:
//      Byte 0: 
//          0x0C to 0x0F: maps to motor number 0 to 3 in default direction (direction pin low)
//          0x10 to 0x13: maps to motor number 0 to 3 in reverse direction 
//      Byte 1: Motor speed (maps to 0% to 100% duty cycle)
//  SetPrecisionControlDCMotorParameters (not implemented):
//      Byte 0: 0x14 to 0x17: Choose which one of the motors (0 - 3) to repurpose the "camera control" motor
//      Byte 1: Control loop period (0-255 ms)
//      Byte 2-5: Control loop P gain (floating point)
//      Byte 6-9: Control loop I gain (floating point)
//      Byte 10-13: Control loop D gain (floating point)
//  SetPrecisionControlDCMotorAngle (not implemented):
//      Byte 0: 0x18 to 0x19: maps to camera number 0 to 1
//      Byte 1-2: Precicion control dc motor angle (will map to 0 to 360 degrees)
//  SetThrusterAcceleration:
//      Byte 0: 0x1A 
//      Byte 1: Thruster Acceleration (0-255)
//  SetThrusterTimeout:
//      Byte 0: 0x1B
//      Byte 1 and 2: Thruster Timeout (0-65535 ms)
void frame_received(const uint8_t *frame_buffer, uint16_t frame_length)
{
    // We expect all mesages to come in two bytes at the moment
    if (frame_length != 2 && frame_length != 3) return;

    switch (frame_buffer[0]) {

        case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: {
            mutex_enter_blocking(&thrusterMutex);
            thrusterLastInputTime = to_ms_since_boot(get_absolute_time());
            targetThrust[thrusterPins[frame_buffer[0]]] = frame_buffer[1]; // Will map 0-255 to 1100us-1900us in main loop
            mutex_exit(&thrusterMutex);
            break;
        }

        case 0x06: case 0x07: {
            uint8_t ledPin = ledPins[frame_buffer[0]-LED_BIT_OFFSET];
            pwm_set_gpio_level(ledPin, UINT8_DUTY_CYCLE_TO_PWM_COUNT(frame_buffer[1]));
            break;
        }

        case 0x08: case 0x09: case 0x0A: case 0x0B: {
            uint8_t servoPin = servoPins[frame_buffer[0]-SERVO_BIT_OFFSET];
            pwm_set_gpio_level(servoPin, 500+((frame_buffer[1]*2000)/255));
            break;
        }

        case 0x0C: case 0x0D: case 0x0E: case 0x0F: {
            uint8_t motorPin = motorPins[frame_buffer[0]-MOTOR_BIT_OFFSET];
            uint8_t motorDirectionPin = motorDirectionPins[frame_buffer[0]-MOTOR_BIT_OFFSET];
            pwm_set_gpio_level(motorPin, UINT8_DUTY_CYCLE_TO_PWM_COUNT(frame_buffer[1]));
            gpio_put(motorDirectionPin, 0);
            break;
        }

        case 0x10: case 0x11: case 0x12: case 0x13: {
            uint8_t motorPin = motorPins[frame_buffer[0]-4-MOTOR_BIT_OFFSET];
            uint8_t motorDirectionPin = motorDirectionPins[frame_buffer[0]-4-MOTOR_BIT_OFFSET];
            pwm_set_gpio_level(motorPin, UINT8_DUTY_CYCLE_TO_PWM_COUNT(frame_buffer[1]));
            gpio_put(motorDirectionPin, 1);
            break;
        }

        case 0x1A: {
            mutex_enter_blocking(&thrusterMutex);
            thrusterAcceleration = frame_buffer[1];
            mutex_exit(&thrusterMutex);
            break;
        }

        case 0x1B: {
            if (frame_length != 3) return;
            mutex_enter_blocking(&thrusterMutex);
            thrusterTimeoutMS = (frame_buffer[2] << 8) | frame_buffer[1];
            mutex_exit(&thrusterMutex);
            break;
        }

        default:
            break;
    }
}

// minihdlc does not know how to actually recieve bytes from UART, so we set up an IRQ handler to do that
// once minihdlc recieves enough bytes to form a frame, it will call our frame_received callback
void on_uart_rx() {
    if (uart_is_readable(UART_ID)) minihdlc_char_receiver(uart_getc(UART_ID));
}

void initializeUART() {
    uart_init(UART_ID, UART_BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    irq_set_exclusive_handler(UART0_IRQ, on_uart_rx);
    irq_set_enabled(UART0_IRQ, true);
    uart_set_irq_enables(UART_ID, true, false);
}

void initializePWMPins(const uint8_t *pinArray, int arraySize, int init_value) {
    for (int i = 0; i < arraySize; i++) {
        gpio_set_function(pinArray[i], GPIO_FUNC_PWM);
        pwm_set_gpio_level(pinArray[i], init_value);
    }
}

int main() {
    // Initialize stdio for debugging if usb is connected
    // stdio_init_all();    
    
    // Initalize all PWM slices to run at 50Hz and use a 1 MHz clock
    for (int i = 0; i < 7; i++) {
        pwm_set_clkdiv(i, CLOCK_DIV);
        pwm_set_wrap(i, PWM_WRAP);
    } 

    // Initalize thrusters to 0 thrust, servos to neutral, motors to 0 speed, and leds to half brightness
    initializePWMPins(thrusterPins, 6, 1500);
    initializePWMPins(servoPins, 4, 1500);
    initializePWMPins(motorPins, 4, 0);
    initializePWMPins(ledPins, 2, UINT8_DUTY_CYCLE_TO_PWM_COUNT(127));

    // Initalize all motor direction pins
    for (int i = 0; i<4;i++){
        gpio_init(motorDirectionPins[i]);
        gpio_set_dir(motorDirectionPins[i], GPIO_OUT);
        gpio_put(motorDirectionPins[i], 0);
    }

    // Initialize RS-485 to recieve mode (set SEND_MODE pin to low)
    gpio_init(RS485_SEND_MODE_PIN);
    gpio_set_dir(RS485_SEND_MODE_PIN, GPIO_OUT);
    gpio_put(RS485_SEND_MODE_PIN, 0);

    // Enable all PWM slices 
    for (int i = 0; i < 7; i++) {
        pwm_set_enabled(i, true);
    } 

    // Initialize minihdlc with our byte send and frame receive handlers
    minihdlc_init(send_char, frame_received);

    // Thruster mutex for thread safety
    mutex_init(&thrusterMutex);

    // Initalize UART pins and interrupt handling
    initializeUART();

    for (;;) {
        mutex_enter_blocking(&thrusterMutex);
        if ((to_ms_since_boot(get_absolute_time()) - thrusterLastInputTime) > thrusterTimeoutMS) {
            for (int i = 0; i < 6; i++) {
                targetThrust[i] = 127;
            }
            thrusterLastInputTime = to_ms_since_boot(get_absolute_time());
        }
        for (int i = 0; i < 6; i++) {
            if (targetThrust[i] != currentThrust[i]) {
                if (abs(targetThrust[i] - currentThrust[i]) > thrusterAcceleration) {
                    currentThrust[i] += sgn(targetThrust[i] - currentThrust[i]) * thrusterAcceleration;
                    if (currentThrust[i] > 254) currentThrust[i] = 254;
                    else if (currentThrust[i] < 0) currentThrust[i] = 0;
                } else if (abs(targetThrust[i] - currentThrust[i]) > 0) {
                    currentThrust[i] = targetThrust[i];
                }
                pwm_set_gpio_level(thrusterPins[i], 1100+((currentThrust[i]*900)/255));
            }
        }
        mutex_exit(&thrusterMutex);
        sleep_ms(6);
    }
}