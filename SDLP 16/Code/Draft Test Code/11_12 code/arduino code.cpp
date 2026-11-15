#include <Adafruit_MotorShield.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

Adafruit_MotorShield AFMSbot(0x61);
Adafruit_MotorShield AFMStop(0x60);

Adafruit_StepperMotor *stepperX = NULL;
Adafruit_StepperMotor *stepperY = NULL;
Adafruit_StepperMotor *stepperZ1 = NULL;
Adafruit_StepperMotor *stepperZ2 = NULL;

#define LED_PIN 30
#define BUTTON_PIN 31

LiquidCrystal_I2C lcd(0x27, 16, 2);

#define STEPS_PER_MM 10
#define PEN_UP_STEPS 0
#define PEN_DOWN_STEPS 50
#define SCALE 0.8
#define BASE_LETTER_SPACING 3
#define LINE_SPACING 12
#define STROKE_DELAY 5
#define HOME_X 0
#define HOME_Y 0
#define MAX_LINE_WIDTH 150

long currentX = 0;
long currentY = 0;
long currentZ = 0;

void displayMessage(String l1, String l2 = "") {
    Serial.println(l1);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(l1.substring(0, 16));
    if (l2.length()) {
        lcd.setCursor(0, 1);
        lcd.print(l2.substring(0, 16));
    }
}

bool initializeMotors() {
    Serial.println("Initializing motor shields...");

    if (!AFMSbot.begin()) {
        Serial.println("Could not find Motor Shield 1, check address 0x61");
        return false;
    }

    if (!AFMStop.begin()) {
        Serial.println("Could not find Motor Shield 2, check address 0x60");
        return false;
    }

    stepperX = AFMSbot.getStepper(200, 2);
    stepperY = AFMStop.getStepper(200, 1);
    stepperZ1 = AFMStop.getStepper(200, 2);
    stepperZ2 = AFMSbot.getStepper(200, 1);

    if (!stepperX || !stepperY || !stepperZ1 || !stepperZ2) {
        Serial.println("Error: One or more motors failed to initialize");
        return false;
    }

    Serial.println("All motors initialized successfully");
    return true;
}

void moveAxis(Adafruit_StepperMotor *motor, long steps, bool forward) {
    if (!motor) {
        Serial.println("Error: Motor not initialized!");
        return;
    }

    uint8_t direction = forward ? FORWARD : BACKWARD;

    for (long i = 0; i < abs(steps); i++) {
        motor->onestep(direction, DOUBLE);
        delay(STROKE_DELAY);
    }
}

void moveZ(long targetZ) {
    if (!stepperZ1 || !stepperZ2) {
        Serial.println("Error: Z-axis motors not initialized!");
        return;
    }

    long deltaZ = targetZ - currentZ;
    if (deltaZ == 0) return;

    bool direction = (deltaZ > 0);
    long steps = abs(deltaZ);

    for (long i = 0; i < steps; i++) {
        stepperZ1->onestep(direction ? FORWARD : BACKWARD, DOUBLE);
        stepperZ2->onestep(direction ? FORWARD : BACKWARD, DOUBLE);
        delay(STROKE_DELAY);
    }

    currentZ = targetZ;
}

void liftPen() {
    Serial.println("Lifting pen");
    moveZ(PEN_UP_STEPS);
}

void lowerPen() {
    Serial.println("Lowering pen");
    moveZ(PEN_DOWN_STEPS);
}

