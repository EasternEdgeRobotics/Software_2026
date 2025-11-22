import { exec } from 'child_process';
import ROSLIB from 'roslib';

let servo_pwm = 700;
let lastSetPwmTime = 0;
const PWM_THROTTLE_INTERVAL = 100; // in milliseconds
const PWM_MIN = 500;
const PWM_MAX = 2500;
const FRONT_CAMERA = true;

var ros = new ROSLIB.Ros({
  url: 'ws://192.168.137.250:9090'
});

ros.on('connection', function () {
  console.log('Connected to websocket server.');
});

ros.on('error', function (error) {
  console.log('Error connecting to websocket server: ', error);
});

ros.on('close', function () {
  console.log('Connection to websocket server closed.');
});

var listener = new ROSLIB.Topic({
  ros: ros,
  name: '/pilot_input',
  messageType: 'eer_interfaces/msg/PilotInput'
});

listener.subscribe(function (message) {

  // Allow the user to set the angle directly
  if (FRONT_CAMERA)
  {
    if (message.front_servo_angle >= 0 && message.front_servo_angle <= 180)
    {
      servo_pwm = PWM_MIN + ((message.front_servo_angle / 180) * (PWM_MAX - PWM_MIN));
    }
    if (message.turn_front_servo_cw) {
      servo_pwm -= 3;
    }
    if (message.turn_front_servo_ccw) {
      servo_pwm += 3;
    }
  } 
  else if (!FRONT_CAMERA)
  {
    if (message.back_servo_angle >= 0 && message.back_servo_angle <= 180)
    {
      servo_pwm = PWM_MIN + ((message.back_servo_angle / 180) * (PWM_MAX - PWM_MIN));
    }
    if (message.turn_back_servo_cw) {
      servo_pwm -= 3;
    }
    if (message.turn_back_servo_ccw) {
      servo_pwm += 3;
    }
  }

  // Clamp the servo_pwm value between 500 and 2500
  servo_pwm = Math.max(PWM_MIN, Math.min(PWM_MAX, servo_pwm));

  throttledSetPwm(servo_pwm);
});

function set_pwm(value) {
//  console.log('Setting PWM to:', value);
  exec(`pigs s 12 ${value}`);
}

function throttledSetPwm(value) {
  const now = Date.now();
  if (now - lastSetPwmTime >= PWM_THROTTLE_INTERVAL) {
    lastSetPwmTime = now;
    set_pwm(value);
  }
}

