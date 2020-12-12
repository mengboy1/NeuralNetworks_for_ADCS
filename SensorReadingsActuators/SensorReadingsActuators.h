/*
===Hardware===
- Arduino Uno R3
- MPU-6050 /GY-521
- DC MOTOR
- L293D

===Software===
- Arduino IDE v1.8.42
- Arduino Wire library
- I2Cdev.h --> https://github.com/jrowberg/i2cdevlib
- MPU6050.h -->  https://github.com/jrowberg/i2cdevlib
 */

#ifndef SensorReadingsActuators_h
#define SensorReadingsActuators_h


#include <I2Cdev.h>
#include <math.h>
#include <MPU6050_6Axis_MotionApps20.h>               // esta libreria incluye las funciones para usar el DMP
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    #include <Wire.h>                                 // incluir la libreria incluida de Arduino Wire.h de manera que no interfiera con MPU6050_6Axis_MotionApps20.h
#endif
#include "Arduino.h"

/******************************************************************
* Classes
******************************************************************/

class Sensor{

  public:
  /***********************  Define connections ************************/
  //DMP connection (Sensor) // El MPU6050 se conecta al pin 2 (digital) del Arduino para activar la funcionalidad del DMP
  const int INTERRUPT_PIN = 2;

  /***********************  Define parameters ************************/
  //const int mpuAddress = 0x68;  // Puede ser 0x68 o 0x69
  //MPU6050 mpu(mpuAddress);
  MPU6050 mpu;

  //accelerations
  float accX, accY, accZ;

  //velocities
  float velX, velY, velZ;

  //angle
  float angleX, angleY, angleZ;

  //statevector
  float statevector[9];

  //time -> tbd

  // definir parámetros del DMP
  uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
  uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
  uint16_t fifoCount;     // count of all bytes currently in FIFO
  uint8_t fifoBuffer[64]; // FIFO storage buffer

  // definir Quaternion y vectores para las lecturas del DMP
  Quaternion q;           // [w, x, y, z]
  VectorInt16 aa;         // [x, y, z]
  VectorInt16 aaReal;     // [x, y, z]
  VectorInt16 aaWorld;    // [x, y, z]
  VectorFloat gravity;    // [x, y, z]
  float ypr[3];           // [yaw, pitch, roll]

  volatile bool mpuInterrupt = false;

  const float sampleTime = 0.005;

  /***********************  Define methods ************************/
  Sensor();
  void mpu_setup();
  void readFifoBuffer_();
  void readSensor();
};

class Actuator{

public:
  /***********************  Motor connections ************************/
  // Motor X connections
  const int enA = 9;
  const int in1 = 8;
  const int in2 = 7;
  // Motor Y connections
  const int enB = 3;
  const int in3 = 5;
  const int in4 = 4;
  // Motor Z connections
  const int enC = 10;
  const int in5 = 11;
  const int in6 = 12;

  float MotorIn[3]; //Vx, Vy, Vz

  /***********************  Motor Methods ************************/
  Actuator();
  /******************************************************************
  * directionControl
  ******************************************************************/
  void directionControl();

};

#endif
