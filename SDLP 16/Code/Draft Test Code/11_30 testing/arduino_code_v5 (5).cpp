#include <Adafruit_MotorShield.h>
#include <AccelStepper.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

Adafruit_MotorShield AFMSbot(0x61);
Adafruit_MotorShield AFMStop(0x60);

Adafruit_StepperMotor *stepperX = NULL;
Adafruit_StepperMotor *stepperY = NULL;
Adafruit_StepperMotor *stepperZ1 = NULL;
Adafruit_StepperMotor *stepperZ2 = NULL;

// Wrapper functions for AccelStepper
// FINAL DIRECTIONS - Nov 16, 2025 (Fixed all inversions)
// X: Normal, Y: Inverted, Z1: Inverted, Z2: Normal (synchronized with Z1)
void forwardstepX() { 
    if (stepperX) stepperX->onestep(FORWARD, DOUBLE);   // Belt: Normal
}
void backwardstepX() { 
    if (stepperX) stepperX->onestep(BACKWARD, DOUBLE);  // Belt: Normal
}
void forwardstepY() { 
    if (stepperY) stepperY->onestep(BACKWARD, DOUBLE);  // Platform: INVERTED
}
void backwardstepY() { 
    if (stepperY) stepperY->onestep(FORWARD, DOUBLE);   // Platform: INVERTED
}
void forwardstepZ1() { 
    if (stepperZ1) stepperZ1->onestep(BACKWARD, DOUBLE);  // Left pen: INVERTED
}
void backwardstepZ1() { 
    if (stepperZ1) stepperZ1->onestep(FORWARD, DOUBLE);   // Left pen: INVERTED
}
void forwardstepZ2() { 
    if (stepperZ2) stepperZ2->onestep(FORWARD, DOUBLE);   // Right pen: NORMAL (matches Z1)
}
void backwardstepZ2() { 
    if (stepperZ2) stepperZ2->onestep(BACKWARD, DOUBLE);  // Right pen: NORMAL (matches Z1)
}

// AccelStepper objects
AccelStepper accelX(forwardstepX, backwardstepX);
AccelStepper accelY(forwardstepY, backwardstepY);
AccelStepper accelZ1(forwardstepZ1, backwardstepZ1);
AccelStepper accelZ2(forwardstepZ2, backwardstepZ2);

#define LED_PIN 30
#define BUTTON_PIN 31

LiquidCrystal_I2C lcd(0x27, 16, 2);

// NEW LETTER DRAWING SPECIFICATIONS (Nov 16, 2025 - UPDATED)
#define STEPS_PER_MM 10
#define PEN_UP_STEPS 50
#define PEN_DOWN_STEPS 0

// Letter drawing dimensions (in steps, not mm!)
#define LETTER_WIDTH_STEPS 100     // One letter width
#define LETTER_HEIGHT_STEPS 100    // Letter height
#define LETTER_SPACING_STEPS 12    // Spacing between letters
#define FULL_WIDTH_STEPS 400       // Maximum line width
#define LINE_SPACING_STEPS 150     // Spacing between lines

// SCALE: Letters use 0-10 coordinates. SCALE converts to mm. Then STEPS_PER_MM converts to steps.
#define SCALE 0.7
#define BASE_LETTER_SPACING ((LETTER_SPACING_STEPS / (float)STEPS_PER_MM) * SCALE)
#define LINE_SPACING ((LINE_SPACING_STEPS / (float)STEPS_PER_MM) * SCALE)
#define MAX_LINE_WIDTH (FULL_WIDTH_STEPS / (float)STEPS_PER_MM)

#define MAX_SPEED 200.0
#define ACCELERATION 100.0

// PHYSICAL STARTING POSITION
#define HOME_X 0.0       // Start at current X position (0,0 origin)
#define HOME_Y 0.0       // Start at current Y position (0,0 origin)

// Position tracking - initialized to starting position at 0,0
long currentX = 0;       // Start at X=0 (current position)
long currentY = 0;       // Start at Y=0 (current position)
long currentZ = 50;      // Start at Z=50 steps (pen UP, off paper)

// Global Y offset for letter drawing
float globalYOffset = 0;

// =============================================
// NEW HELPER FUNCTIONS FOR SIMPLIFIED MOVEMENT
// =============================================

void displayMessage(const char* l1, const char* l2 = "") {
    Serial.print(F("[LCD] Line1: "));
    Serial.println(l1);
    if (l2[0] != '\0') {
        Serial.print(F("[LCD] Line2: "));
        Serial.println(l2);
    }
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(l1);
    if (l2[0] != '\0') {
        lcd.setCursor(0, 1);
        lcd.print(l2);
    }
}

// Coordinate transformation functions
float transformX(float x) {
    return x * SCALE;
}

float transformY(float y) {
    return y * SCALE;
}

float getLetterX(float offset, float x) {
    return offset + transformX(x);
}

float getLetterY(float y) {
    return globalYOffset + transformY(y);
}

// Enhanced movement functions
void drawLine(float startX, float startY, float endX, float endY) {
    Serial.print(F("[drawLine] From ("));
    Serial.print(startX);
    Serial.print(F(","));
    Serial.print(startY);
    Serial.print(F(") to ("));
    Serial.print(endX);
    Serial.print(F(","));
    Serial.print(endY);
    Serial.println(F(")"));
    
    liftPen();
    moveToXY(startX, startY);
    lowerPen();
    moveToXY(endX, endY);
    liftPen();
}

void drawPolyline(int numPoints, float points[][2]) {
    if (numPoints < 1) return;
    
    liftPen();
    moveToXY(points[0][0], points[0][1]);
    lowerPen();
    
    for (int i = 1; i < numPoints; i++) {
        moveToXY(points[i][0], points[i][1]);
    }
    liftPen();
}

