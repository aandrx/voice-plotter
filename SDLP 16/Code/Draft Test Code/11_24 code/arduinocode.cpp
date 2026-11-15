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
// Based on physical testing with working motors:
// - Letter width: 100 steps (X-axis: LEFT/RIGHT)
// - Letter height: 100 steps (Y-axis: FORWARD/BACK)
// - Letter spacing: 50 steps between letters (from furthest right point)
// - Letters are mirrored both horizontally and vertically to correct upside-down drawing
// - Letter coordinates are 0-10, SCALE converts to mm, then STEPS_PER_MM converts to steps
#define STEPS_PER_MM 10
#define PEN_UP_STEPS 50
#define PEN_DOWN_STEPS 0

// Letter drawing dimensions (in steps, not mm!)
#define LETTER_WIDTH_STEPS 100     // One letter width (changed from 50 to 100)
#define LETTER_HEIGHT_STEPS 100    // Letter height (changed from 150 to 100)
#define LETTER_SPACING_STEPS 12    // Spacing between letters (was 25, now half = 12.5, rounded to 12)
#define FULL_WIDTH_STEPS 400       // Maximum line width (fewer letters per line now)
#define LINE_SPACING_STEPS 150     // Spacing between lines (letter height + gap)

// SCALE: Letters use 0-10 coordinates. SCALE converts to mm. Then STEPS_PER_MM converts to steps.
// For 100-step letters: 10 units * 1.0 mm/unit * 10 steps/mm = 100 steps ✓
#define SCALE 1.0
#define BASE_LETTER_SPACING (LETTER_SPACING_STEPS / (float)STEPS_PER_MM)  // Convert to mm
#define LINE_SPACING (LINE_SPACING_STEPS / (float)STEPS_PER_MM)           // Convert to mm
#define MAX_LINE_WIDTH (FULL_WIDTH_STEPS / (float)STEPS_PER_MM)           // Convert to mm

#define MAX_SPEED 200.0
#define ACCELERATION 100.0

// PHYSICAL STARTING POSITION (Nov 16, 2025 - UPDATED)
// NOW ASSUMING PEN STARTS AT 0,0 (current position is origin)
// This allows writing to start from wherever the pen currently is
#define HOME_X 0.0       // Start at current X position (0,0 origin)
#define HOME_Y 0.0       // Start at current Y position (0,0 origin)

// Position tracking - initialized to starting position at 0,0
long currentX = 0;       // Start at X=0 (current position)
long currentY = 0;       // Start at Y=0 (current position)
long currentZ = 50;      // Start at Z=50 steps (pen UP, off paper)

// Global Y offset for letter drawing (so letters can use it without changing all functions)
float globalYOffset = 0;  // Will be set before each letter is drawn

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

    // FINAL CORRECT MAPPING based on latest testing (Nov 15, 2025):
    // UP moves belt+right pen → Z1=belt, Z2=right pen
    // LEFT moves left pen → X=left pen
    // Need: X=belt, Z1=left pen, Z2=right pen
    stepperX = AFMSbot.getStepper(200, 2);  // X-axis: Bottom Port 2 → Belt (was Z1)
    stepperY = AFMSbot.getStepper(200, 1);  // Y-axis: Bottom Port 1 → Platform ✓
    stepperZ1 = AFMStop.getStepper(200, 1);  // Z1: Upper Port 1 → Left pen (was X)
    stepperZ2 = AFMStop.getStepper(200, 2);  // Z2: Upper Port 2 → Right pen ✓

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

    // Initialize AccelStepper position counters to match physical starting position
    // Pen starts at: X=0 (current), Y=0 (current), Z=50 (UP/off paper)
    accelX.setCurrentPosition(0);
    accelY.setCurrentPosition(0);
    accelZ1.setCurrentPosition(50);
    accelZ2.setCurrentPosition(50);
    
    Serial.println(F("All motors initialized successfully"));
    Serial.println(F("Starting position: X=0 steps (current), Y=0 steps (current), Z=50 (UP)"));
    Serial.print(F("[POSITION] Initial: X="));
    Serial.print(currentX);
    Serial.print(F(", Y="));
    Serial.print(currentY);
    Serial.print(F(", Z="));
    Serial.println(currentZ);
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

    // Move both Z motors simultaneously using AccelStepper
    accelZ1.move(deltaZ);
    accelZ2.move(deltaZ);
    
    // Run both motors until they reach target
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

    // Log pen status clearly
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
    Serial.print(F("[moveToXY] Delta: X="));
    Serial.print(deltaX);
    Serial.print(F(" steps, Y="));
    Serial.print(deltaY);
    Serial.println(F(" steps"));

    if (deltaX == 0 && deltaY == 0) {
        Serial.println(F("[moveToXY] No movement needed (delta = 0)"));
        return;
    }

    // Move both X and Y simultaneously using AccelStepper
    accelX.move(deltaX);
    accelY.move(deltaY);
    
    // Run both motors until they reach target
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
    Serial.print(F("[POSITION] X="));
    Serial.print(currentX);
    Serial.print(F(", Y="));
    Serial.print(currentY);
    Serial.print(F(", Z="));
    Serial.println(currentZ);
}

