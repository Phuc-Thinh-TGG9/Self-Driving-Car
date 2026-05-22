#include <ESP32Servo.h>

Servo esc;
Servo steering;

const int ESC_PIN = 13;
const int SERVO_PIN = 14;

// Default values (Stop and Center)
int current_motor_pwm = 1500; 
int current_servo_angle = 90;

void setup() {
  Serial.begin(115200);
  
  // Attach ESC and Servo
  esc.attach(ESC_PIN, 1000, 2000); // Typical ESC PWM range
  steering.attach(SERVO_PIN, 500, 2500); // Typical Servo PWM range

  // Initialize values
  esc.writeMicroseconds(current_motor_pwm);
  steering.write(current_servo_angle);
  
  // Wait a bit for ESC to initialize
  delay(3000);
  Serial.println("ESP32 Ready. Waiting for commands...");
}

void loop() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    // Command format expected: "M:1500,S:90"
    int m_index = command.indexOf("M:");
    int s_index = command.indexOf(",S:");

    if (m_index != -1 && s_index != -1) {
      String m_val_str = command.substring(m_index + 2, s_index);
      String s_val_str = command.substring(s_index + 3);

      int motor_pwm = m_val_str.toInt();
      int servo_angle = s_val_str.toInt();

      // Constrain values to be safe
      motor_pwm = constrain(motor_pwm, 1000, 2000);
      servo_angle = constrain(servo_angle, 0, 180);

      // Write to hardware
      esc.writeMicroseconds(motor_pwm);
      steering.write(servo_angle);

      // Reply back for debugging
      Serial.print("ACK M:");
      Serial.print(motor_pwm);
      Serial.print(" S:");
      Serial.println(servo_angle);
    }
  }
}