void moveRelative(float deltaX, float deltaY) {
    float currentXmm = currentX / (float)STEPS_PER_MM;
    float currentYmm = currentY / (float)STEPS_PER_MM;
    moveToXY(currentXmm + deltaX, currentYmm + deltaY);
}

void beginLetter() {
    liftPen();  // Always start with pen up
    Serial.println(F("[beginLetter] Pen up, ready to draw"));
}

void endLetter() {
    liftPen();  // Always end with pen up
    Serial.println(F("[endLetter] Pen up, letter complete"));
}

void drawStroke(float startX, float startY, float endX, float endY) {
    moveToXY(startX, startY);
    lowerPen();
    moveToXY(endX, endY);
    liftPen();
}

// Debugging functions
void debugLetterCoordinates(char letter, float offset) {
    Serial.println(F("=== LETTER COORDINATE DEBUG ==="));
    Serial.print(F("Letter: ")); Serial.println(letter);
    Serial.print(F("Offset: ")); Serial.println(offset);
    Serial.print(F("Global Y Offset: ")); Serial.println(globalYOffset);
    
    Serial.println(F("Key Points:"));
    Serial.print(F("  Top-Left: (")); 
    Serial.print(getLetterX(offset, 0)); Serial.print(F(", "));
    Serial.print(getLetterY(0)); Serial.println(F(")"));
    
    Serial.print(F("  Top-Right: (")); 
    Serial.print(getLetterX(offset, 10)); Serial.print(F(", "));
    Serial.print(getLetterY(0)); Serial.println(F(")"));
    
    Serial.print(F("  Bottom-Left: (")); 
    Serial.print(getLetterX(offset, 0)); Serial.print(F(", "));
    Serial.print(getLetterY(10)); Serial.println(F(")"));
    
    Serial.print(F("  Bottom-Right: (")); 
    Serial.print(getLetterX(offset, 10)); Serial.print(F(", "));
    Serial.print(getLetterY(10)); Serial.println(F(")"));
    
    Serial.println(F("==============================="));
}

void testCoordinateSystem() {
    Serial.println(F("=== COORDINATE SYSTEM TEST ==="));
    liftPen();
    
    // Draw a box to verify coordinates
    moveToXY(getLetterX(0, 0), getLetterY(0));   // Top-left
    lowerPen();
    moveToXY(getLetterX(0, 10), getLetterY(0));  // Top-right
    moveToXY(getLetterX(0, 10), getLetterY(10)); // Bottom-right  
    moveToXY(getLetterX(0, 0), getLetterY(10));  // Bottom-left
    moveToXY(getLetterX(0, 0), getLetterY(0));   // Back to top-left
    liftPen();
    
    // Draw cross for center
    moveToXY(getLetterX(0, 5), getLetterY(0));   // Top-center
    lowerPen();
    moveToXY(getLetterX(0, 5), getLetterY(10));  // Bottom-center
    liftPen();
    moveToXY(getLetterX(0, 0), getLetterY(5));   // Left-center  
    lowerPen();
    moveToXY(getLetterX(0, 10), getLetterY(5));  // Right-center
    liftPen();
    
    Serial.println(F("=== TEST COMPLETE ==="));
}

// =============================================
// EXISTING CORE FUNCTIONS (OPTIMIZED)
// =============================================

bool initializeMotors() {
    Serial.println(F("Initializing motor shields..."));

    if (!AFMSbot.begin()) {
        Serial.println(F("Could not find Motor Shield 1, check address 0x61"));
        return false;
    }

    if (!AFMStop.begin()) {
        Serial.println(F("Could not find Motor Shield 2, check address 0x60"));
        return false;
    }

    stepperX = AFMSbot.getStepper(200, 2);  // X-axis: Bottom Port 2 → Belt
    stepperY = AFMSbot.getStepper(200, 1);  // Y-axis: Bottom Port 1 → Platform
    stepperZ1 = AFMStop.getStepper(200, 1); // Z1: Upper Port 1 → Left pen
    stepperZ2 = AFMStop.getStepper(200, 2); // Z2: Upper Port 2 → Right pen

    if (!stepperX || !stepperY || !stepperZ1 || !stepperZ2) {
        Serial.println(F("Error: One or more motors failed to initialize"));
        return false;
    }

    // Configure AccelStepper settings
    accelX.setMaxSpeed(MAX_SPEED);
    accelX.setAcceleration(ACCELERATION);
    accelY.setMaxSpeed(MAX_SPEED);
    accelY.setAcceleration(ACCELERATION);
    accelZ1.setMaxSpeed(MAX_SPEED);
    accelZ1.setAcceleration(ACCELERATION);
    accelZ2.setMaxSpeed(MAX_SPEED);
    accelZ2.setAcceleration(ACCELERATION);

    // Initialize position counters
    accelX.setCurrentPosition(0);
    accelY.setCurrentPosition(0);
    accelZ1.setCurrentPosition(50);
    accelZ2.setCurrentPosition(50);
    
    Serial.println(F("All motors initialized successfully"));
    Serial.println(F("Starting position: X=0 steps (current), Y=0 steps (current), Z=50 (UP)"));
    return true;
}

void moveZ(long targetZ) {
    if (!stepperZ1 || !stepperZ2) {
        Serial.println(F("Error: Z not init!"));
        return;
    }

    long deltaZ = targetZ - currentZ;
    
    Serial.print(F("[moveZ] Target: "));
    Serial.print(targetZ);
    Serial.print(F(" steps, Current: "));
    Serial.print(currentZ);
    Serial.print(F(" steps, Delta: "));
    Serial.print(deltaZ);
    Serial.println(F(" steps"));
    
    if (deltaZ == 0) {
        Serial.println(F("[moveZ] No movement needed (delta = 0)"));
        return;
    }

    // Move both Z motors simultaneously
    accelZ1.move(deltaZ);
    accelZ2.move(deltaZ);
    
    while (accelZ1.distanceToGo() != 0 || accelZ2.distanceToGo() != 0) {
        accelZ1.run();
        accelZ2.run();
    }

    currentZ = targetZ;
    Serial.print(F("[moveZ] Complete. New Z position: "));
    Serial.print(currentZ);
    Serial.println(F(" steps"));
}

