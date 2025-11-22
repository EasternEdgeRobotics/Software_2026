#include <pico/stdlib.h>
#include <pico/multicore.h>
#include <hardware/uart.h>
#include <hardware/irq.h>
#include <hardware/pwm.h>

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"


#define CLOCK_DIV 250
#define PWM_WRAP 5000

#define UART_ID uart0
#define UART_TX_PIN 0
#define UART_RX_PIN 1
#define UART_BAUD_RATE 115200

#define THRUSTER_ACCELERATION 1
#define THRUSTER_TIMEOUT 2500

int thrusterPins[6] = {10, 11, 12, 13, 14, 15};
uint8_t targetThrust[6] = {127, 127, 127, 127, 127, 127};
uint8_t currentThrust[6] = {127, 127, 127, 127, 127, 127};

#define LED_PIN 20 //TODO
#define MOTOR_PIN 19 //TODO
#define LED_WRAP 255
#define LED_CLOCK_DIV 9.77f

uint64_t lastInputTime = 0;

int sgn(int x) {
    if (x > 0) return 1;
    else if (x < 0) return -1;
    else return 0;
}

void uartHandler() {
    while (uart_is_readable(UART_ID)) {
        uint8_t receivedData[2];
        receivedData[0] = uart_getc(UART_ID);
        if (uart_is_readable(UART_ID)) {
            receivedData[1] = uart_getc(UART_ID);
        } else {
            continue;
        }
        
        lastInputTime = to_ms_since_boot(get_absolute_time());

        switch (receivedData[0]) {
            case 0: case 1: case 2: case 3: case 4: case 5:
                if (receivedData[1] == 255) targetThrust[receivedData[0]] = 254;
                else targetThrust[receivedData[0]] = receivedData[1];
            case 6:
                pwm_set_gpio_level(LED_PIN, receivedData[1]);
                break;
            case 7:
                pwm_set_gpio_level(MOTOR_PIN, receivedData[1]);
                break;
            case 255: //test case for pico led
                gpio_put(PICO_DEFAULT_LED_PIN, receivedData[1]);
            default:
                break;
        }
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

int main() {
    // stdio_init_all();

    for (int i = 0; i < 6; i++) {
        gpio_set_function(thrusterPins[i], GPIO_FUNC_PWM);
        uint8_t slice_num = pwm_gpio_to_slice_num(thrusterPins[i]);
        pwm_set_clkdiv(slice_num, CLOCK_DIV);
        pwm_set_wrap(slice_num, PWM_WRAP);
        pwm_set_enabled(slice_num, true);

        // init all thrusters to 1500us
        pwm_set_gpio_level(thrusterPins[i], PWM_WRAP / 10 * 1.5);
    }

    gpio_set_function(LED_PIN, GPIO_FUNC_PWM);
    uint8_t led_slice = pwm_gpio_to_slice_num(LED_PIN);
    pwm_set_clkdiv(led_slice, LED_CLOCK_DIV);
    pwm_set_wrap(led_slice, LED_WRAP);
    pwm_set_enabled(led_slice, true);
    pwm_set_gpio_level(LED_PIN, 0);

    gpio_set_function(MOTOR_PIN, GPIO_FUNC_PWM);
    uint8_t motor_slice = pwm_gpio_to_slice_num(MOTOR_PIN);
    pwm_set_clkdiv(motor_slice, LED_CLOCK_DIV);
    pwm_set_wrap(motor_slice, LED_WRAP);
    pwm_set_enabled(motor_slice, true);
    pwm_set_gpio_level(MOTOR_PIN, 0);

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    multicore_launch_core1(uartComm);

    for (;;) {
        if ((to_ms_since_boot(get_absolute_time()) - lastInputTime) > THRUSTER_TIMEOUT) {
            for (int i = 0; i < 6; i++) {
                targetThrust[i] = 127;
            }
            lastInputTime = to_ms_since_boot(get_absolute_time());
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