void moveToXY(float x, float y) {
    Serial.print("Moving to X:");
    Serial.print(x);
    Serial.print(" Y:");
    Serial.println(y);

    long targetX = x * STEPS_PER_MM;
    long targetY = y * STEPS_PER_MM;

    long deltaX = targetX - currentX;
    long deltaY = targetY - currentY;

    if (deltaX == 0 && deltaY == 0) return;

    long steps = max(abs(deltaX), abs(deltaY));

    float stepX = (float)deltaX / steps;
    float stepY = (float)deltaY / steps;

    for (long i = 0; i <= steps; i++) {
        long newX = currentX + (stepX * i);
        long newY = currentY + (stepY * i);

        if (newX != currentX) {
            moveAxis(stepperX, newX - currentX, newX > currentX);
            currentX = newX;
        }

        if (newY != currentY) {
            moveAxis(stepperY, newY - currentY, newY > currentY);
            currentY = newY;
        }
    }

    if (currentX != targetX) {
        moveAxis(stepperX, targetX - currentX, targetX > currentX);
    }
    if (currentY != targetY) {
        moveAxis(stepperY, targetY - currentY, targetY > currentY);
    }

    currentX = targetX;
    currentY = targetY;
}

void goHome() {
    Serial.println("Going home");
    liftPen();
    moveToXY(HOME_X, HOME_Y);
    currentX = 0;
    currentY = 0;
}

void resetSystem() {
    lcd.clear();
    lcd.print("RESETTING...");
    goHome();
    lcd.clear();
    lcd.print("INPUT REQUIRED");
    digitalWrite(LED_PIN, HIGH);
}

float getLetterWidth(char c) {
    switch (c) {
        case 'I':
            return 3 * SCALE;
        case 'J':
            return 6 * SCALE;
        case 'T':
            return 8 * SCALE;
        case 'F':
        case 'L':
            return 9 * SCALE;
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'G':
        case 'H':
        case 'K':
        case 'O':
        case 'P':
        case 'Q':
        case 'R':
        case 'S':
        case 'U':
        case 'V':
        case 'X':
        case 'Y':
        case 'Z':
        case 'N':
            return 10 * SCALE;
        case 'M':
        case 'W':
            return 12 * SCALE;
        case ' ':
            return 4 * SCALE;
        default:
            return 10 * SCALE;
    }
}