void liftPen() {
    Serial.println(F("========================================"));
    Serial.println(F(">>> [PEN UP] Lifting pen - NOT WRITING <<<"));
    Serial.println(F("========================================"));
    moveZ(PEN_UP_STEPS);
}

void lowerPen() {
    Serial.println(F("========================================"));
    Serial.println(F(">>> [PEN DOWN] Lowering pen - WRITING <<<"));
    Serial.println(F("========================================"));
    moveZ(PEN_DOWN_STEPS);
}

void moveToXY(float x, float y) {
    long targetX = x * STEPS_PER_MM;
    long targetY = y * STEPS_PER_MM;

    long deltaX = targetX - currentX;
    long deltaY = targetY - currentY;

    const char* penStatus = (currentZ == PEN_DOWN_STEPS) ? "PEN DOWN - WRITING" : "PEN UP - NOT WRITING";
    
    Serial.print(F("[moveToXY] "));
    Serial.print(penStatus);
    Serial.print(F(" | Target: X="));
    Serial.print(x);
    Serial.print(F("mm ("));
    Serial.print(targetX);
    Serial.print(F(" steps), Y="));
    Serial.print(y);
    Serial.print(F("mm ("));
    Serial.print(targetY);
    Serial.print(F(" steps)"));
    Serial.println();

    if (deltaX == 0 && deltaY == 0) {
        Serial.println(F("[moveToXY] No movement needed (delta = 0)"));
        return;
    }

    // Move both X and Y simultaneously
    accelX.move(deltaX);
    accelY.move(deltaY);
    
    while (accelX.distanceToGo() != 0 || accelY.distanceToGo() != 0) {
        accelX.run();
        accelY.run();
    }

    currentX = targetX;
    currentY = targetY;
    
    Serial.print(F("[moveToXY] Complete. New position: X="));
    Serial.print(currentX);
    Serial.print(F(" steps, Y="));
    Serial.print(currentY);
    Serial.println(F(" steps"));
}

void goHome() {
    Serial.println(F("Going home to starting position"));
    liftPen();
    moveToXY(HOME_X, HOME_Y);
    
    // Reset position tracking to 0,0 origin
    currentX = 0;
    currentY = 0;
    currentZ = 50;
    
    // Reset AccelStepper position counters
    accelX.setCurrentPosition(0);
    accelY.setCurrentPosition(0);
    accelZ1.setCurrentPosition(50);
    accelZ2.setCurrentPosition(50);
    
    Serial.println(F("Home position: X=0, Y=0, Z=50 (UP)"));
}

void resetSystem() {
    lcd.clear();
    lcd.print(F("RESETTING..."));
    goHome();
    lcd.clear();
    lcd.print(F("INPUT REQUIRED"));
    digitalWrite(LED_PIN, HIGH);
}

float getLetterWidth(char c) {
    float widthSteps;
    
    switch (c) {
        case 'I':
            widthSteps = 100;
            break;
        case ' ':
            widthSteps = 50;
            break;
        default:
            widthSteps = 100;
            break;
    }
    
    return (widthSteps / (float)STEPS_PER_MM) * SCALE;
}

// =============================================
// CORRECTED LETTER DRAWING FUNCTIONS
// =============================================

void drawLetterA(float off) {
    Serial.println(F("[LETTER A] Starting - Optimized"));
    beginLetter();
    
    // Define coordinates in natural 0-10 system
    float leftX = getLetterX(off, 0);
    float rightX = getLetterX(off, 10);
    float topY = getLetterY(0);
    float bottomY = getLetterY(10);
    float centerX = getLetterX(off, 5);
    float crossY = getLetterY(5);
    
    // Left diagonal
    drawStroke(leftX, bottomY, centerX, topY);
    // Right diagonal
    drawStroke(centerX, topY, rightX, bottomY);
    // Crossbar
    drawStroke(getLetterX(off, 2), crossY, getLetterX(off, 8), crossY);
    
    endLetter();
    Serial.println(F("[LETTER A] Complete - Optimized"));
}

void drawLetterB(float off) {
    Serial.println(F("[LETTER B] Starting - Optimized"));
    beginLetter();
    
    float leftX = getLetterX(off, 0);
    float rightX1 = getLetterX(off, 6);
    float rightX2 = getLetterX(off, 8);
    float topY = getLetterY(0);
    float bottomY = getLetterY(10);
    float midY1 = getLetterY(8);
    float midY2 = getLetterY(6);
    float midY3 = getLetterY(5);
    float midY4 = getLetterY(4);
    float midY5 = getLetterY(2);
    
    // Main vertical line
    drawStroke(leftX, bottomY, leftX, topY);
    // Top curve
    drawStroke(leftX, topY, rightX1, topY);
    drawStroke(rightX1, topY, rightX2, midY1);
    drawStroke(rightX2, midY1, rightX2, midY2);
    drawStroke(rightX2, midY2, rightX1, midY3);
    // Bottom curve
    drawStroke(rightX1, midY3, rightX2, midY4);
    drawStroke(rightX2, midY4, rightX2, midY5);
    drawStroke(rightX2, midY5, rightX1, bottomY);
    drawStroke(rightX1, bottomY, leftX, bottomY);
    
    endLetter();
    Serial.println(F("[LETTER B] Complete - Optimized"));
}

