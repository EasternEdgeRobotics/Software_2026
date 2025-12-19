#include <pico/stdlib.h>
#include <pico/multicore.h>
#include <hardware/uart.h>
#include <hardware/irq.h>
#include <hardware/pwm.h>
#include <hardware/clocks.h>

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"

#define PWM_FREQ_HZ 50
#define PWM_WRAP 20000
#define UINT8_DUTY_CYCLE_TO_PWM_COUNT(x) (((x)*20000)/255)

// Divide clock frequency by 1 MHz (50 * 200000)
// Now, every `CLOCK_DIV` clock cycles, 1 us passes 
#define CLOCK_DIV (clock_get_hz(clk_sys) / (PWM_FREQ_HZ * PWM_WRAP)) 

#define UART_ID uart0
#define UART_TX_PIN 16
#define UART_RX_PIN 17
#define RS485_SEND_MODE_PIN 18
#define UART_BAUD_RATE 115200

#define THRUSTER_ACCELERATION 1
#define THRUSTER_TIMEOUT 2500
const uint8_t thrusterPins[6] = {0, 1, 2, 3, 4, 5};
uint8_t targetThrust[6] = {127, 127, 127, 127, 127, 127};
uint8_t currentThrust[6] = {127, 127, 127, 127, 127, 127};

// LED Pins have different positions on the Pico v.s. LapisLuzuli (tooling board) 
#ifdef LAPIS_LUZULI
const uint8_t ledPins[2] = {14, 15};
#else 
const uint8_t ledPins[2] = {25, 15};
#endif 
#define LED_BIT_OFFSET 0x06

const uint8_t servoPins[4] = {6, 7, 8, 9};
uint8_t servoPositions[4] = {127, 127, 127, 127};
#define SERVO_BIT_OFFSET 0x08

const uint8_t motorPins[4] = {10, 11, 12, 13};
const uint8_t motorDirectionPins[4] = {26, 27, 28, 29};
uint8_t targetSpeeds[4] = {127, 127, 127, 127};
uint8_t currentCurrent[4] = {127, 127, 127, 127};
#define MOTOR_BIT_OFFSET 0x0D

uint64_t thrusterLastInputTime = 0;


int sgn(int x) {
    if (x > 0) return 1;
    else if (x < 0) return -1;
    else return 0;
}

// Used since we aren't sending anythng
void send_char(uint8_t data) {
    uart_putc_raw(UART_ID, data);
}

// FRAMING:
//  Thrusters:
//      Byte 0: 0x00 to 0x05 (maps to thruster number 0 to 5)
//      Byte 1: Thruster speed (maps to 1100us to 1900us)
//  Leds:
//      Byte 0: 0x06 or 0x07 (maps to LED 0 and 1)
//      Byte 1: LED brightness (maps to 0% to 100% duty cycle)
//  Servos:
//      Byte 0: 0x08 to 0x0B (maps to servo number 0 to 3)
//      Byte 1: Servo position (map to 500us to 2500us)
//  Motors:
//      Byte 0: 
//          0x0C to 0x0F: maps to motor number 0 to 3 in default direction (direction pin low)
//          0x10 to 0x13: maps to motor number 0 to 3 in reverse direction 
//      Byte 1: Motor speed (maps to 0% to 100% duty cycle)
void frame_received(const uint8_t *frame_buffer, uint16_t frame_length)
{
    // We expect all mesages to come in two bytes
    if (frame_length != 2) return;

    switch (frame_buffer[0]) {
        case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05:
            thrusterLastInputTime = to_ms_since_boot(get_absolute_time());
            targetThrust[thrusterPins[frame_buffer[0]]] = frame_buffer[1];
            break;
        case 0x06: case 0x07:
            uint8_t ledPin = ledPins[frame_buffer[0]-LED_BIT_OFFSET];
            pwm_set_gpio_level(ledPin, frame_buffer[1]);
            break;
        case 0x08: case 0x09: case 0x0A: case 0x0B:
            uint8_t servoPin = servoPins[frame_buffer[0]-SERVO_BIT_OFFSET];
            pwm_set_gpio_level(servoPin, 500+((frame_buffer[1]*2000)/255));
            break;
        case 0x0C: case 0x0D: case 0x0E: case 0x0F:
            uint8_t motorPin = motorPins[frame_buffer[0]-MOTOR_BIT_OFFSET];
            pwm_set_gpio_level(motorPin, UINT8_DUTY_CYCLE_TO_PWM_COUNT(frame_buffer[1]));
            gpio_put(motorPin, 0);
            break;
        case 0x10: case 0x11: case 0x12: case 0x13:
            uint8_t motorPin = motorPins[frame_buffer[0]-4-MOTOR_BIT_OFFSET];
            pwm_set_gpio_level(motorPin, UINT8_DUTY_CYCLE_TO_PWM_COUNT(frame_buffer[1]));
            gpio_put(motorPin, 1);
            break;
        default:
            break;
    }
}

