#include <Arduino.h>

float Kp = 0.01;
float Ki = 0.1;

float integral = 0;
float dt = 50e-6;

float vref = 12.0;
float vout = 0;
float duty = 0;

void setup() {
  Serial.begin(115200);
}

void loop() {

  if (Serial.available()) {
    vout = Serial.parseFloat();

    float error = vref - vout;

    integral += error * dt;

    duty = Kp*error + Ki*integral;

    // Saturation
    if(duty > 1.0){
      duty = 1.0;
      integral -= error*dt;   // anti-windup
    }

    if(duty < 0.0){
      duty = 0.0;
      integral -= error*dt;
    }

    Serial.println(duty,4);
  }
}