void drawLetterC(float off) {
    Serial.println(F("[LETTER C] Starting - Optimized"));
    beginLetter();
    
    float points[][2] = {
        {getLetterX(off, 10), getLetterY(0)},
        {getLetterX(off, 2), getLetterY(0)},
        {getLetterX(off, 0), getLetterY(2)},
        {getLetterX(off, 0), getLetterY(8)},
        {getLetterX(off, 2), getLetterY(10)},
        {getLetterX(off, 10), getLetterY(10)}
    };
    
    drawPolyline(4, points);
    endLetter();
    Serial.println(F("[LETTER C] Complete - Optimized"));
}

void drawLetterD(float off) {
    Serial.println(F("[LETTER D] Starting - Optimized"));
    beginLetter();
    
    float leftX = getLetterX(off, 0);
    float rightX1 = getLetterX(off, 6);
    float rightX2 = getLetterX(off, 10);
    float topY = getLetterY(0);
    float bottomY = getLetterY(10);
    float midY1 = getLetterY(3);
    float midY2 = getLetterY(7);
    
    // Main vertical line
    drawStroke(leftX, topY, leftX, bottomY);
    // Curve
    drawStroke(leftX, bottomY, rightX1, bottomY);
    drawStroke(rightX1, bottomY, rightX2, midY2);
    drawStroke(rightX2, midY2, rightX2, midY);
    drawStroke(rightX2, midY1, rightX1, topY);
    drawStroke(rightX1, topY, leftX, topY);
    
    endLetter();
    Serial.println(F("[LETTER D] Complete - Optimized"));
}

void drawLetterE(float off) {
    Serial.println(F("[LETTER E] Starting - Optimized"));
    beginLetter();
    
    float leftX = getLetterX(off, 0);
    float rightX1 = getLetterX(off, 8);
    float rightX2 = getLetterX(off, 6);
    float topY = getLetterY(0);
    float bottomY = getLetterY(10);
    float midY = getLetterY(5);
    
    drawStroke(leftX, topY, leftX, bottomY);
    drawStroke(leftX, topY, rightX1, topY);
    drawStroke(leftX, midY, rightX2, midY);
    drawStroke(leftX, bottomY, rightX1, bottomY);
    
    endLetter();
    Serial.println(F("[LETTER E] Complete - Optimized"));
}

void drawLetterF(float off) {
    Serial.println(F("[LETTER F] Starting - Optimized"));
    beginLetter();
    
    float leftX = getLetterX(off, 0);
    float rightX1 = getLetterX(off, 8);
    float rightX2 = getLetterX(off, 6);
    float topY = getLetterY(0);
    float bottomY = getLetterY(10);
    float midY = getLetterY(5);
    
    drawStroke(leftX, topY, leftX, bottomY);
    drawStroke(leftX, topY, rightX1, topY);
    drawStroke(leftX, midY, rightX2, midY);
    
    endLetter();
    Serial.println(F("[LETTER F] Complete - Optimized"));
}

void drawLetterG(float off) {
    Serial.println(F("[LETTER G] Starting - Fixed"));
    beginLetter();
    
    float points[][2] = {
        {getLetterX(off, 10), getLetterY(2)},
        {getLetterX(off, 8), getLetterY(0)},
        {getLetterX(off, 2), getLetterY(0)},
        {getLetterX(off, 0), getLetterY(2)},
        {getLetterX(off, 0), getLetterY(8)},
        {getLetterX(off, 2), getLetterY(10)},
        {getLetterX(off, 8), getLetterY(10)},
        {getLetterX(off, 10), getLetterY(8)},
        {getLetterX(off, 10), getLetterY(5)},
        {getLetterX(off, 6), getLetterY(5)},
    };
    
    drawPolyline(10, points);
    endLetter();
    Serial.println(F("[LETTER G] Complete - Fixed"));
}

void drawLetterH(float off) {
    Serial.println(F("[LETTER H] Starting - Optimized"));
    beginLetter();
    
    float leftX = getLetterX(off, 0);
    float rightX = getLetterX(off, 8);
    float topY = getLetterY(0);
    float bottomY = getLetterY(10);
    float midY = getLetterY(5);
    
    drawStroke(leftX, topY, leftX, bottomY);
    drawStroke(rightX, topY, rightX, bottomY);
    drawStroke(leftX, midY, rightX, midY);
    
    endLetter();
    Serial.println(F("[LETTER H] Complete - Optimized"));
}

void drawLetterI(float off) {
    Serial.println(F("[LETTER I] Starting - Optimized"));
    beginLetter();
    
    float leftX = getLetterX(off, 2);
    float rightX = getLetterX(off, 8);
    float centerX = getLetterX(off, 5);
    float topY = getLetterY(0);
    float bottomY = getLetterY(10);
    
    drawStroke(leftX, topY, rightX, topY);
    drawStroke(leftX, bottomY, rightX, bottomY);
    drawStroke(centerX, topY, centerX, bottomY);
    
    endLetter();
    Serial.println(F("[LETTER I] Complete - Optimized"));
}

void drawLetterJ(float off) {
    Serial.println(F("[LETTER J] Starting - Fixed"));
    beginLetter();
      
    float points[][2] = {
        {getLetterX(off, 2), getLetterY(0)},
        {getLetterX(off, 8), getLetterY(0)},
        {getLetterX(off, 5), getLetterY(0)},
        {getLetterX(off, 5), getLetterY(8)},
        {getLetterX(off, 3), getLetterY(10)},
        {getLetterX(off, 1), getLetterY(10)},
        {getLetterX(off, 0), getLetterY(8)}
    };
    
    drawPolyline(7, points);
    endLetter();
    Serial.println(F("[LETTER J] Complete - Fixed"));
}