void goHome() {
    Serial.println(F("Going home to starting position"));
    liftPen();  // Make sure pen is up
    moveToXY(HOME_X, HOME_Y);
    
    // Reset position tracking to 0,0 origin
    currentX = 0;
    currentY = 0;
    currentZ = 50;   // Pen up
    
    // Reset AccelStepper position counters to 0,0 origin
    accelX.setCurrentPosition(0);
    accelY.setCurrentPosition(0);
    accelZ1.setCurrentPosition(50);
    accelZ2.setCurrentPosition(50);
    
    Serial.println(F("Home position: X=0, Y=0, Z=50 (UP)"));
    Serial.print(F("[POSITION] X="));
    Serial.print(currentX);
    Serial.print(F(", Y="));
    Serial.print(currentY);
    Serial.print(F(", Z="));
    Serial.println(currentZ);
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
    // All letters are now 100 steps wide (10mm)
    // Spacing handled separately by BASE_LETTER_SPACING
    float widthSteps;
    
    switch (c) {
        case 'I':
            widthSteps = 100;  // All letters same width now
            break;
        case ' ':
            widthSteps = 50;   // Space between words (5mm)
            break;
        default:
            widthSteps = 100;  // All letters 100 steps = 10mm
            break;
    }
    
    // Convert steps to mm for compatibility with moveToXY
    return widthSteps / (float)STEPS_PER_MM;
}

