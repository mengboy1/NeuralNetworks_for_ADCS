/*
===Hardware===
- Arduino Uno R3
- MPU-6050 /GY-521
- DC MOTOR
- L293D

==Connections==
-GND - GND
-VCC - VCC
-SDA - Pin A4
-SCL - Pin A5
-INT - Pin 2 // this pin allows for the use of DMP (Digital Motion Processor) onboard the MPU-6050 chip. This significantly simplifies the process of obtaining filtered rotation data from the sensor

===Software===
- Arduino IDE v1.8.42
- Arduino Wire library
- I2Cdev.h --> https://github.com/jrowberg/i2cdevlib
- MPU6050.h -->  https://github.com/jrowberg/i2cdevlib
 */

/* NOTE!!!!!!!!!!
Se muestran las lecturas obtenidas con varios métodos para determinar cual es preferible usar. Los que no se usen se podrán borrar.
Funcionan:    - ángulo de giro del acelerómetro
              - datos de aceleración, en g (1g = 9.81)
              - ángulo de giro obtenido usando el DMP integrado en el MPU6050
No funcionan: - ángulo de giro del giroscopio
                --> ángulo de giro con filtro High Pass y Low Pass */

/***********************  INCLUDE LIBRARIES ************************/
#include<I2Cdev.h>
#include <math.h>
#include <MPU6050_6Axis_MotionApps20.h>               // esta libreria incluye las funciones para usar el DMP
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    #include <Wire.h>                                 // incluir la libreria incluida de Arduino Wire.h de manera que no interfiera con MPU6050_6Axis_MotionApps20.h
#endif

/***********************  Define parameters ************************/
//const int mpuAddress = 0x68;  // Puede ser 0x68 o 0x69
//MPU6050 mpu(mpuAddress);
MPU6050 mpu;

long accelX, accelY, accelZ;

float gForceX, gForceY, gForceZ;
int16_t gyroX, gyroY, gyroZ, gyroRateX, gyroRateY, gyroRateZ, accX, accY, accZ;
float gyroAngleX, gyroAngleY, gyroAngleZ, accAngleX, accAngleY, accAngleZ; // double //gyroAngleX=0
float currentAngleX, currentAngleY, currentAngleZ, prevAngleX=0, prevAngleY=0, prevAngleZ=0, error, prevError=0, errorSum=0; //
unsigned long currTime, prevTime=0, loopTime;
float dt;

// definir parámetros del DMP // no todos son necesarios
bool dmpReady = false;  // set true if DMP init was successful
uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
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

#define sampleTime 0.005

/***********************  Define connections ************************/
//DMP connection (Sensor) // El MPU6050 se conecta al pin 2 (digital) del Arduino para activar la funcionalidad del DMP
#define INTERRUPT_PIN 2

// Motor A connections
#define enA 9
#define in1 8
#define in2 7
// Motor B connections
#define enB 3
#define in3 5
#define in4 4

void dmpDataReady() {
    mpuInterrupt = true;
}

/******************************************************************
* Void Setup
******************************************************************/
void setup() {
  //
  #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
        Wire.begin();
        Wire.setClock(400000); // 400kHz I2C clock. Comment this line if having compilation difficulties
    #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
        Fastwire::setup(400, true);
    #endif
    //
  mpu.initialize();

/***********************  Define offsets ************************/
  mpu.setXAccelOffset(-2842); // from calibration sketch
  mpu.setYAccelOffset(-21); // from calibration sketch
  mpu.setZAccelOffset(1088); // from calibration sketch

  mpu.setXGyroOffset(25); // from calibration sketch
  mpu.setYGyroOffset(-24); // from calibration sketch
  mpu.setZGyroOffset(5); // from calibration sketch

  Serial.begin(115200);
  Wire.begin();

  // Set all the motor control pins to outputs
  pinMode(enA, OUTPUT);   // MOTOR 1
  pinMode(enB, OUTPUT);   // MOTOR 2
  pinMode(in1, OUTPUT);   // MOTOR 1
  pinMode(in2, OUTPUT);   // MOTOR 1
  pinMode(in3, OUTPUT);   // MOTOR 2
  pinMode(in4, OUTPUT);   // MOTOR 2

  // Turn off motors - Initial state
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
  digitalWrite(in3, LOW);
  digitalWrite(in4, LOW);

   // Iniciar MPU6050
  Serial.println(F("Initializing I2C devices..."));
  mpu.initialize();
  pinMode(INTERRUPT_PIN, INPUT);

  // Comprobar  conexion
  Serial.println(F("Testing device connections..."));
  Serial.println(mpu.testConnection() ? F("MPU6050 connection successful") : F("MPU6050 connection failed"));

  // Iniciar DMP
  Serial.println(F("Initializing DMP..."));
  devStatus = mpu.dmpInitialize();

  // Valores de calibracion
  mpu.setXGyroOffset(24);
  mpu.setYGyroOffset(-24);
  mpu.setZGyroOffset(5);
  mpu.setXAccelOffset(-2833);
  mpu.setYAccelOffset(-108);
  mpu.setZAccelOffset(1088);

  // Activar DMP
  if (devStatus == 0) {
      Serial.println(F("Enabling DMP..."));
      mpu.setDMPEnabled(true);

      // Activar interrupcion
      attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), dmpDataReady, RISING);
      mpuIntStatus = mpu.getIntStatus();

      Serial.println(F("DMP ready! Waiting for first interrupt..."));
      dmpReady = true;

      // get expected DMP packet size for later comparison
      packetSize = mpu.dmpGetFIFOPacketSize();
  } else {
      // ERROR!
      // 1 = initial memory load failed
      // 2 = DMP configuration updates failed
      // (if it's going to break, usually the code will be 1)
      Serial.print(F("DMP Initialization failed (code "));
      Serial.print(devStatus);
      Serial.println(F(")"));
  }

}