void drawLetterK(float off) {
    Serial.println(F("[LETTER K] Starting - Optimized"));
    beginLetter();
    
    float leftX = getLetterX(off, 0);
    float rightX = getLetterX(off, 8);
    float topY = getLetterY(0);
    float bottomY = getLetterY(10);
    float midY = getLetterY(5);
    
    drawStroke(leftX, topY, leftX, bottomY);
    drawStroke(rightX, midY, leftX, topY);
    drawStroke(leftX, midY, rightX, bottomY);
    
    endLetter();
    Serial.println(F("[LETTER K] Complete - Optimized"));
}

void drawLetterL(float off) {
    Serial.println(F("[LETTER L] Starting - Optimized"));
    beginLetter();
    
    float leftX = getLetterX(off, 0);
    float rightX = getLetterX(off, 6);
    float topY = getLetterY(0);
    float bottomY = getLetterY(10);
    
    drawStroke(leftX, topY, leftX, bottomY);
    drawStroke(leftX, bottomY, rightX, bottomY);
    
    endLetter();
    Serial.println(F("[LETTER L] Complete - Optimized"));
}

void drawLetterM(float off) {
    Serial.println(F("[LETTER M] Starting - Optimized"));
    beginLetter();
    
    float points[][2] = {
        {getLetterX(off, 0), getLetterY(0)},
        {getLetterX(off, 0), getLetterY(10)},
        {getLetterX(off, 5), getLetterY(5)},
        {getLetterX(off, 10), getLetterY(10)},
        {getLetterX(off, 10), getLetterY(0)}
    };
    
    drawPolyline(5, points);
    endLetter();
    Serial.println(F("[LETTER M] Complete - Optimized"));
}

void drawLetterN(float off) {
    Serial.println(F("[LETTER N] Starting - Optimized"));
    beginLetter();
    
    float points[][2] = {
        {getLetterX(off, 0), getLetterY(0)},
        {getLetterX(off, 0), getLetterY(10)},
        {getLetterX(off, 8), getLetterY(0)},
        {getLetterX(off, 8), getLetterY(10)}
    };
    
    drawPolyline(4, points);
    endLetter();
    Serial.println(F("[LETTER N] Complete - Optimized"));
}

void drawLetterO(float off) {
    Serial.println(F("[LETTER O] Starting - Optimized"));
    beginLetter();
    
    float points[][2] = {
        {getLetterX(off, 2), getLetterY(10)},
        {getLetterX(off, 8), getLetterY(10)},
        {getLetterX(off, 10), getLetterY(7)},
        {getLetterX(off, 10), getLetterY(3)},
        {getLetterX(off, 8), getLetterY(0)},
        {getLetterX(off, 2), getLetterY(0)},
        {getLetterX(off, 0), getLetterY(3)},
        {getLetterX(off, 0), getLetterY(7)},
        {getLetterX(off, 2), getLetterY(10)}
    };
    
    drawPolyline(9, points);
    endLetter();
    Serial.println(F("[LETTER O] Complete - Optimized"));
}

void drawLetterP(float off) {
    Serial.println(F("[LETTER P] Starting - Optimized"));
    beginLetter();
    
    float leftX = getLetterX(off, 0);
    float rightX1 = getLetterX(off, 6);
    float rightX2 = getLetterX(off, 8);
    float topY = getLetterY(10);
    float bottomY = getLetterY(0);
    float midY1 = getLetterY(8);
    float midY2 = getLetterY(6);
    float midY3 = getLetterY(5);
    
    // Main vertical line
    drawStroke(leftX, bottomY, leftX, topY);
    // Top curve
    drawStroke(leftX, topY, rightX1, topY);
    drawStroke(rightX1, topY, rightX2, midY1);
    drawStroke(rightX2, midY1, rightX2, midY2);
    drawStroke(rightX2, midY2, rightX1, midY3);
    drawStroke(rightX1, midY3, leftX, midY3);
    
    endLetter();
    Serial.println(F("[LETTER P] Complete - Optimized"));
}

void drawLetterQ(float off) {
    Serial.println(F("[LETTER Q] Starting - Optimized"));
    beginLetter();
    
    // Main O shape
    float points[][2] = {
        {getLetterX(off, 2), getLetterY(10)},
        {getLetterX(off, 8), getLetterY(10)},
        {getLetterX(off, 10), getLetterY(7)},
        {getLetterX(off, 10), getLetterY(3)},
        {getLetterX(off, 8), getLetterY(0)},
        {getLetterX(off, 2), getLetterY(0)},
        {getLetterX(off, 0), getLetterY(3)},
        {getLetterX(off, 0), getLetterY(7)},
        {getLetterX(off, 2), getLetterY(10)}
    };
    
    drawPolyline(9, points);
    
    // Tail
    drawStroke(getLetterX(off, 6), getLetterY(3), getLetterX(off, 10), getLetterY(0));
    
    endLetter();
    Serial.println(F("[LETTER Q] Complete - Optimized"));
}

void drawLetterR(float off) {
    Serial.println(F("[LETTER R] Starting - Fixed"));
    beginLetter();
    
    // Fixed R - was upside down
    float leftX = getLetterX(off, 0);
    float rightX1 = getLetterX(off, 6);
    float rightX2 = getLetterX(off, 8);
    float topY = getLetterY(0);
    float bottomY = getLetterY(10);
    float midY = getLetterY(5);
    
    // Main vertical line (from top to bottom)
    drawStroke(leftX, topY, leftX, bottomY);
    
    // Top curve (clockwise from top-left)
    drawStroke(leftX, topY, rightX1, topY);
    drawStroke(rightX1, topY, rightX2, getLetterY(2));
    drawStroke(rightX2, getLetterY(2), rightX2, getLetterY(4));
    drawStroke(rightX2, getLetterY(4), rightX1, midY);
    drawStroke(rightX1, midY, leftX, midY);
    
    // Diagonal leg (from middle to bottom-right)
    drawStroke(getLetterX(off, 5), midY, getLetterX(off, 8), bottomY);
    
    endLetter();
    Serial.println(F("[LETTER R] Complete - Fixed"));
}