void drawLetterA(float off) {
    Serial.println(F("[LETTER A] Starting"));
    liftPen();
    moveToXY(off + 10 * SCALE, globalYOffset + 10 * SCALE);   // Reflect both X and Y: (0,0)->(10,10)
    lowerPen();
    moveToXY(off + 5 * SCALE, globalYOffset + 0 * SCALE);     // Reflect both X and Y: (5,10)->(5,0)
    moveToXY(off + 0 * SCALE, globalYOffset + 10 * SCALE);    // Reflect both X and Y: (10,0)->(0,10)
    liftPen();
    moveToXY(off + 8 * SCALE, globalYOffset + 5 * SCALE);     // Reflect both X and Y: (2,5)->(8,5)
    lowerPen();
    moveToXY(off + 2 * SCALE, globalYOffset + 5 * SCALE);     // Reflect both X and Y: (8,5)->(2,5)
    liftPen();
    Serial.println(F("[LETTER A] Complete"));
}
void drawLetterB(float off) {
    Serial.println(F("[LETTER B] Starting"));
    liftPen();
    moveToXY(off + 0 * SCALE, globalYOffset + 0 * SCALE);
    lowerPen();
    moveToXY(off + 0 * SCALE, globalYOffset + 10 * SCALE);
    moveToXY(off + 6 * SCALE, globalYOffset + 10 * SCALE);
    moveToXY(off + 8 * SCALE, globalYOffset + 8 * SCALE);
    moveToXY(off + 8 * SCALE, globalYOffset + 6 * SCALE);
    moveToXY(off + 6 * SCALE, globalYOffset + 5 * SCALE);
    moveToXY(off + 8 * SCALE, globalYOffset + 4 * SCALE);
    moveToXY(off + 8 * SCALE, globalYOffset + 2 * SCALE);
    moveToXY(off + 6 * SCALE, globalYOffset + 0 * SCALE);
    moveToXY(off + 0 * SCALE, globalYOffset + 0 * SCALE);
    liftPen();
    Serial.println(F("[LETTER B] Complete"));
}
void drawLetterC(float off) {
    Serial.println(F("[LETTER C] Starting"));
    liftPen();
    moveToXY(off + 10 * SCALE, globalYOffset + 10 * SCALE);  // Flip X: (0,10)->(10,10)
    lowerPen();
    moveToXY(off + 0 * SCALE, globalYOffset + 10 * SCALE);   // Flip X: (10,10)->(0,10)
    moveToXY(off + 0 * SCALE, globalYOffset + 0 * SCALE);    // Flip X: (10,0)->(0,0)
    moveToXY(off + 10 * SCALE, globalYOffset + 0 * SCALE);   // Flip X: (0,0)->(10,0)
    liftPen();
    Serial.println(F("[LETTER C] Complete"));
}
void drawLetterD(float off) {
    Serial.println(F("[LETTER D] Starting"));
    liftPen();
  
    // D: Apply both X and Y mirroring to fix upside-down orientation
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-0) * SCALE);   // was (0,0) -> now (10,10)
    lowerPen();
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-10) * SCALE);  // was (0,10) -> now (10,0)
    moveToXY(off + (10-6) * SCALE, globalYOffset + (10-10) * SCALE);  // was (6,10) -> now (4,0)
    moveToXY(off + (10-10) * SCALE, globalYOffset + (10-7) * SCALE);  // was (10,7) -> now (0,3)
    moveToXY(off + (10-10) * SCALE, globalYOffset + (10-3) * SCALE);  // was (10,3) -> now (0,7)
    moveToXY(off + (10-6) * SCALE, globalYOffset + (10-0) * SCALE);   // was (6,0) -> now (4,10)
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-0) * SCALE);   // was (0,0) -> now (10,10)
  
    liftPen();
    Serial.println(F("[LETTER D] Complete"));
}
void drawLetterE(float off) {
    Serial.println(F("[LETTER E] Starting"));
    liftPen();
  
    // E: Apply both X and Y mirroring to fix upside-down orientation
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-0) * SCALE);   // was (0,0) -> now (10,10)
    lowerPen();
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-10) * SCALE);  // was (0,10) -> now (10,0)
    moveToXY(off + (10-8) * SCALE, globalYOffset + (10-10) * SCALE);  // was (8,10) -> now (2,0)
    liftPen();
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-5) * SCALE);   // was (0,5) -> now (10,5)
    lowerPen();
    moveToXY(off + (10-6) * SCALE, globalYOffset + (10-5) * SCALE);   // was (6,5) -> now (4,5)
    liftPen();
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-0) * SCALE);   // was (0,0) -> now (10,10)
    lowerPen();
    moveToXY(off + (10-8) * SCALE, globalYOffset + (10-0) * SCALE);   // was (8,0) -> now (2,10)
  
    liftPen();
    Serial.println(F("[LETTER E] Complete"));
}
void drawLetterF(float off) {
    Serial.println(F("[LETTER F] Starting"));
    liftPen();
  
    // F: Apply both X and Y mirroring to fix upside-down orientation
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-0) * SCALE);   // was (0,0) -> now (10,10)
    lowerPen();
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-10) * SCALE);  // was (0,10) -> now (10,0)
    moveToXY(off + (10-8) * SCALE, globalYOffset + (10-10) * SCALE);  // was (8,10) -> now (2,0)
    liftPen();
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-5) * SCALE);   // was (0,5) -> now (10,5)
    lowerPen();
    moveToXY(off + (10-6) * SCALE, globalYOffset + (10-5) * SCALE);   // was (6,5) -> now (4,5)
  
    liftPen();
    Serial.println(F("[LETTER F] Complete"));
}
void drawLetterG(float off) {
    Serial.println(F("[LETTER G] Starting"));
    liftPen();
  
    // G: Apply both X and Y mirroring to fix upside-down orientation
    moveToXY(off + (10-8) * SCALE, globalYOffset + (10-10) * SCALE);  // was (8,10) -> now (2,0)
    lowerPen();
    moveToXY(off + (10-2) * SCALE, globalYOffset + (10-10) * SCALE);  // was (2,10) -> now (8,0)
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-7) * SCALE);   // was (0,7) -> now (10,3)
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-3) * SCALE);   // was (0,3) -> now (10,7)
    moveToXY(off + (10-2) * SCALE, globalYOffset + (10-0) * SCALE);   // was (2,0) -> now (8,10)
    moveToXY(off + (10-8) * SCALE, globalYOffset + (10-0) * SCALE);   // was (8,0) -> now (2,10)
    moveToXY(off + (10-8) * SCALE, globalYOffset + (10-5) * SCALE);   // was (8,5) -> now (2,5)
    moveToXY(off + (10-5) * SCALE, globalYOffset + (10-5) * SCALE);   // was (5,5) -> now (5,5)
  
    liftPen();
    Serial.println(F("[LETTER G] Complete"));
}
void drawLetterH(float off) {
    Serial.println(F("[LETTER H] Starting"));
    liftPen();
  
    // H: Apply both X and Y mirroring to fix upside-down orientation
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-0) * SCALE);   // was (0,0) -> now (10,10)
    lowerPen();
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-10) * SCALE);  // was (0,10) -> now (10,0)
    liftPen();
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-5) * SCALE);   // was (0,5) -> now (10,5)
    lowerPen();
    moveToXY(off + (10-8) * SCALE, globalYOffset + (10-5) * SCALE);   // was (8,5) -> now (2,5)
    liftPen();
    moveToXY(off + (10-8) * SCALE, globalYOffset + (10-0) * SCALE);   // was (8,0) -> now (2,10)
    lowerPen();
    moveToXY(off + (10-8) * SCALE, globalYOffset + (10-10) * SCALE);  // was (8,10) -> now (2,0)
  
    liftPen();
    Serial.println(F("[LETTER H] Complete"));
}
void drawLetterI(float off) {
    Serial.println(F("[LETTER I] Starting"));
    liftPen();
  
    // I: Apply both X and Y mirroring to fix upside-down orientation
    moveToXY(off + (10-2) * SCALE, globalYOffset + (10-10) * SCALE);  // was (2,10) -> now (8,0)
    lowerPen();
    moveToXY(off + (10-8) * SCALE, globalYOffset + (10-10) * SCALE);  // was (8,10) -> now (2,0)
    liftPen();
    moveToXY(off + (10-5) * SCALE, globalYOffset + (10-10) * SCALE);  // was (5,10) -> now (5,0)
    lowerPen();
    moveToXY(off + (10-5) * SCALE, globalYOffset + (10-0) * SCALE);   // was (5,0) -> now (5,10)
    liftPen();
    moveToXY(off + (10-2) * SCALE, globalYOffset + (10-0) * SCALE);   // was (2,0) -> now (8,10)
    lowerPen();
    moveToXY(off + (10-8) * SCALE, globalYOffset + (10-0) * SCALE);   // was (8,0) -> now (2,10)
  
    liftPen();
    Serial.println(F("[LETTER I] Complete"));
}
void drawLetterJ(float off) {
    Serial.println(F("[LETTER J] Starting"));
    liftPen();
  
    // J: Apply both X and Y mirroring to fix upside-down orientation
    moveToXY(off + (10-2) * SCALE, globalYOffset + (10-10) * SCALE);  // was (2,10) -> now (8,0)
    lowerPen();
    moveToXY(off + (10-8) * SCALE, globalYOffset + (10-10) * SCALE);  // was (8,10) -> now (2,0)
    liftPen();
    moveToXY(off + (10-6) * SCALE, globalYOffset + (10-10) * SCALE);  // was (6,10) -> now (4,0)
    lowerPen();
    moveToXY(off + (10-6) * SCALE, globalYOffset + (10-2) * SCALE);   // was (6,2) -> now (4,8)
    moveToXY(off + (10-4) * SCALE, globalYOffset + (10-0) * SCALE);   // was (4,0) -> now (6,10)
    moveToXY(off + (10-2) * SCALE, globalYOffset + (10-0) * SCALE);   // was (2,0) -> now (8,10)
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-2) * SCALE);   // was (0,2) -> now (10,8)
  
    liftPen();
    Serial.println(F("[LETTER J] Complete"));
}
void drawLetterK(float off) {
    Serial.println(F("[LETTER K] Starting"));
    liftPen();
  
    // K: Apply both X and Y mirroring to fix upside-down orientation
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-0) * SCALE);   // was (0,0) -> now (10,10)
    lowerPen();
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-10) * SCALE);  // was (0,10) -> now (10,0)
    liftPen();
    moveToXY(off + (10-8) * SCALE, globalYOffset + (10-10) * SCALE);  // was (8,10) -> now (2,0)
    lowerPen();
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-5) * SCALE);   // was (0,5) -> now (10,5)
    moveToXY(off + (10-8) * SCALE, globalYOffset + (10-0) * SCALE);   // was (8,0) -> now (2,10)
  
    liftPen();
    Serial.println(F("[LETTER K] Complete"));
}
void drawLetterL(float off) {
    Serial.println(F("[LETTER L] Starting"));
    liftPen();
    moveToXY(off + 0 * SCALE, globalYOffset + 10 * SCALE);
    lowerPen();
    moveToXY(off + 0 * SCALE, globalYOffset + 0 * SCALE);
    moveToXY(off + 10 * SCALE, globalYOffset + 0 * SCALE);
    liftPen();
    Serial.println(F("[LETTER L] Complete"));
}
void drawLetterM(float off) {
    Serial.println(F("[LETTER M] Starting"));
    liftPen();
  
    // M: Apply both X and Y mirroring to fix upside-down orientation
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-0) * SCALE);   // was (0,0) -> now (10,10)
    lowerPen();
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-10) * SCALE);  // was (0,10) -> now (10,0)
    moveToXY(off + (10-5) * SCALE, globalYOffset + (10-5) * SCALE);   // was (5,5) -> now (5,5)
    moveToXY(off + (10-10) * SCALE, globalYOffset + (10-10) * SCALE); // was (10,10) -> now (0,0)
    moveToXY(off + (10-10) * SCALE, globalYOffset + (10-0) * SCALE);  // was (10,0) -> now (0,10)
  
    liftPen();
    Serial.println(F("[LETTER M] Complete"));
}
void drawLetterN(float off) {
    Serial.println(F("[LETTER N] Starting"));
    liftPen();
  
    // N: Apply both X and Y mirroring to fix upside-down orientation
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-0) * SCALE);   // was (0,0) -> now (10,10)
    lowerPen();
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-10) * SCALE);  // was (0,10) -> now (10,0)
    moveToXY(off + (10-8) * SCALE, globalYOffset + (10-0) * SCALE);   // was (8,0) -> now (2,10)
    moveToXY(off + (10-8) * SCALE, globalYOffset + (10-10) * SCALE);  // was (8,10) -> now (2,0)
  
    liftPen();
    Serial.println(F("[LETTER N] Complete"));
}
void drawLetterO(float off) {
    Serial.println(F("[LETTER O] Starting"));
    liftPen();
  
    // O: Apply both X and Y mirroring to fix upside-down orientation
    moveToXY(off + (10-2) * SCALE, globalYOffset + (10-10) * SCALE);  // was (2,10) -> now (8,0)
    lowerPen();
    moveToXY(off + (10-8) * SCALE, globalYOffset + (10-10) * SCALE);  // was (8,10) -> now (2,0)
    moveToXY(off + (10-10) * SCALE, globalYOffset + (10-7) * SCALE);  // was (10,7) -> now (0,3)
    moveToXY(off + (10-10) * SCALE, globalYOffset + (10-3) * SCALE);  // was (10,3) -> now (0,7)
    moveToXY(off + (10-8) * SCALE, globalYOffset + (10-0) * SCALE);   // was (8,0) -> now (2,10)
    moveToXY(off + (10-2) * SCALE, globalYOffset + (10-0) * SCALE);   // was (2,0) -> now (8,10)
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-3) * SCALE);   // was (0,3) -> now (10,7)
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-7) * SCALE);   // was (0,7) -> now (10,3)
    moveToXY(off + (10-2) * SCALE, globalYOffset + (10-10) * SCALE);  // was (2,10) -> now (8,0)
  
    liftPen();
    Serial.println(F("[LETTER O] Complete"));
}
void drawLetterP(float off) {
    Serial.println(F("[LETTER P] Starting"));
    liftPen();
  
    // P: Apply both X and Y mirroring to fix upside-down orientation
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-0) * SCALE);   // was (0,0) -> now (10,10)
    lowerPen();
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-10) * SCALE);  // was (0,10) -> now (10,0)
    moveToXY(off + (10-6) * SCALE, globalYOffset + (10-10) * SCALE);  // was (6,10) -> now (4,0)
    moveToXY(off + (10-8) * SCALE, globalYOffset + (10-8) * SCALE);   // was (8,8) -> now (2,2)
    moveToXY(off + (10-8) * SCALE, globalYOffset + (10-6) * SCALE);   // was (8,6) -> now (2,4)
    moveToXY(off + (10-6) * SCALE, globalYOffset + (10-5) * SCALE);   // was (6,5) -> now (4,5)
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-5) * SCALE);   // was (0,5) -> now (10,5)
  
    liftPen();
    Serial.println(F("[LETTER P] Complete"));
}
void drawLetterQ(float off) {
    Serial.println(F("[LETTER Q] Starting"));
    liftPen();
  
    // Q: Apply both X and Y mirroring to fix upside-down orientation
    moveToXY(off + (10-2) * SCALE, globalYOffset + (10-10) * SCALE);  // was (2,10) -> now (8,0)
    lowerPen();
    moveToXY(off + (10-8) * SCALE, globalYOffset + (10-10) * SCALE);  // was (8,10) -> now (2,0)
    moveToXY(off + (10-10) * SCALE, globalYOffset + (10-7) * SCALE);  // was (10,7) -> now (0,3)
    moveToXY(off + (10-10) * SCALE, globalYOffset + (10-3) * SCALE);  // was (10,3) -> now (0,7)
    moveToXY(off + (10-8) * SCALE, globalYOffset + (10-0) * SCALE);   // was (8,0) -> now (2,10)
    moveToXY(off + (10-2) * SCALE, globalYOffset + (10-0) * SCALE);   // was (2,0) -> now (8,10)
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-3) * SCALE);   // was (0,3) -> now (10,7)
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-7) * SCALE);   // was (0,7) -> now (10,3)
    moveToXY(off + (10-2) * SCALE, globalYOffset + (10-10) * SCALE);  // was (2,10) -> now (8,0)
    liftPen();
    moveToXY(off + (10-6) * SCALE, globalYOffset + (10-3) * SCALE);   // was (6,3) -> now (4,7)
    lowerPen();
    moveToXY(off + (10-10) * SCALE, globalYOffset + (10-0) * SCALE);  // was (10,0) -> now (0,10)
  
    liftPen();
    Serial.println(F("[LETTER Q] Complete"));
}
void drawLetterR(float off) {
    Serial.println(F("[LETTER R] Starting"));
    liftPen();
  
    // R: Apply both X and Y mirroring to fix upside-down orientation
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-0) * SCALE);   // was (0,0) -> now (10,10)
    lowerPen();
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-10) * SCALE);  // was (0,10) -> now (10,0)
    moveToXY(off + (10-6) * SCALE, globalYOffset + (10-10) * SCALE);  // was (6,10) -> now (4,0)
    moveToXY(off + (10-8) * SCALE, globalYOffset + (10-8) * SCALE);   // was (8,8) -> now (2,2)
    moveToXY(off + (10-8) * SCALE, globalYOffset + (10-6) * SCALE);   // was (8,6) -> now (2,4)
    moveToXY(off + (10-6) * SCALE, globalYOffset + (10-5) * SCALE);   // was (6,5) -> now (4,5)
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-5) * SCALE);   // was (0,5) -> now (10,5)
    liftPen();
    moveToXY(off + (10-5) * SCALE, globalYOffset + (10-5) * SCALE);   // was (5,5) -> now (5,5)
    lowerPen();
    moveToXY(off + (10-8) * SCALE, globalYOffset + (10-0) * SCALE);   // was (8,0) -> now (2,10)
  
    liftPen();
    Serial.println(F("[LETTER R] Complete"));
}
void drawLetterS(float off) {
    Serial.println(F("[LETTER S] Starting"));
    liftPen();
  
    // S: Apply both X and Y mirroring to fix upside-down orientation
    moveToXY(off + (10-8) * SCALE, globalYOffset + (10-10) * SCALE);  // was (8,10) -> now (2,0)
    lowerPen();
    moveToXY(off + (10-2) * SCALE, globalYOffset + (10-10) * SCALE);  // was (2,10) -> now (8,0)
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-8) * SCALE);   // was (0,8) -> now (10,2)
    moveToXY(off + (10-2) * SCALE, globalYOffset + (10-6) * SCALE);   // was (2,6) -> now (8,4)
    moveToXY(off + (10-8) * SCALE, globalYOffset + (10-4) * SCALE);   // was (8,4) -> now (2,6)
    moveToXY(off + (10-10) * SCALE, globalYOffset + (10-2) * SCALE);  // was (10,2) -> now (0,8)
    moveToXY(off + (10-2) * SCALE, globalYOffset + (10-0) * SCALE);   // was (2,0) -> now (8,10)
  
    liftPen();
    Serial.println(F("[LETTER S] Complete"));
}
void drawLetterT(float off) {
    Serial.println(F("[LETTER T] Starting"));
    liftPen();
  
    // T: Top horizontal line (Y=10) and vertical stem down the middle
    // Apply both X and Y mirroring to fix upside-down orientation
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-10) * SCALE);  // was (0,10) -> now (10,0)
    lowerPen();
    moveToXY(off + (10-10) * SCALE, globalYOffset + (10-10) * SCALE); // was (10,10) -> now (0,0)
    moveToXY(off + (10-5) * SCALE, globalYOffset + (10-10) * SCALE);  // was (5,10) -> now (5,0)
    moveToXY(off + (10-5) * SCALE, globalYOffset + (10-0) * SCALE);   // was (5,0) -> now (5,10)
  
    liftPen();
    Serial.println(F("[LETTER T] Complete"));
}
void drawLetterU(float off) {
    Serial.println(F("[LETTER U] Starting"));
    liftPen();
  
    // U: Apply both X and Y mirroring to fix upside-down orientation
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-10) * SCALE);  // was (0,10) -> now (10,0)
    lowerPen();
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-2) * SCALE);   // was (0,2) -> now (10,8)
    moveToXY(off + (10-2) * SCALE, globalYOffset + (10-0) * SCALE);   // was (2,0) -> now (8,10)
    moveToXY(off + (10-8) * SCALE, globalYOffset + (10-0) * SCALE);   // was (8,0) -> now (2,10)
    moveToXY(off + (10-10) * SCALE, globalYOffset + (10-2) * SCALE);  // was (10,2) -> now (0,8)
    moveToXY(off + (10-10) * SCALE, globalYOffset + (10-10) * SCALE); // was (10,10) -> now (0,0)
  
    liftPen();
    Serial.println(F("[LETTER U] Complete"));
}
void drawLetterV(float off) {
    Serial.println(F("[LETTER V] Starting"));
    liftPen();
  
    // V: Apply both X and Y mirroring to fix upside-down orientation
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-10) * SCALE);  // was (0,10) -> now (10,0)
    lowerPen();
    moveToXY(off + (10-5) * SCALE, globalYOffset + (10-0) * SCALE);   // was (5,0) -> now (5,10)
    moveToXY(off + (10-10) * SCALE, globalYOffset + (10-10) * SCALE); // was (10,10) -> now (0,0)
  
    liftPen();
    Serial.println(F("[LETTER V] Complete"));
}
void drawLetterW(float off) {
    Serial.println(F("[LETTER W] Starting"));
    liftPen();
  
    // W: Apply both X and Y mirroring to fix upside-down orientation
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-10) * SCALE);  // was (0,10) -> now (10,0)
    lowerPen();
    moveToXY(off + (10-3) * SCALE, globalYOffset + (10-0) * SCALE);   // was (3,0) -> now (7,10)
    moveToXY(off + (10-5) * SCALE, globalYOffset + (10-5) * SCALE);   // was (5,5) -> now (5,5)
    moveToXY(off + (10-7) * SCALE, globalYOffset + (10-0) * SCALE);   // was (7,0) -> now (3,10)
    moveToXY(off + (10-10) * SCALE, globalYOffset + (10-10) * SCALE); // was (10,10) -> now (0,0)
  
    liftPen();
    Serial.println(F("[LETTER W] Complete"));
}
void drawLetterX(float off) {
    Serial.println(F("[LETTER X] Starting"));
    liftPen();
  
    // X: Apply both X and Y mirroring to fix upside-down orientation
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-10) * SCALE);  // was (0,10) -> now (10,0)
    lowerPen();
    moveToXY(off + (10-10) * SCALE, globalYOffset + (10-0) * SCALE);  // was (10,0) -> now (0,10)
    liftPen();
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-0) * SCALE);   // was (0,0) -> now (10,10)
    lowerPen();
    moveToXY(off + (10-10) * SCALE, globalYOffset + (10-10) * SCALE); // was (10,10) -> now (0,0)
  
    liftPen();
    Serial.println(F("[LETTER X] Complete"));
}
void drawLetterY(float off) {
    Serial.println(F("[LETTER Y] Starting"));
    liftPen();
    moveToXY(off + 10 * SCALE, globalYOffset + 0 * SCALE);   // Reflect both X and Y: (0,10)->(10,0)
    lowerPen();
    moveToXY(off + 5 * SCALE, globalYOffset + 5 * SCALE);    // Reflect both X and Y: (5,5)->(5,5)
    moveToXY(off + 0 * SCALE, globalYOffset + 0 * SCALE);    // Reflect both X and Y: (10,10)->(0,0)
    liftPen();
    moveToXY(off + 5 * SCALE, globalYOffset + 5 * SCALE);    // Reflect both X and Y: (5,5)->(5,5)
    lowerPen();
    moveToXY(off + 5 * SCALE, globalYOffset + 10 * SCALE);   // Reflect both X and Y: (5,0)->(5,10)
    liftPen();
    Serial.println(F("[LETTER Y] Complete"));
}
void drawLetterZ(float off) {
    Serial.println(F("[LETTER Z] Starting"));
    liftPen();
  
    // Z: Apply both X and Y mirroring to fix upside-down orientation
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-10) * SCALE);  // was (0,10) -> now (10,0)
    lowerPen();
    moveToXY(off + (10-10) * SCALE, globalYOffset + (10-10) * SCALE); // was (10,10) -> now (0,0)
    moveToXY(off + (10-0) * SCALE, globalYOffset + (10-0) * SCALE);   // was (0,0) -> now (10,10)
    moveToXY(off + (10-10) * SCALE, globalYOffset + (10-0) * SCALE);  // was (10,0) -> now (0,10)
  
    liftPen();
    Serial.println(F("[LETTER Z] Complete"));
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
    // Debug heartbeat every 30 seconds (reduced from 5 seconds)
    static unsigned long lastHeartbeat = 0;
    if (millis() - lastHeartbeat > 30000) {
        Serial.print(F("[Heartbeat] Loop running, Serial.available() = "));
        Serial.println(Serial.available());
        lastHeartbeat = millis();
    }
    
    // Check for reset button press (TEMPORARILY DISABLED FOR DEBUGGING)
    /*
    if (digitalRead(BUTTON_PIN) == LOW) {
        delay(50);  // Debounce
        if (digitalRead(BUTTON_PIN) == LOW) {
            Serial.println(F("Reset pressed"));
            resetSystem();
        }
        while (digitalRead(BUTTON_PIN) == LOW);  // Wait for release
    }
    */

    // Position offset tracking for letter drawing
    // Letters are drawn in a 0-based coordinate system
    // Then we add HOME_X and HOME_Y when actually moving
    static float xOff = 0;  // Start at 0mm in drawing space
    static float yOff = 0;  // Start at 0mm in drawing space
    static char word[32];  // Buffer for incoming text
    
    // Check for serial input
    if (Serial.available()) {
        Serial.print(F(">>> Serial available! Bytes: "));
        Serial.println(Serial.available());
        
        int len = Serial.readBytesUntil('\n', word, sizeof(word) - 1);
        word[len] = '\0';  // Null terminate
        
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
        
        // Display original input on LCD before processing
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Input: ");
        lcd.setCursor(0, 1);
        lcd.print(word);  // Show original input
        delay(1000);  // Show for 1 second
        
        // Convert to uppercase
        for (int i = 0; i < len; i++) {
            if (word[i] >= 'a' && word[i] <= 'z') {
                word[i] = word[i] - 32;
            }
        }

        Serial.print(F("Processing (uppercase): "));
        Serial.println(word);
        
        // Check for STOP command (kill switch)
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
            
            // Halt forever until reset
            while(true) {
                delay(1000);
            }
        }
        
        // Check for motor test commands
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
            Serial.print(F("Current pos: X="));
            Serial.print(currentXmm);
            Serial.print(F("mm, Y="));
            Serial.print(currentYmm);
            Serial.println(F("mm"));
            moveToXY(currentXmm - 20, currentYmm);
            // Reset AccelStepper and tracking to origin
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
            Serial.print(F("Current pos: X="));
            Serial.print(currentXmm);
            Serial.print(F("mm, Y="));
            Serial.print(currentYmm);
            Serial.println(F("mm"));
            moveToXY(currentXmm + 20, currentYmm);
            // Reset AccelStepper and tracking to origin
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
            Serial.print(F("Current pos: X="));
            Serial.print(currentXmm);
            Serial.print(F("mm, Y="));
            Serial.print(currentYmm);
            Serial.println(F("mm"));
            moveToXY(currentXmm, currentYmm + 20);
            // Reset AccelStepper and tracking to origin
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
            Serial.print(F("Current pos: X="));
            Serial.print(currentXmm);
            Serial.print(F("mm, Y="));
            Serial.print(currentYmm);
            Serial.println(F("mm"));
            moveToXY(currentXmm, currentYmm - 20);
            // Reset AccelStepper and tracking to origin
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
        lcd.print(word);  // Will auto-truncate at 16 chars
        
        delay(1000);  // Show received text for 1 second
        
        digitalWrite(LED_PIN, HIGH);  // LED ON when writing

        liftPen();
        moveToXY(xOff, yOff);
        
        // Update LCD to show writing in progress
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

            globalYOffset = yOff;  // Set Y offset for letter drawing
            drawLetter(c, xOff);
            
            Serial.println(F(""));
            Serial.print(F(">>> LETTER '"));
            Serial.print(c);
            Serial.println(F("' DRAWING COMPLETE <<<"));
            Serial.println(F(""));

            xOff += letterWidth + BASE_LETTER_SPACING;

            // Check if we need to wrap to next line
            if (xOff > MAX_LINE_WIDTH && i < len - 1) {  // Don't wrap after last letter
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

        digitalWrite(LED_PIN, LOW);  // LED OFF when done writing
        lcd.clear();
        lcd.print(F("DONE!"));
        Serial.println(F("DRAWING_COMPLETE"));
        Serial.println(F("========================================="));

        delay(2000);

        // Reset position for next word - return to physical starting position
        xOff = HOME_X;  // Back to 20mm (center)
        yOff = HOME_Y;  // Back to 40mm (back/top)
        goHome();
        displayMessage("Ready to draw!", "Input Required");
        Serial.println(F("Ready for next input..."));
    }
    
    // Small delay to prevent tight loop
    delay(10);
}