/******************************************************************
* Void Loop
******************************************************************/
void loop() {
  setupMPU();
  mpu.getAcceleration(&accX, &accY, &accZ);
  mpu.getRotation(&gyroX, &gyroY, &gyroZ);
  recordAccelGryoRegisters();
  complementaryFilter();
  digitalmotionprocessor();s
  directionControl();
  printData();
}

/******************************************************************
* setupMPU
******************************************************************/
void setupMPU(){

// count time
  currTime = millis();
  loopTime = currTime - prevTime;
  prevTime = currTime;

// map accelerations
  accX = map(accX, -17000, 17000, -1500, 1500);
  accY = map(accY, -17000, 17000, -1500, 1500);
  accZ = map(accZ, -17000, 17000, -1500, 1500);
}

/******************************************************************
* recordAcceGyrolRegisters
******************************************************************/
void recordAccelGryoRegisters() {
    // Calculate Angle of Inclination
/* atan2(y,z) function gives the angle in radians between the positive z-axis of a plane
and the point given by the coordinates (z,y) on that plane, with positive sign for
counter-clockwise angles (right half-plane, y > 0), and negative sign for clockwise
angles (left half-plane, y < 0). */
  accAngleX = atan(accY / sqrt(pow(accX, 2) + pow(accZ, 2)))*RAD_TO_DEG; //accAngleX = atan2(accX, accZ)*RAD_TO_DEG;  //  accAngleX = atan2(accY, accZ)*RAD_TO_DEG;
  accAngleY = atan(-accX / sqrt(pow(accY, 2) + pow(accZ, 2)))*RAD_TO_DEG; //accAngleY = atan2(accY, accZ)*RAD_TO_DEG; //   accAngleX = atan2(accY, accZ)*RAD_TO_DEG;
  accAngleZ = atan2(accY, accX)*RAD_TO_DEG; // atan(accY / sqrt(pow(accZ, 2) + pow(accX, 2)))*RAD_TO_DEG; // //  accAngleX = atan2(accY, accZ)*RAD_TO_DEG;

  gyroRateX = map(gyroX, -32776, 32776, -250, 250);
  gyroRateY = map(gyroY, -32776, 32776, -250, 250);
  gyroRateZ = map(gyroZ, -32776, 32776, -250, 250);

  //gyroAngleX = gyroAngleX + (float)gyroRateX*loopTime/1000;
  gyroAngleX = (float)gyroRateX*sampleTime;
  gyroAngleY = (float)gyroRateY*sampleTime;
  gyroAngleZ = (float)gyroRateZ*sampleTime;

  // Calculate Acceleration
  gForceX = accX / 16388.0;  // 16388 is the value that the accel would show when subject to 1g acceleration
  gForceY = accY / 16388.0;
  gForceZ = accZ / 16388.0;

}