void drawLetterS(float off) {
    Serial.println(F("[LETTER S] Starting - Fixed"));
    beginLetter();
    
    // Fixed S - was backwards
    float points[][2] = {
        {getLetterX(off, 2), getLetterY(0)},   // Top-left start
        {getLetterX(off, 8), getLetterY(0)},   // Top-right
        {getLetterX(off, 10), getLetterY(2)},  // Right-top curve
        {getLetterX(off, 8), getLetterY(4)},   // Middle-top
        {getLetterX(off, 2), getLetterY(6)},   // Middle-bottom
        {getLetterX(off, 0), getLetterY(8)},   // Left-bottom curve
        {getLetterX(off, 2), getLetterY(10)}   // Bottom-right
    };
    
    drawPolyline(7, points);
    endLetter();
    Serial.println(F("[LETTER S] Complete - Fixed"));
}

void drawLetterT(float off) {
    Serial.println(F("[LETTER T] Starting - Fixed Orientation"));
    beginLetter();
    
    float leftX = getLetterX(off, 0);
    float rightX = getLetterX(off, 10);
    float centerX = getLetterX(off, 5);
    float topY = getLetterY(0);
    float bottomY = getLetterY(10);
    
    // Top horizontal line (at the top)
    drawStroke(leftX, topY, rightX, topY);
    // Vertical line (from top to bottom)
    drawStroke(centerX, topY, centerX, bottomY);
    
    endLetter();
    Serial.println(F("[LETTER T] Complete - Fixed Orientation"));
}

void drawLetterU(float off) {
    Serial.println(F("[LETTER U] Starting - Optimized"));
    beginLetter();
    
    float points[][2] = {
        {getLetterX(off, 0), getLetterY(0)},
        {getLetterX(off, 0), getLetterY(8)},
        {getLetterX(off, 2), getLetterY(10)},
        {getLetterX(off, 8), getLetterY(10)},
        {getLetterX(off, 10), getLetterY(8)},
        {getLetterX(off, 10), getLetterY(0)}
    };
    
    drawPolyline(6, points);
    endLetter();
    Serial.println(F("[LETTER U] Complete - Optimized"));
}

void drawLetterV(float off) {
    Serial.println(F("[LETTER V] Starting - Optimized"));
    beginLetter();
    
    float leftX = getLetterX(off, 0);
    float rightX = getLetterX(off, 10);
    float centerX = getLetterX(off, 5);
    float topY = getLetterY(0);
    float bottomY = getLetterY(10);
    
    drawStroke(leftX, topY, centerX, bottomY);
    drawStroke(centerX, bottomY, rightX, topY);
    
    endLetter();
    Serial.println(F("[LETTER V] Complete - Optimized"));
}

void drawLetterW(float off) {
    Serial.println(F("[LETTER W] Starting - Optimized"));
    beginLetter();
    
    float points[][2] = {
        {getLetterX(off, 0), getLetterY(0)},
        {getLetterX(off, 3), getLetterY(10)},
        {getLetterX(off, 5), getLetterY(5)},
        {getLetterX(off, 7), getLetterY(10)},
        {getLetterX(off, 10), getLetterY(0)}
    };
    
    drawPolyline(5, points);
    endLetter();
    Serial.println(F("[LETTER W] Complete - Optimized"));
}

void drawLetterX(float off) {
    Serial.println(F("[LETTER X] Starting - Optimized"));
    beginLetter();
    
    float leftX = getLetterX(off, 0);
    float rightX = getLetterX(off, 10);
    float topY = getLetterY(0);
    float bottomY = getLetterY(10);
    
    drawStroke(leftX, topY, rightX, bottomY);
    drawStroke(leftX, bottomY, rightX, topY);
    
    endLetter();
    Serial.println(F("[LETTER X] Complete - Optimized"));
}

void drawLetterY(float off) {
    Serial.println(F("[LETTER Y] Starting - Optimized"));
    beginLetter();
    
    float leftX = getLetterX(off, 0);
    float rightX = getLetterX(off, 10);
    float centerX = getLetterX(off, 5);
    float topY = getLetterY(0);
    float bottomY = getLetterY(10);
    float midY = getLetterY(5);
    
    drawStroke(leftX, topY, centerX, midY);
    drawStroke(rightX, topY, centerX, midY);
    drawStroke(centerX, midY, centerX, bottomY);
    
    endLetter();
    Serial.println(F("[LETTER Y] Complete - Optimized"));
}

void drawLetterZ(float off) {
    Serial.println(F("[LETTER Z] Starting - Optimized"));
    beginLetter();
    
    float leftX = getLetterX(off, 0);
    float rightX = getLetterX(off, 10);
    float topY = getLetterY(0);
    float bottomY = getLetterY(10);
    
    drawStroke(leftX, topY, rightX, topY);
    drawStroke(rightX, topY, leftX, bottomY);
    drawStroke(leftX, bottomY, rightX, bottomY);
    
    endLetter();
    Serial.println(F("[LETTER Z] Complete - Optimized"));
}