void drawLetterA(float off) {
    liftPen();
    moveToXY(off + 0 * SCALE, 0 * SCALE);
    lowerPen();
    moveToXY(off + 5 * SCALE, 10 * SCALE);
    moveToXY(off + 10 * SCALE, 0 * SCALE);
    liftPen();
    moveToXY(off + 2 * SCALE, 5 * SCALE);
    lowerPen();
    moveToXY(off + 8 * SCALE, 5 * SCALE);
    liftPen();
}
void drawLetterB(float off) {
    liftPen();
    moveToXY(off + 0 * SCALE, 0 * SCALE);
    lowerPen();
    moveToXY(off + 0 * SCALE, 10 * SCALE);
    moveToXY(off + 6 * SCALE, 10 * SCALE);
    moveToXY(off + 8 * SCALE, 8 * SCALE);
    moveToXY(off + 8 * SCALE, 6 * SCALE);
    moveToXY(off + 6 * SCALE, 5 * SCALE);
    moveToXY(off + 8 * SCALE, 4 * SCALE);
    moveToXY(off + 8 * SCALE, 2 * SCALE);
    moveToXY(off + 6 * SCALE, 0 * SCALE);
    moveToXY(off + 0 * SCALE, 0 * SCALE);
    liftPen();
}
void drawLetterC(float off) {
    liftPen();
    moveToXY(off + 10 * SCALE, 10 * SCALE);
    lowerPen();
    moveToXY(off + 0 * SCALE, 10 * SCALE);
    moveToXY(off + 0 * SCALE, 0 * SCALE);
    moveToXY(off + 10 * SCALE, 0 * SCALE);
    liftPen();
}
void drawLetterD(float off) {
    liftPen();
    moveToXY(off + 0 * SCALE, 0 * SCALE);
    lowerPen();
    moveToXY(off + 0 * SCALE, 10 * SCALE);
    moveToXY(off + 6 * SCALE, 10 * SCALE);
    moveToXY(off + 9 * SCALE, 7 * SCALE);
    moveToXY(off + 9 * SCALE, 3 * SCALE);
    moveToXY(off + 6 * SCALE, 0 * SCALE);
    moveToXY(off + 0 * SCALE, 0 * SCALE);
    liftPen();
}
void drawLetterE(float off) {
    liftPen();
    moveToXY(off + 10 * SCALE, 10 * SCALE);
    lowerPen();
    moveToXY(off + 0 * SCALE, 10 * SCALE);
    moveToXY(off + 0 * SCALE, 0 * SCALE);
    moveToXY(off + 10 * SCALE, 0 * SCALE);
    liftPen();
    moveToXY(off + 0 * SCALE, 5 * SCALE);
    lowerPen();
    moveToXY(off + 6 * SCALE, 5 * SCALE);
    liftPen();
}
void drawLetterF(float off) {
    liftPen();
    moveToXY(off + 0 * SCALE, 0 * SCALE);
    lowerPen();
    moveToXY(off + 0 * SCALE, 10 * SCALE);
    moveToXY(off + 10 * SCALE, 10 * SCALE);
    liftPen();
    moveToXY(off + 0 * SCALE, 5 * SCALE);
    lowerPen();
    moveToXY(off + 6 * SCALE, 5 * SCALE);
    liftPen();
}
void drawLetterG(float off) {
    liftPen();
    moveToXY(off + 10 * SCALE, 2 * SCALE);
    lowerPen();
    moveToXY(off + 10 * SCALE, 0 * SCALE);
    moveToXY(off + 2 * SCALE, 0 * SCALE);
    moveToXY(off + 0 * SCALE, 2 * SCALE);
    moveToXY(off + 0 * SCALE, 8 * SCALE);
    moveToXY(off + 2 * SCALE, 10 * SCALE);
    moveToXY(off + 10 * SCALE, 10 * SCALE);
    moveToXY(off + 10 * SCALE, 5 * SCALE);
    moveToXY(off + 6 * SCALE, 5 * SCALE);
    liftPen();
}
void drawLetterH(float off) {
    liftPen();
    moveToXY(off + 0 * SCALE, 0 * SCALE);
    lowerPen();
    moveToXY(off + 0 * SCALE, 10 * SCALE);
    liftPen();
    moveToXY(off + 10 * SCALE, 0 * SCALE);
    lowerPen();
    moveToXY(off + 10 * SCALE, 10 * SCALE);
    liftPen();
    moveToXY(off + 0 * SCALE, 5 * SCALE);
    lowerPen();
    moveToXY(off + 10 * SCALE, 5 * SCALE);
    liftPen();
}
void drawLetterI(float off) {
    liftPen();
    moveToXY(off + 0 * SCALE, 10 * SCALE);
    lowerPen();
    moveToXY(off + 10 * SCALE, 10 * SCALE);
    liftPen();
    moveToXY(off + 5 * SCALE, 10 * SCALE);
    lowerPen();
    moveToXY(off + 5 * SCALE, 0 * SCALE);
    liftPen();
    moveToXY(off + 0 * SCALE, 0 * SCALE);
    lowerPen();
    moveToXY(off + 10 * SCALE, 0 * SCALE);
    liftPen();
}
void drawLetterJ(float off) {
    liftPen();
    moveToXY(off + 10 * SCALE, 10 * SCALE);
    lowerPen();
    moveToXY(off + 10 * SCALE, 2 * SCALE);
    moveToXY(off + 8 * SCALE, 0 * SCALE);
    moveToXY(off + 2 * SCALE, 0 * SCALE);
    moveToXY(off + 0 * SCALE, 2 * SCALE);
    liftPen();
}
void drawLetterK(float off) {
    liftPen();
    moveToXY(off + 0 * SCALE, 0 * SCALE);
    lowerPen();
    moveToXY(off + 0 * SCALE, 10 * SCALE);
    liftPen();
    moveToXY(off + 10 * SCALE, 10 * SCALE);
    lowerPen();
    moveToXY(off + 0 * SCALE, 5 * SCALE);
    moveToXY(off + 10 * SCALE, 0 * SCALE);
    liftPen();
}
void drawLetterL(float off) {
    liftPen();
    moveToXY(off + 0 * SCALE, 10 * SCALE);
    lowerPen();
    moveToXY(off + 0 * SCALE, 0 * SCALE);
    moveToXY(off + 10 * SCALE, 0 * SCALE);
    liftPen();
}
void drawLetterM(float off) {
    liftPen();
    moveToXY(off + 0 * SCALE, 0 * SCALE);
    lowerPen();
    moveToXY(off + 0 * SCALE, 10 * SCALE);
    moveToXY(off + 5 * SCALE, 5 * SCALE);
    moveToXY(off + 10 * SCALE, 10 * SCALE);
    moveToXY(off + 10 * SCALE, 0 * SCALE);
    liftPen();
}
void drawLetterN(float off) {
    liftPen();
    moveToXY(off + 0 * SCALE, 0 * SCALE);
    lowerPen();
    moveToXY(off + 0 * SCALE, 10 * SCALE);
    moveToXY(off + 10 * SCALE, 0 * SCALE);
    moveToXY(off + 10 * SCALE, 10 * SCALE);
    liftPen();
}
void drawLetterO(float off) {
    liftPen();
    moveToXY(off + 5 * SCALE, 10 * SCALE);
    lowerPen();
    moveToXY(off + 0 * SCALE, 8 * SCALE);
    moveToXY(off + 0 * SCALE, 2 * SCALE);
    moveToXY(off + 5 * SCALE, 0 * SCALE);
    moveToXY(off + 10 * SCALE, 2 * SCALE);
    moveToXY(off + 10 * SCALE, 8 * SCALE);
    moveToXY(off + 5 * SCALE, 10 * SCALE);
    liftPen();
}
void drawLetterP(float off) {
    liftPen();
    moveToXY(off + 0 * SCALE, 0 * SCALE);
    lowerPen();
    moveToXY(off + 0 * SCALE, 10 * SCALE);
    moveToXY(off + 8 * SCALE, 10 * SCALE);
    moveToXY(off + 8 * SCALE, 6 * SCALE);
    moveToXY(off + 0 * SCALE, 6 * SCALE);
    liftPen();
}
void drawLetterQ(float off) {
    liftPen();
    moveToXY(off + 5 * SCALE, 10 * SCALE);
    lowerPen();
    moveToXY(off + 0 * SCALE, 8 * SCALE);
    moveToXY(off + 0 * SCALE, 2 * SCALE);
    moveToXY(off + 5 * SCALE, 0 * SCALE);
    moveToXY(off + 10 * SCALE, 2 * SCALE);
    moveToXY(off + 10 * SCALE, 8 * SCALE);
    moveToXY(off + 5 * SCALE, 10 * SCALE);
    liftPen();
    moveToXY(off + 6 * SCALE, 3 * SCALE);
    lowerPen();
    moveToXY(off + 10 * SCALE, -1 * SCALE);
    liftPen();
}
void drawLetterR(float off) {
    liftPen();
    moveToXY(off + 0 * SCALE, 0 * SCALE);
    lowerPen();
    moveToXY(off + 0 * SCALE, 10 * SCALE);
    moveToXY(off + 8 * SCALE, 10 * SCALE);
    moveToXY(off + 8 * SCALE, 6 * SCALE);
    moveToXY(off + 0 * SCALE, 6 * SCALE);
    liftPen();
    moveToXY(off + 0 * SCALE, 6 * SCALE);
    lowerPen();
    moveToXY(off + 10 * SCALE, 0 * SCALE);
    liftPen();
}
void drawLetterS(float off) {
    liftPen();
    moveToXY(off + 10 * SCALE, 8 * SCALE);
    lowerPen();
    moveToXY(off + 8 * SCALE, 10 * SCALE);
    moveToXY(off + 2 * SCALE, 10 * SCALE);
    moveToXY(off + 0 * SCALE, 8 * SCALE);
    moveToXY(off + 10 * SCALE, 2 * SCALE);
    moveToXY(off + 8 * SCALE, 0 * SCALE);
    moveToXY(off + 2 * SCALE, 0 * SCALE);
    moveToXY(off + 0 * SCALE, 2 * SCALE);
    liftPen();
}
void drawLetterT(float off) {
    liftPen();
    moveToXY(off + 0 * SCALE, 10 * SCALE);
    lowerPen();
    moveToXY(off + 10 * SCALE, 10 * SCALE);
    liftPen();
    moveToXY(off + 5 * SCALE, 10 * SCALE);
    lowerPen();
    moveToXY(off + 5 * SCALE, 0 * SCALE);
    liftPen();
}
void drawLetterU(float off) {
    liftPen();
    moveToXY(off + 0 * SCALE, 10 * SCALE);
    lowerPen();
    moveToXY(off + 0 * SCALE, 2 * SCALE);
    moveToXY(off + 2 * SCALE, 0 * SCALE);
    moveToXY(off + 8 * SCALE, 0 * SCALE);
    moveToXY(off + 10 * SCALE, 2 * SCALE);
    moveToXY(off + 10 * SCALE, 10 * SCALE);
    liftPen();
}
void drawLetterV(float off) {
    liftPen();
    moveToXY(off + 0 * SCALE, 10 * SCALE);
    lowerPen();
    moveToXY(off + 5 * SCALE, 0 * SCALE);
    moveToXY(off + 10 * SCALE, 10 * SCALE);
    liftPen();
}
void drawLetterW(float off) {
    liftPen();
    moveToXY(off + 0 * SCALE, 10 * SCALE);
    lowerPen();
    moveToXY(off + 3 * SCALE, 0 * SCALE);
    moveToXY(off + 5 * SCALE, 4 * SCALE);
    moveToXY(off + 7 * SCALE, 0 * SCALE);
    moveToXY(off + 10 * SCALE, 10 * SCALE);
    liftPen();
}
void drawLetterX(float off) {
    liftPen();
    moveToXY(off + 0 * SCALE, 0 * SCALE);
    lowerPen();
    moveToXY(off + 10 * SCALE, 10 * SCALE);
    liftPen();
    moveToXY(off + 10 * SCALE, 0 * SCALE);
    lowerPen();
    moveToXY(off + 0 * SCALE, 10 * SCALE);
    liftPen();
}
void drawLetterY(float off) {
    liftPen();
    moveToXY(off + 0 * SCALE, 10 * SCALE);
    lowerPen();
    moveToXY(off + 5 * SCALE, 5 * SCALE);
    moveToXY(off + 10 * SCALE, 10 * SCALE);
    liftPen();
    moveToXY(off + 5 * SCALE, 5 * SCALE);
    lowerPen();
    moveToXY(off + 5 * SCALE, 0 * SCALE);
    liftPen();
}
void drawLetterZ(float off) {
    liftPen();
    moveToXY(off + 0 * SCALE, 10 * SCALE);
    lowerPen();
    moveToXY(off + 10 * SCALE, 10 * SCALE);
    moveToXY(off + 0 * SCALE, 0 * SCALE);
    moveToXY(off + 10 * SCALE, 0 * SCALE);
    liftPen();
}