/******************************************************************
* complementaryFilter
******************************************************************/
void complementaryFilter() {
/*We have two measurements of the angle from two different sources. The measurement from accelerometer gets
 * affected by sudden horizontal movements and the measurement from gyroscope gradually drifts away from actual value.
 * In other words, the accelerometer reading gets affected by short duration signals and the gyroscope reading by long
 * duration signals.  */

/* HPF --> gyroscope, LPF --> accelerometer to filter out the drift and noise from the measurement.
0.9934 and 0.0066 are filter coefficients for a filter time constant of 0.75s. The low pass filter allows any signal longer
than this duration to pass through it and the high pass filter allows any signal shorter than this duration to pass through.
The response of the filter can be tweaked by picking the correct time constant. Lowering the time constant will allow more
horizontal acceleration to pass through. */
  dt = (millis() - prevTime) / 1000.0;
  prevTime = millis();

  currentAngleX = 0.98*(prevAngleX + (gyroX / 131)*dt) + 0.02*accAngleX; // 0.98*(prevAngleX + gyroAngleX) + 0.02*(accAngleX);
  currentAngleY = 0.98*(prevAngleY + (gyroY / 131)*dt) + 0.02*accAngleY; // 0.98*(prevAngleY + gyroAngleY) + 0.02*(accAngleY);
  currentAngleZ = 0.98*(prevAngleZ + (gyroZ / 131)*dt) + 0.02*accAngleZ; // 0.98*(prevAngleZ + gyroAngleZ) + 0.02*(accAngleZ);
}

/******************************************************************
* digitalmotionprocessor
******************************************************************/
/* This method processes the data using the integrated DMP on the sensor, resulting in less processing done by the Arduino.
It is more precise than using the complementary filter according to some sources. */

void digitalmotionprocessor() {
    if (!dmpReady) return;

    // Ejecutar mientras no hay interrupcion
    while (!mpuInterrupt && fifoCount < packetSize) {
        // AQUI EL RESTO DEL CODIGO DE TU PROGRRAMA
    }

    mpuInterrupt = false;
    mpuIntStatus = mpu.getIntStatus();

    // Obtener datos del FIFO
    fifoCount = mpu.getFIFOCount();

    // Controlar overflow
    if ((mpuIntStatus & 0x10) || fifoCount == 1024) {
        mpu.resetFIFO();
        Serial.println(F("FIFO overflow!"));
    }
    else if (mpuIntStatus & 0x02) {
        // wait for correct available data length, should be a VERY short wait
        while (fifoCount < packetSize) fifoCount = mpu.getFIFOCount();

        // read a packet from FIFO
        mpu.getFIFOBytes(fifoBuffer, packetSize);

        // track FIFO count here in case there is > 1 packet available
        // (this lets us immediately read more without waiting for an interrupt)
        fifoCount -= packetSize;
}

    // Yaw, Pitch, Roll //  pitch y, roll x, yaw z
    mpu.dmpGetQuaternion(&q, fifoBuffer);
    mpu.dmpGetGravity(&gravity, &q);
    mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
    // Mostrar aceleracion
    mpu.dmpGetQuaternion(&q, fifoBuffer);
    mpu.dmpGetAccel(&aa, fifoBuffer);
    mpu.dmpGetGravity(&gravity, &q);
    mpu.dmpGetLinearAccel(&aaReal, &aa, &gravity);
}

/******************************************************************
* directionControl
******************************************************************/
void directionControl() {

/* ---------- Control X-direction ----------*/
    if(accAngleX > 0){ //  accX
    if(accAngleX < 255){
      //Serial.println(accX);
      analogWrite(in2,accAngleX);
      analogWrite(enA, 255);
      digitalWrite(in1, HIGH);
      digitalWrite(in2, LOW);
      //delay(2000);
    }
    else{
      //Serial.println("+255");
      analogWrite(in2,255);
      analogWrite(enA, 0);
      digitalWrite(in1, LOW);
      digitalWrite(in2, LOW);
    }

    }
    if(accAngleX < 0){
    if(accAngleX > -255){
      //Serial.println(accX);
      analogWrite(in1, accAngleX-accAngleX-accAngleX);
      analogWrite(enA, 255);
      digitalWrite(in1, LOW);
      digitalWrite(in2, HIGH);
    }
    else{
      analogWrite(enA, 0);
      //Serial.println("-255");
      analogWrite(in1, 255);
      digitalWrite(in1, LOW);
      digitalWrite(in2, LOW);
    }
    }

/* ---------- Control Y-direction ----------*/
    if(accAngleY > 0){ //  accY
    if(accAngleY < 255){
      analogWrite(enB, 255);
      //Serial.println(accY);
      analogWrite(in4,accAngleY);
      digitalWrite(in3, HIGH);
      digitalWrite(in4, LOW);
      //delay(2000);
      }
    else{
      analogWrite(enB, 0);
      //Serial.println("+255");
      analogWrite(in4,255);
      digitalWrite(in3, LOW);
      digitalWrite(in4, LOW);
    }

    }
    if(accAngleY < 0){
    if(accAngleY > -255){
      analogWrite(enB, 255);
      //Serial.println(accY);
      analogWrite(in3, accAngleY-accAngleY-accAngleY);
      digitalWrite(in3, LOW);
      digitalWrite(in4, HIGH);
    }
    else{
      analogWrite(enB, 255);
      //Serial.println("-255");
      analogWrite(in3, 255);
      digitalWrite(in3, LOW);
      digitalWrite(in4, LOW);
    }
    }
}