void drawLetter(char c, float off) {
    // Debug output
    debugLetterCoordinates(c, off);
    
    switch (c) {
        case 'A': drawLetterA(off); break;
        case 'B': drawLetterB(off); break;
        case 'C': drawLetterC(off); break;
        case 'D': drawLetterD(off); break;
        case 'E': drawLetterE(off); break;
        case 'F': drawLetterF(off); break;
        case 'G': drawLetterG(off); break;
        case 'H': drawLetterH(off); break;
        case 'I': drawLetterI(off); break;
        case 'J': drawLetterJ(off); break;
        case 'K': drawLetterK(off); break;
        case 'L': drawLetterL(off); break;
        case 'M': drawLetterM(off); break;
        case 'N': drawLetterN(off); break;
        case 'O': drawLetterO(off); break;
        case 'P': drawLetterP(off); break;
        case 'Q': drawLetterQ(off); break;
        case 'R': drawLetterR(off); break;
        case 'S': drawLetterS(off); break;
        case 'T': drawLetterT(off); break;
        case 'U': drawLetterU(off); break;
        case 'V': drawLetterV(off); break;
        case 'W': drawLetterW(off); break;
        case 'X': drawLetterX(off); break;
        case 'Y': drawLetterY(off); break;
        case 'Z': drawLetterZ(off); break;
        case ' ': break;
        default:
            Serial.print("Unsupported character: ");
            Serial.println(c);
            break;
    }
}

void setup() {
    Serial.begin(9600);
    
    while (!Serial) {
        ; // Wait for serial port to connect
    }
    
    Serial.println(F("========================================="));
    Serial.println(F("Arduino Plotter System Starting..."));
    Serial.println(F("Listening on Serial for text input"));
    Serial.println(F("========================================="));

    lcd.init();
    lcd.backlight();

    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    digitalWrite(LED_PIN, HIGH);

    if (!initializeMotors()) {
        Serial.println(F("ERROR: Motor initialization failed!"));
        displayMessage("MOTOR INIT ERROR", "Check connections");
        while (1) {
            digitalWrite(LED_PIN, HIGH);
            delay(500);
            digitalWrite(LED_PIN, LOW);
            delay(500);
        }
    }

    displayMessage("Ready to draw!", "Input Required");
    Serial.println(F("System ready - waiting for input..."));
    Serial.println(F("Send text via Serial to begin drawing"));
}