void drawLetter(char c, float off) {
    switch (c) {
        case 'A':
            drawLetterA(off);
            break;
        case 'B':
            drawLetterB(off);
            break;
        case 'C':
            drawLetterC(off);
            break;
        case 'D':
            drawLetterD(off);
            break;
        case 'E':
            drawLetterE(off);
            break;
        case 'F':
            drawLetterF(off);
            break;
        case 'G':
            drawLetterG(off);
            break;
        case 'H':
            drawLetterH(off);
            break;
        case 'I':
            drawLetterI(off);
            break;
        case 'J':
            drawLetterJ(off);
            break;
        case 'K':
            drawLetterK(off);
            break;
        case 'L':
            drawLetterL(off);
            break;
        case 'M':
            drawLetterM(off);
            break;
        case 'N':
            drawLetterN(off);
            break;
        case 'O':
            drawLetterO(off);
            break;
        case 'P':
            drawLetterP(off);
            break;
        case 'Q':
            drawLetterQ(off);
            break;
        case 'R':
            drawLetterR(off);
            break;
        case 'S':
            drawLetterS(off);
            break;
        case 'T':
            drawLetterT(off);
            break;
        case 'U':
            drawLetterU(off);
            break;
        case 'V':
            drawLetterV(off);
            break;
        case 'W':
            drawLetterW(off);
            break;
        case 'X':
            drawLetterX(off);
            break;
        case 'Y':
            drawLetterY(off);
            break;
        case 'Z':
            drawLetterZ(off);
            break;
        case ' ':
            break;
        default:
            Serial.print("Unsupported character: ");
            Serial.println(c);
            break;
    }
}

