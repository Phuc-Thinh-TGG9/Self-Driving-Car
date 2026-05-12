#include <Arduino.h>

#define CUSTOM_SETTINGS
#define INCLUDE_GAMEPAD_MODULE

#include <DabbleESP32.h>
#include <ESP32Servo.h>

// ======================================================
// SERVO + ESC
// ======================================================

Servo steeringServo;
Servo escServo;

// ======================================================
// PIN CONFIG
// ======================================================

const int STEER_PIN = 14;
const int ESC_PIN   = 13;

// ======================================================
// SERVO CONFIG
// ======================================================

const int SERVO_CENTER = 90;

const int SERVO_LEFT_MAX  = 65;
const int SERVO_RIGHT_MAX = 130;

// ======================================================
// ESC CONFIG
// ======================================================

const int ESC_NEUTRAL = 1500;

const int ESC_MIN = 1000;
const int ESC_MAX = 2000;

// ======================================================
// SAFE SPEED LIMIT
// ======================================================

const int ESC_FORWARD_MAX = 1900;
const int ESC_REVERSE_MIN = 1200;

// ======================================================
// SMOOTH CONFIG
// ======================================================

const int ESC_RAMP_UP   = 8;
const int ESC_RAMP_DOWN = 12;

const int STEER_RAMP = 3;

// ======================================================
// JOYSTICK CONFIG
// ======================================================

const int JOYSTICK_MIN = -7;
const int JOYSTICK_MAX = 7;

const int JOYSTICK_DEADZONE = 2;

// ======================================================
// CONTROL VALUES
// ======================================================

int targetESC = ESC_NEUTRAL;
int currentESC = ESC_NEUTRAL;

int reverseState = 0;          // 0: Đang tiến/Đứng im, 1: Bóp phanh, 2: Nhả về N, 3: Đang lùi
unsigned long reverseTimer = 0;
unsigned long lastYZeroTime = 0;

int targetSteer = SERVO_CENTER;
int currentSteer = SERVO_CENTER;

// ======================================================
// SPEED MODE
// ======================================================

float speedScale = 0.25;

// ======================================================
// STEP FUNCTION
// ======================================================

int stepToward(
    int currentValue,
    int targetValue,
    int stepValue
) {
    if(currentValue < targetValue) {
        return min(currentValue + stepValue, targetValue);
    }

    if(currentValue > targetValue) {
        return max(currentValue - stepValue, targetValue);
    }

    return currentValue;
}

// ======================================================
// SETUP
// ======================================================

void setup() {

    Serial.begin(115200);

    // ==================================================
    // START DABBLE BLUETOOTH
    // ==================================================

    Dabble.begin("PuTi_Delivery");

    // ==================================================
    // ATTACH SERVO + ESC
    // ==================================================

    steeringServo.attach(STEER_PIN);

    escServo.attach(
        ESC_PIN,
        ESC_MIN,
        ESC_MAX
    );

    // ==================================================
    // DEFAULT POSITION
    // ==================================================

    steeringServo.write(SERVO_CENTER);
    escServo.writeMicroseconds(ESC_NEUTRAL);

    // ESC ARM TIME
    delay(3000);

    Serial.println("=================================");
    Serial.println("PuTi Delivery Robot Ready");
    Serial.println("Joystick Control Mode");
    Serial.println("Triangle = 35%");
    Serial.println("Square   = 50%");
    Serial.println("Circle   = 75%");
    Serial.println("Select   = 100%");
    Serial.println("Diagonal joystick enabled");
    Serial.println("=================================");
}

// ======================================================
// LOOP
// ======================================================