/******************************************************************
* printData
******************************************************************/
void printData() {        /* PRINT DATA */
  /* Se muestran las lecturas obtenidas con métodos distintos para determinar cual es preferible usar.
  Funcionan:    - ángulo de giro del acelerómetro
                - datos de aceleración, en g (1g = 9.81)
                - ángulo de giro obtenido usando el DMP integrado en el MPU6050
  No funcionan: - ángulo de giro del giroscopio
                  --> ángulo de giro con filtro High Pass y Low Pass */

// Llamar subrutinas, seguramente innecesario
  complementaryFilter();
  digitalmotionprocessor();

//raw data from accelerometer
//  Serial.print(" acc (RAW): ");
//  Serial.print (" X = ");
//  Serial.print( accX);
//  Serial.print (" Y = ");
//  Serial.print( accY);
//  Serial.print (" Z = ");
//  Serial.print( accZ);

// Mostrar lectura ángulo de giro del acelerómetro // FUNCIONA BIEN pero tiene los problemas relacionados con el "drift"
  Serial.println (" ");
  Serial.print(" Angle, accel (deg): ");
  Serial.print (" X = ");
  Serial.print( accAngleX);
  Serial.print (" Y = ");
  Serial.print( accAngleY);
  Serial.print (" Z = ");
  Serial.print( accAngleZ);

  Serial.println (" ");
// Mostrar lectura ángulo de giro del giroscopio // NO FUNCIONA da como output solo 0s, idk
  Serial.print(" Angle, gyro (deg): ");
  Serial.print (" X = ");
  Serial.print( gyroAngleX);
  Serial.print (" Y = ");
  Serial.print( gyroAngleY);
  Serial.print (" Z = ");
  Serial.print( gyroAngleZ);

  Serial.println (" ");
  Serial.println (" ");

// Mostrar lectura ángulo de giro filtrada usando void complementaryFilter // como depende del giroscopio la lectura no sale bien
  Serial.print(" Angle, filtered (deg): ");
  Serial.print (" X = ");
  Serial.print( currentAngleX);
  Serial.print (" Y = ");
  Serial.print( currentAngleY);
  Serial.print (" Z = ");
  Serial.print( currentAngleZ);

  Serial.println (" ");

// Mostar lectura aceleración (datos del ácelerómetro)  // FUNCIONA BIEN
  Serial.print(" Accel (g): ");
  Serial.print(" X=");
  Serial.print(gForceX);
  Serial.print(" Y=");
  Serial.print(gForceY);
  Serial.print(" Z=");
  Serial.println(gForceZ);

  Serial.println (" ");

  Serial.println (" ---------------------------------------------------------- ");
// Mostar los datos obtenidos usando el DMP integrado en el MPU6050 // TARDAN ENTRE 10-15s EN ESTABILIZARSE

// Mostrar Yaw, Pitch, Roll //  pitch y, roll x, yaw z // hay que confirmar que eje es que // SALE BIEN
    Serial.println (" ");
    Serial.print(" Angle, filtered (deg): ");
    // Serial.print (" ");
    Serial.print(" X ="); // Serial.print("\t");
    Serial.print(ypr[2] * 180/M_PI);
    Serial.print(" Y ="); //Serial.print("\t");
    Serial.print(ypr[1] * 180/M_PI);;
    Serial.print(" Z ="); // Serial.print("ypr\t");
    Serial.println(ypr[0] * 180/M_PI);

    Serial.println (" ");

//   // Mostrar aceleracion // Puede que esté bien pero no se en qué unidades está
   Serial.print (" Acceleration:");
   //Serial.print("areal\t"); // Serial.print("areal\t");
   Serial.print(" X =");
   Serial.print(aaReal.x);
//   Serial.print("\t");
   Serial.print(" Y =");
   Serial.print(aaReal.y);
//   Serial.print("\t");
   Serial.print(" Z =");
   Serial.print(aaReal.z);

   Serial.println (" ");

   Serial.println("**************************************************************");

   delay(100);
}