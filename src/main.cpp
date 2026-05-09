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
    Serial.println("Triangle = 25%");
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
        speedScale = 0.25;
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
    // THROTTLE CONTROL WITH DIAGONAL COMPENSATION
    //
    // Vấn đề cũ:
    // Khi kéo joystick lên rồi nghiêng trái/phải,
    // giá trị Y có thể giảm về gần 0 nên xe mất tiến.
    //
    // Cách sửa:
    // Nếu joystick vẫn đang ở vùng tiến/lùi,
    // dùng max(abs(Y), abs(X)) làm độ lớn ga.
    //
    // Kết quả:
    // Lên + trái  = tiến và rẽ trái
    // Lên + phải  = tiến và rẽ phải
    // Ngang trái/phải = chỉ rẽ, không tiến
    // Xuống + trái/phải = lùi và rẽ
    // ==================================================

    targetESC = ESC_NEUTRAL;

    if(y > 0) {

        int throttleInput = max(
            y,
            abs(x)
        );

        throttleInput = constrain(
            throttleInput,
            0,
            JOYSTICK_MAX
        );

        int maxForwardDelta =
            (ESC_FORWARD_MAX - ESC_NEUTRAL) * speedScale;

        int delta = map(
            throttleInput,
            0,
            JOYSTICK_MAX,
            0,
            maxForwardDelta
        );

        targetESC = ESC_NEUTRAL + delta;
    }

    else if(y < 0) {

        int throttleInput = max(
            -y,
            abs(x)
        );

        throttleInput = constrain(
            throttleInput,
            0,
            JOYSTICK_MAX
        );

        int maxReverseDelta =
            (ESC_NEUTRAL - ESC_REVERSE_MIN) * speedScale;

        int delta = map(
            throttleInput,
            0,
            JOYSTICK_MAX,
            0,
            maxReverseDelta
        );

        targetESC = ESC_NEUTRAL - delta;
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

    currentESC = stepToward(
        currentESC,
        targetESC,
        escRamp
    );

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