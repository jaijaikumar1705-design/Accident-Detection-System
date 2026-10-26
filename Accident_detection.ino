v#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

Adafruit_MPU6050 mpu;
TinyGPSPlus gps;

// GPS: UART2
HardwareSerial GPS(2);

// GSM: UART1
HardwareSerial GSM(1);

// ESP32 pin configuration
#define GPS_RX 16
#define GPS_TX 17

#define GSM_RX 26
#define GSM_TX 27

// Accident detection threshold in m/s^2.
// Tune this value through controlled testing.
const float ACCIDENT_THRESHOLD = 30.0;

// SMS recipient: replace with the emergency contact number.
const char PHONE_NUMBER[] = "+91XXXXXXXXXX";

bool accidentDetected = false;
bool alertSent = false;

unsigned long accidentTime = 0;
const unsigned long GPS_WAIT_TIME = 30000;

void sendSMS(String message) {
  GSM.println("AT+CMGF=1");
  delay(1000);

  GSM.print("AT+CMGS=\"");
  GSM.print(PHONE_NUMBER);
  GSM.println("\"");
  delay(1000);

  GSM.print(message);
  delay(500);

  GSM.write(26);  // Ctrl+Z: send SMS
  delay(5000);

  Serial.println("SMS command submitted.");
}

void setup() {
  Serial.begin(115200);

  // Initialize GPS and GSM UARTs
  GPS.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  GSM.begin(9600, SERIAL_8N1, GSM_RX, GSM_TX);

  // Initialize I2C
  Wire.begin(21, 22);

  // Initialize MPU6050
  if (!mpu.begin()) {
    Serial.println("MPU6050 not detected!");
    while (true) {
      delay(1000);
    }
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_16_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("Accident Detection System Started");

  delay(2000);

  // Check GSM communication
  GSM.println("AT");
  delay(1000);
  GSM.println("AT+CMGF=1");
  delay(1000);
}

void loop() {
  // Continuously process GPS data
  while (GPS.available()) {
    gps.encode(GPS.read());
  }

  // Read acceleration
  sensors_event_t accel, gyro, temp;
  mpu.getEvent(&accel, &gyro, &temp);

  float ax = accel.acceleration.x;
  float ay = accel.acceleration.y;
  float az = accel.acceleration.z;

  // Total acceleration magnitude
  float totalAcceleration = sqrt(
    ax * ax + ay * ay + az * az
  );

  Serial.print("Acceleration: ");
  Serial.print(totalAcceleration);
  Serial.println(" m/s^2");

  // Detect a possible impact
  if (totalAcceleration > ACCIDENT_THRESHOLD &&
      !accidentDetected) {

    accidentDetected = true;
    accidentTime = millis();

    Serial.println("Possible accident detected!");
    Serial.println("Waiting for GPS coordinates...");
  }

  // Wait for a GPS fix, up to 30 seconds
  if (accidentDetected && !alertSent) {

    if (gps.location.isValid()) {

      String message = "ACCIDENT ALERT! "
                       "Possible vehicle accident detected. "
                       "Location: https://maps.google.com/?q=";

      message += String(gps.location.lat(), 6);
      message += ",";
      message += String(gps.location.lng(), 6);

      sendSMS(message);

      alertSent = true;

      Serial.println("Accident alert processed.");
    }
    else if (millis() - accidentTime >= GPS_WAIT_TIME) {

      String message = "ACCIDENT ALERT! "
                       "Possible vehicle accident detected. "
                       "GPS location unavailable.";

      sendSMS(message);

      alertSent = true;

      Serial.println("Alert sent without GPS coordinates.");
    }
  }

  delay(100);
}