void uartHandler() {
    while (uart_is_readable(UART_ID)) {

        minihdlc_char_reciever(uart_getc(UART_ID));
    }
}

void on_uart_rx() {
    uartHandler();
}

void uartComm() {
    uart_init(UART_ID, UART_BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    irq_set_exclusive_handler(UART0_IRQ, on_uart_rx);
    irq_set_enabled(UART0_IRQ, true);
    uart_set_irq_enables(UART_ID, true, false);

    while (true) tight_loop_contents();
}

void initalizePWMPins(uint8_t *pinArray, size_t arraySize, int init_value) {

    // Initialize thruster pins
    for (int i = 0; i < arraySize; i++) {
        gpio_set_function(pinArray[i], GPIO_FUNC_PWM);
        pwm_set_gpio_level(pinArray[i], init_value);
    }

}

int main() {
    // stdio_init_all();
    
    // Initalize all PWM slices to run at 50Hz and use a 1 MHz clock
    for (int i = 0; i < 7; i++) {
        pwm_set_clkdiv(i, CLOCK_DIV);
        pwm_set_wrap(i, PWM_WRAP);
    } 

    initalizePWMPins(thrusterPins, 6, 1500);
    initalizePWMPins(servoPins, 4, 1500);
    initalizePWMPins(motorPins, 4, 0); // PWM duty cycle correspond directly to speed for the motor drivers
    initalizePWMPins(ledPins, 2, UINT8_DUTY_CYCLE_TO_PWM_COUNT(0));

    // Initalize all motor direction pins
    for (int i = 0; i<4;i++){
        gpio_init(motorDirectionPins[i]);
        gpio_set_dir(motorDirectionPins[i], GPIO_OUT);
        gpio_put(motorDirectionPins[i], 0);
    }

    // Initialize RS-485 to recieve mode
    gpio_init(RS485_SEND_MODE_PIN);
    gpio_set_dir(RS485_SEND_MODE_PIN, GPIO_OUT);
    gpio_put(RS485_SEND_MODE_PIN, 0);

    // Enable all PWM slices 
    for (int i = 0; i < 7; i++) {
        pwm_set_enabled(i, true);
    } 

    // Initialize minihdlc with our send and recieve handlers
    minihdlc_init(send_char, frame_received);

    multicore_launch_core1(uartComm);

    for (;;) {
        if ((to_ms_since_boot(get_absolute_time()) - thrusterLastInputTime) > THRUSTER_TIMEOUT) {
            for (int i = 0; i < 6; i++) {
                targetThrust[i] = 127;
            }
            thrusterLastInputTime = to_ms_since_boot(get_absolute_time());
        }
        for (int i = 0; i < 6; i++) {
            if (targetThrust[i] != currentThrust[i]) {
                if (abs(targetThrust[i] - currentThrust[i]) > THRUSTER_ACCELERATION) {
                    currentThrust[i] += sgn(targetThrust[i] - currentThrust[i]) * THRUSTER_ACCELERATION;
                    if (currentThrust[i] > 254) currentThrust[i] = 254;
                    else if (currentThrust[i] < 0) currentThrust[i] = 0;
                } else if (abs(targetThrust[i] - currentThrust[i]) > 0) {
                    currentThrust[i] = targetThrust[i];
                }
                float thrust = (currentThrust[i] - -127) * (2.0 - 1.0) / (127 - -127) + 0.5;
                pwm_set_gpio_level(thrusterPins[i], PWM_WRAP / 10 * thrust);
            }
        }
        sleep_ms(6);
    }
}