void loop() {

    // ==================================================
    // READ DABBLE INPUT
    // ==================================================

    Dabble.processInput();

    // ==================================================
    // SPEED MODE BUTTONS
    // ==================================================

    if(GamePad.isTrianglePressed()) {
        speedScale = 0.35;
    }

    if(GamePad.isSquarePressed()) {
        speedScale = 0.50;
    }

    if(GamePad.isCirclePressed()) {
        speedScale = 0.75;
    }

    if(GamePad.isSelectPressed()) {
        speedScale = 1.00;
    }

    // ==================================================
    // READ JOYSTICK
    // ==================================================

    int x = GamePad.getx_axis();
    int y = GamePad.gety_axis();

    // ==================================================
    // JOYSTICK DEADZONE
    // ==================================================

    if(abs(x) < JOYSTICK_DEADZONE) {
        x = 0;
    }

    if(abs(y) < JOYSTICK_DEADZONE) {
        y = 0;
    }

    // ==================================================
    // LIMIT JOYSTICK VALUES
    // ==================================================

    x = constrain(
        x,
        JOYSTICK_MIN,
        JOYSTICK_MAX
    );

    y = constrain(
        y,
        JOYSTICK_MIN,
        JOYSTICK_MAX
    );

    // ==================================================
    // STEERING CONTROL
    // Joystick X:
    // -7 = left
    //  0 = center
    //  7 = right
    // ==================================================

    targetSteer = map(
        x,
        JOYSTICK_MIN,
        JOYSTICK_MAX,
        SERVO_LEFT_MAX,
        SERVO_RIGHT_MAX
    );

    targetSteer = constrain(
        targetSteer,
        SERVO_LEFT_MAX,
        SERVO_RIGHT_MAX
    );

    // ==================================================
    // THROTTLE CONTROL VÀ LOGIC CHỐNG KẸT PHANH (NHẤP NHẢ KHÔNG DELAY)
    // ==================================================

    targetESC = ESC_NEUTRAL;

    if(y > 0) {
        reverseState = 0; // Đang tiến thì reset chu trình lùi
        
        int throttleInput = y; 
        
        int maxForwardDelta = (ESC_FORWARD_MAX - ESC_NEUTRAL) * speedScale;
        int delta = map(throttleInput, 0, JOYSTICK_MAX, 0, maxForwardDelta);
        
        targetESC = ESC_NEUTRAL + delta;
    }

    else if(y < 0) {
        int throttleInput = -y;
        
        int maxReverseDelta = (ESC_NEUTRAL - ESC_REVERSE_MIN) * speedScale;
        int delta = map(throttleInput, 0, JOYSTICK_MAX, 0, maxReverseDelta);
        
        int finalReverseESC = ESC_NEUTRAL - delta;

        // Bắt đầu chu trình nhấp nhả lùi (KHÔNG sử dụng hàm delay)
        if(reverseState == 0) {
            reverseState = 1;         // Bước 1: Bắt đầu bóp phanh
            reverseTimer = millis();  // Bắt đầu bấm giờ
        }

        if(reverseState == 1) {
            targetESC = ESC_MIN; // Gửi tín hiệu lùi sâu nhất để mạch ESC hiểu đây là phanh
            if(millis() - reverseTimer > 200) { 
                reverseState = 2; // Qua 200ms -> Chuyển sang bước nhả N
                reverseTimer = millis();
            }
        }
        else if(reverseState == 2) {
            targetESC = ESC_NEUTRAL; // Trả về N giống như thao tác nhả cò bằng tay ròng
            if(millis() - reverseTimer > 200) { 
                reverseState = 3; // Qua tiếp 200ms -> Chuyển sang bước Lùi thật sự
            }
        }
        else if(reverseState == 3) {
            targetESC = finalReverseESC; // Đã qua bước nhấp nhả -> xuất tín hiệu lùi tuỳ biến theo nút bấm
        }
    }
    
    else { // y == 0
        // Khắc phục nhiễu báo chập chờn Dabble khi người dùng đè kéo nhưng đôi lúc app tuột data báo y = 0
        // Chỉ reset trạng thái lùi nếu người dùng thực sự thả ngón tay (y=0) hơn 300ms
        if(millis() - lastYZeroTime > 300) {
            reverseState = 0; 
        }
    }

    // Đánh dấu thời gian có cảm ứng Y gửi tới, để không bị đếm nhầm 300ms
    if(y != 0) {
        lastYZeroTime = millis();
    }

    targetESC = constrain(
        targetESC,
        ESC_REVERSE_MIN,
        ESC_FORWARD_MAX
    );

    // ==================================================
    // ESC SMOOTHING
    // ==================================================

    int escRamp =
        (targetESC > currentESC)
        ? ESC_RAMP_UP
        : ESC_RAMP_DOWN;

    // Trong 400ms lúc đang nhấp nhả (Bóp/Thả), đòi hỏi cắt mượt, nhảy thẳng giá trị để dứt khoát ép ESC nhận tín hiệu
    if(reverseState == 1 || reverseState == 2) {
        currentESC = targetESC;
    }
    else {
        // Chỉ làm mượt khi Tiến hoặc lúc Lùi thật sự (reverseState = 0 hoặc 3)
        if(targetESC == ESC_NEUTRAL) {
            escRamp = ESC_RAMP_DOWN * 3; // Trả về 1500 nhanh dần nhưng không cắt đột ngột
        }

        currentESC = stepToward(
            currentESC,
            targetESC,
            escRamp
        );
    }

    currentESC = constrain(
        currentESC,
        ESC_REVERSE_MIN,
        ESC_FORWARD_MAX
    );

    // ==================================================
    // STEERING SMOOTHING
    // ==================================================

    currentSteer = stepToward(
        currentSteer,
        targetSteer,
        STEER_RAMP
    );

    currentSteer = constrain(
        currentSteer,
        SERVO_LEFT_MAX,
        SERVO_RIGHT_MAX
    );

    // ==================================================
    // OUTPUT TO ESC + SERVO
    // ==================================================

    escServo.writeMicroseconds(currentESC);
    steeringServo.write(currentSteer);

    // ==================================================
    // DEBUG
    // ==================================================

    Serial.print("X: ");
    Serial.print(x);

    Serial.print(" | Y: ");
    Serial.print(y);

    Serial.print(" | ESC: ");
    Serial.print(currentESC);

    Serial.print(" | STEER: ");
    Serial.print(currentSteer);

    Serial.print(" | SPEED: ");
    Serial.println(speedScale);

    delay(20);
}