void loop() {
    static unsigned long lastHeartbeat = 0;
    if (millis() - lastHeartbeat > 30000) {
        Serial.print(F("[Heartbeat] Loop running, Serial.available() = "));
        Serial.println(Serial.available());
        lastHeartbeat = millis();
    }
    
    // Position offset tracking for letter drawing
    static float xOff = 0;
    static float yOff = 0;
    static char word[32];
    
    // Check for serial input
    if (Serial.available()) {
        Serial.print(F(">>> Serial available! Bytes: "));
        Serial.println(Serial.available());
        
        int len = Serial.readBytesUntil('\n', word, sizeof(word) - 1);
        word[len] = '\0';
        
        Serial.print(F(">>> Read "));
        Serial.print(len);
        Serial.println(F(" bytes"));
        
        // Trim whitespace
        while (len > 0 && (word[len-1] == ' ' || word[len-1] == '\r')) {
            word[--len] = '\0';
        }
        
        if (len == 0) {
            Serial.println(F("Empty string"));
            return;
        }
        
        // Echo received data
        Serial.print(F("[Arduino Serial] Received: "));
        Serial.println(word);
        
        // Display original input on LCD
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Input: ");
        lcd.setCursor(0, 1);
        lcd.print(word);
        delay(1000);
        
        // Convert to uppercase
        for (int i = 0; i < len; i++) {
            if (word[i] >= 'a' && word[i] <= 'z') {
                word[i] = word[i] - 32;
            }
        }

        Serial.print(F("Processing (uppercase): "));
        Serial.println(word);
        
        // Check for STOP command
        if (strcmp(word, "STOP") == 0 || strcmp(word, "HALT") == 0) {
            Serial.println(F(">>> EMERGENCY STOP ACTIVATED <<<"));
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("EMERGENCY STOP");
            lcd.setCursor(0, 1);
            lcd.print("System Halted");
            digitalWrite(LED_PIN, HIGH);
            
            // Stop all motors immediately
            if (stepperX) stepperX->release();
            if (stepperY) stepperY->release();
            if (stepperZ1) stepperZ1->release();
            if (stepperZ2) stepperZ2->release();
            
            while(true) {
                delay(1000);
            }
        }
        
        // Check for TESTCOORD command
        if (strcmp(word, "TESTCOORD") == 0) {
            Serial.println(F("TEST: Coordinate System Test"));
            displayMessage("TEST: COORDINATES", "Testing system...");
            testCoordinateSystem();
            Serial.println(F("TEST_COMPLETE"));
            displayMessage("TEST: COORDINATES", "Complete!");
            delay(1000);
            displayMessage("Ready to draw!", "Input Required");
            return;
        }
        
        // Motor test commands
        if (strcmp(word, "UP") == 0) {
            Serial.println(F("TEST: Lifting pen"));
            displayMessage("TEST: UP", "Lifting pen...");
            liftPen();
            Serial.println(F("TEST_COMPLETE"));
            displayMessage("TEST: UP", "Complete!");
            delay(1000);
            displayMessage("Ready to draw!", "Input Required");
            return;
        }
        
        if (strcmp(word, "DOWN") == 0) {
            Serial.println(F("TEST: Lowering pen"));
            displayMessage("TEST: DOWN", "Lowering pen...");
            lowerPen();
            Serial.println(F("TEST_COMPLETE"));
            displayMessage("TEST: DOWN", "Complete!");
            delay(1000);
            displayMessage("Ready to draw!", "Input Required");
            return;
        }
        
        if (strcmp(word, "LEFT") == 0) {
            Serial.println(F("TEST: Moving pen left 20mm"));
            displayMessage("TEST: LEFT", "Moving 20mm...");
            float currentXmm = currentX / (float)STEPS_PER_MM;
            float currentYmm = currentY / (float)STEPS_PER_MM;
            moveToXY(currentXmm - 20, currentYmm);
            accelX.setCurrentPosition(0);
            accelY.setCurrentPosition(0);
            currentX = 0;
            currentY = 0;
            Serial.println(F("TEST_COMPLETE - Position reset to 0,0"));
            displayMessage("TEST: LEFT", "Complete!");
            delay(1000);
            displayMessage("Ready to draw!", "Input Required");
            return;
        }
        
        if (strcmp(word, "RIGHT") == 0) {
            Serial.println(F("TEST: Moving pen right 20mm"));
            displayMessage("TEST: RIGHT", "Moving 20mm...");
            float currentXmm = currentX / (float)STEPS_PER_MM;
            float currentYmm = currentY / (float)STEPS_PER_MM;
            moveToXY(currentXmm + 20, currentYmm);
            accelX.setCurrentPosition(0);
            accelY.setCurrentPosition(0);
            currentX = 0;
            currentY = 0;
            Serial.println(F("TEST_COMPLETE - Position reset to 0,0"));
            displayMessage("TEST: RIGHT", "Complete!");
            delay(1000);
            displayMessage("Ready to draw!", "Input Required");
            return;
        }
        
        if (strcmp(word, "FORWARD") == 0) {
            Serial.println(F("TEST: Moving platform forward 20mm"));
            displayMessage("TEST: FORWARD", "Moving 20mm...");
            float currentXmm = currentX / (float)STEPS_PER_MM;
            float currentYmm = currentY / (float)STEPS_PER_MM;
            moveToXY(currentXmm, currentYmm + 20);
            accelX.setCurrentPosition(0);
            accelY.setCurrentPosition(0);
            currentX = 0;
            currentY = 0;
            Serial.println(F("TEST_COMPLETE - Position reset to 0,0"));
            displayMessage("TEST: FORWARD", "Complete!");
            delay(1000);
            displayMessage("Ready to draw!", "Input Required");
            return;
        }
        
        if (strcmp(word, "BACK") == 0) {
            Serial.println(F("TEST: Moving platform back 20mm"));
            displayMessage("TEST: BACK", "Moving 20mm...");
            float currentXmm = currentX / (float)STEPS_PER_MM;
            float currentYmm = currentY / (float)STEPS_PER_MM;
            moveToXY(currentXmm, currentYmm - 20);
            accelX.setCurrentPosition(0);
            accelY.setCurrentPosition(0);
            currentX = 0;
            currentY = 0;
            Serial.println(F("TEST_COMPLETE - Position reset to 0,0"));
            displayMessage("TEST: BACK", "Complete!");
            delay(1000);
            displayMessage("Ready to draw!", "Input Required");
            return;
        }
        
        if (strcmp(word, "HOME") == 0) {
            Serial.println(F("TEST: Going to home position"));
            displayMessage("TEST: HOME", "Going home...");
            goHome();
            Serial.println(F("TEST_COMPLETE"));
            displayMessage("TEST: HOME", "Complete!");
            delay(1000);
            displayMessage("Ready to draw!", "Input Required");
            return;
        }
        
        // Display on LCD
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("RX: ");
        lcd.print(word);
        
        delay(1000);
        
        digitalWrite(LED_PIN, HIGH);

        liftPen();
        moveToXY(xOff, yOff);
        
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Currently");
        lcd.setCursor(0, 1);
        lcd.print("writing...");
        
        Serial.print(F("Starting to draw at X="));
        Serial.print(xOff);
        Serial.print(F("mm, Y="));
        Serial.print(yOff);
        Serial.println(F("mm"));

        // Draw each character
        for (int i = 0; i < len; i++) {
            char c = word[i];
            float letterWidth = getLetterWidth(c);
            
            Serial.println(F(""));
            Serial.println(F("╔═══════════════════════════════════════╗"));
            Serial.print(F("║ DRAWING LETTER #"));
            Serial.print(i + 1);
            Serial.print(F(": '"));
            Serial.print(c);
            Serial.println(F("'"));
            Serial.print(F("║ Position: X="));
            Serial.print(xOff);
            Serial.print(F("mm, Y="));
            Serial.print(yOff);
            Serial.println(F("mm"));
            Serial.print(F("║ Width: "));
            Serial.print(letterWidth);
            Serial.println(F("mm"));
            Serial.println(F("╚═══════════════════════════════════════╝"));
            Serial.println(F(""));

            globalYOffset = yOff;
            drawLetter(c, xOff);
            
            Serial.println(F(""));
            Serial.print(F(">>> LETTER '"));
            Serial.print(c);
            Serial.println(F("' DRAWING COMPLETE <<<"));
            Serial.println(F(""));

            xOff += letterWidth + BASE_LETTER_SPACING;

            // Check if we need to wrap to next line
            if (xOff > MAX_LINE_WIDTH && i < len - 1) {
                Serial.println(F("Line wrap - moving to next line"));
                xOff = 0;
                yOff += LINE_SPACING;
                liftPen();
                moveToXY(xOff, yOff);
            }
        }

        // Lift pen after completing the word
        liftPen();
        Serial.println(F(">>> [WORD COMPLETE] Pen lifted, resetting position <<<"));

        digitalWrite(LED_PIN, LOW);
        lcd.clear();
        lcd.print(F("DONE!"));
        Serial.println(F("DRAWING_COMPLETE"));
        Serial.println(F("========================================="));

        delay(2000);

        // Reset position for next word
        xOff = HOME_X;
        yOff = HOME_Y;
        goHome();
        displayMessage("Ready to draw!", "Input Required");
        Serial.println(F("Ready for next input..."));
    }
    
    delay(10);
}