void setup() {
    Serial.begin(9600);
    
    // Wait for serial port to connect
    while (!Serial) {
        ; // Wait for serial port to connect (needed for native USB)
    }
    
    Serial.println("=========================================");
    Serial.println("Arduino Plotter System Starting...");
    Serial.println("Listening on Serial for text input");
    Serial.println("=========================================");

    lcd.init();
    lcd.backlight();

    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    digitalWrite(LED_PIN, HIGH);

    if (!initializeMotors()) {
        Serial.println("ERROR: Motor initialization failed!");
        displayMessage("MOTOR INIT ERROR", "Check connections");
        while (1) {
            digitalWrite(LED_PIN, HIGH);
            delay(500);
            digitalWrite(LED_PIN, LOW);
            delay(500);
        }
    }

    displayMessage("Ready to draw!", "Input Required");
    Serial.println("System ready - waiting for input...");
    Serial.println("Send text via Serial to begin drawing");
}

void loop() {
    // Check for reset button press
    if (digitalRead(BUTTON_PIN) == LOW) {
        delay(50);  // Debounce
        if (digitalRead(BUTTON_PIN) == LOW) {
            Serial.println("Reset button pressed - returning home");
            resetSystem();
        }
        while (digitalRead(BUTTON_PIN) == LOW);  // Wait for release
    }

    static float xOff = 0;
    static float yOff = 0;

    // Check for serial input
    if (Serial.available()) {
        String word = Serial.readStringUntil('\n');
        word.trim();
        
        if (word.length() == 0) {
            Serial.println("Received empty string, ignoring");
            return;
        }
        
        // Echo received data (like Python mock_arduino.py)
        Serial.print("[Arduino Serial] ");
        Serial.println(word);
        
        word.toUpperCase();

        Serial.print("Processing word: '");
        Serial.print(word);
        Serial.println("'");
        
        displayMessage("RECEIVED:", word);
        digitalWrite(LED_PIN, LOW);

        liftPen();
        moveToXY(xOff, yOff);

        // Draw each character
        for (int i = 0; i < word.length(); i++) {
            char c = word[i];
            float letterWidth = getLetterWidth(c);

            Serial.print("Drawing character '");
            Serial.print(c);
            Serial.print("' at X:");
            Serial.print(xOff);
            Serial.print(" Y:");
            Serial.println(yOff);

            drawLetter(c, xOff);

            float nextX = xOff + letterWidth + BASE_LETTER_SPACING;

            // Check if we need to wrap to next line
            if (nextX > MAX_LINE_WIDTH) {
                Serial.println("Line width exceeded, moving to next line");
                xOff = 0;
                yOff += LINE_SPACING;
                liftPen();
                moveToXY(xOff, yOff);
            } else {
                liftPen();
                moveToXY(nextX, yOff);
                xOff = nextX;
            }
        }

        digitalWrite(LED_PIN, HIGH);
        lcd.clear();
        lcd.print("DONE!");
        Serial.println("DRAWING_COMPLETE");
        Serial.println("=========================================");

        delay(2000);

        // Reset position for next word
        xOff = 0;
        yOff = 0;
        goHome();
        displayMessage("Ready to draw!", "Input Required");
        Serial.println("Ready for next input...");
    }
    
    // Small delay to prevent tight loop
    delay(10);
}
