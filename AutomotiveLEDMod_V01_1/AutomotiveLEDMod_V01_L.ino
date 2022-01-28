/*
  MechLabs AutomotiveLEDMod Functional Program

  Reads binary signal from comparator circuit.
  Reads binary state from dip switch array.
  Selects state from digital input pins.
  Outputs data string for states animation of RGB's.

  States table
   PLRB Park, Indicator Left, Indicator Right, Brake
   0000/x All off
   xxx1/0 Brake on, rear
   x001/1 Brake on, front (alternate function)
   xxx0/x Brake off
   xxx0/1 Brake off, front
   x1x0/x Left  indicator on, brake off
   xx10/x Right indicator on, brake off
   x1x1/0 Left  indicator on, brake on, rear
   xx11/0 Right indicator on, brake on, rear
   x0xx/x Left  indicator off
   xx0x/x Right indicator off
   1000/x Parker only
      
  Serial output for debug only.

  Hardware Connections Left to Right
    Blade 1 > 12V Supply High Current
    Blade 2 > 12V Ignition signal
    Blade 3 > 0V Ground High Current
    Blade 4 > Park lights signal
    Blade 5 > J2 Output Indicator signal
    Blade 6 > J4 Output Indicator signal
    Blade 7 > Brake light signal
    3Pin J2 > Output | 5V | Data | 0V | High current
    3Pin J4 > Output | 5V | Data | 0V | High current
    
  Last edit 28 01 2022
  by DJPriaulx
*/

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

#define baseCount 10                      // Base number of LEDs in array
#define cycleDelay 15                     // Cycle time
#define cycleTrigger 10000                // No of cycles on park before animate
#define brakeFlashDelay 100               // Millisecond delay between brake annimation steps
#define divisor 4                         // Division for solid fill section of indicator animation
#define flashes 3                         // Number of flashes at brake state start
#define maxLEDs 72                        // Maximum LEDs in array

#define PRK 16                            // Input signals from comparator
#define LFT 17
#define RGT 18
#define BRK 19
                                          
#define F01 34                            // +2
#define F02 35                            // +4
#define F03 32                            // +8
#define F04 33                            // +16
#define F05 25                            // +32
#define F06 26                            // Front (on), Rear (off)
#define F07 27                            // Debug
/* F08 is hardware selection for ignition enable or Vin enable */

#define dataLHS 5                         // Data output pin
#define dataRHS 4                         // Data output pin

int statePRK = 0;                         // Variable for status of external inputs
int stateLFT = 0;
int stateRGT = 0;
int stateBRK = 0;
                                     
int dipAddition[] = {2, 4, 8, 16, 32};    // Value of array length dip switchs
boolean dipStates[] = {0,0,0,0,0};        // State of dip switches
boolean dipDebug = false;
boolean dipFront = false;

int brakeCheck = 0;                       // Temp variables
unsigned long cycleTimer;
unsigned long cycleCount;
unsigned long brakeFlashTime;
int annimationCount = 0;
int halfArray = 0;
boolean lastStateBRK = 0;
int lengthOfArray = maxLEDs;
int preFill = 0;
int wipeLHS = 0;
int wipeRHS = 0;
int wipePos = 0;
int wipeNeg = lengthOfArray;

                                          // Initialise library
Adafruit_NeoPixel stripLHS(lengthOfArray, dataLHS, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel stripRHS(lengthOfArray, dataRHS, NEO_GRB + NEO_KHZ800);

                                          // Colour presets
uint32_t colRed = stripLHS.Color(255, 0, 0);
uint32_t colRedDim = stripLHS.Color(40, 0, 0);
uint32_t colAmber = stripLHS.Color(255, 191, 0);
uint32_t colWhite = stripLHS.Color(255, 255, 255);
uint32_t colPurple = stripLHS.Color(204, 153, 255);
uint32_t colNone = stripLHS.Color(0, 0, 0);

uint32_t colPark = stripLHS.Color(0, 0, 0); // Variable colour holder


void setup() {
  stripLHS.begin();                       // Start library
  stripRHS.begin();
  stripLHS.clear();                       // Set LED data to off
  stripRHS.clear();
  stripLHS.show();                        // Action data to LED arrays
  stripRHS.show();
  
  Serial.begin(115200);                   // Initialize serial communications at 9600baud
  
  pinMode(PRK, INPUT_PULLUP);             // Set pull up by software to match circuit output
  pinMode(LFT, INPUT_PULLUP);
  pinMode(RGT, INPUT_PULLUP);
  pinMode(BRK, INPUT_PULLUP);
  pinMode(F01, INPUT);
  pinMode(F02, INPUT);
  pinMode(F03, INPUT);
  pinMode(F04, INPUT);
  pinMode(F05, INPUT);
  pinMode(F06, INPUT);
  pinMode(F07, INPUT);

  dipStates[0] = digitalRead(F01);        // Read dip switch states to array
  dipStates[1] = digitalRead(F02);
  dipStates[2] = digitalRead(F03);
  dipStates[3] = digitalRead(F04);
  dipStates[4] = digitalRead(F05);
  dipFront = digitalRead(F06);            // Read dip switch for front or not front
  dipDebug = digitalRead(F07);            // Read dip for alternate functions
                                      
  lengthOfArray = baseCount;              // Calculate length of LED array from dip positions                            
  for (int i = 0; i < sizeof(dipStates); i++) {
    if (dipStates[i]==true) {
      lengthOfArray = lengthOfArray + dipAddition[i];
    }
  }
  
  stripLHS.updateLength(lengthOfArray);   // Change length of LED array
  stripRHS.updateLength(lengthOfArray);
  
  halfArray = int(lengthOfArray/2);       // Set mid point of LED array
  wipeNeg = lengthOfArray;                // Set reverse wipe counter

  if (divisor != 0) {                     // Set indicator solid fill length
    preFill = int(lengthOfArray/divisor);
  }

  if (dipFront){                          // Set front or rear
    colPark = colWhite;
  }
  else {
    colPark = colRedDim;
  }

  statePRK = digitalRead(PRK);            // Read parker input

  if (statePRK == 1){
    while (wipeNeg >= 0){                 // Inro annimation
      stripLHS.setPixelColor(wipeNeg, colPark);
      stripRHS.setPixelColor(wipeNeg, colPark);
      stripLHS.show();
      stripRHS.show();
      delay(cycleDelay);
      wipeNeg--;
    }
  }
  wipeNeg = lengthOfArray;

}

void loop() {
  cycleTimer = millis();                  // Set cycle start time
  
  statePRK = digitalRead(PRK);            // Read inputs
  stateLFT = digitalRead(LFT);
  stateRGT = digitalRead(RGT);
  stateBRK = digitalRead(BRK);
  dipDebug = digitalRead(F07);
      
// 0000 All off
  if (statePRK == 0 && stateLFT == 0 && stateRGT == 0 && stateBRK == 0){
    stripLHS.clear();
    stripRHS.clear();
  }

// xxx1/0 Brake on, rear
  if (stateBRK == 1 && dipFront == 0){
    if (brakeCheck < flashes*2){          // Flash brakes at start of brake state
      if (brakeCheck & 0x01) {            // Odd counts clear array
        stripLHS.clear();
        stripRHS.clear();
      }
      else {                              // Even counts fill array
        stripLHS.fill(colRed);
        stripRHS.fill(colRed);
      }
    }
    else {                                // After flashes fill array
      stripLHS.fill(colRed);
      stripRHS.fill(colRed);
    }
    if (cycleTimer - brakeFlashTime >= brakeFlashDelay) {
      brakeFlashTime = cycleTimer;
      brakeCheck ++;
    }
  }

// x001/1 Indicators off, Brake on, front
  if (stateLFT == 0 && stateRGT == 0 && stateBRK == 1 && dipFront == 1){
    if (annimationCount <= lengthOfArray*2){
      stripLHS.fill(colWhite);
      stripRHS.fill(colWhite);
      if (wipeNeg >= 0) {                 // Annimation scroll from end to start
        stripLHS.fill(colPurple, wipeNeg);
        stripRHS.fill(colPurple, wipeNeg);
      }
      if (wipeNeg-1 >= 0){                // Lead scroll with none
        stripLHS.fill(colNone, wipeNeg-1, 1);
        stripRHS.fill(colNone, wipeNeg-1, 1);
      }
    }
    else if (annimationCount <= lengthOfArray*4){
      stripLHS.fill(colPurple);
      stripRHS.fill(colPurple);
      if (wipeNeg >= 0) {                 // Annimation scroll from end to start
        stripLHS.fill(colWhite, wipeNeg);
        stripRHS.fill(colWhite, wipeNeg);
      }
      if (wipeNeg-1 >= 0){                // Lead scroll with none
        stripLHS.fill(colNone, wipeNeg-1, 2);
        stripRHS.fill(colNone, wipeNeg-1, 2);
      }
    }
    else {
      annimationCount = 0;                // Reset annimation cycle counter
    }
    if (annimationCount & 0x01) {
      wipeNeg--;                          // Decrement negative wipe counter
      wipePos++;                          // Increment positive wipe counter
    }
    annimationCount ++;                   // Increment annimation cycle counter
  }

// xxx0/x Brake off
  if (stateBRK == 0){
    brakeCheck = 0;                       // Reset brake flash counter
  }

// xxx0/1 Brake off, front
  if (stateBRK == 0 && dipFront == 1){
    annimationCount = 0;                  // Reset annimation cycle counter
  }
  
// x1x0/x Left indicator on, brake off
  if (stateLFT == 1 && stateBRK == 0){
    if (wipeLHS <= lengthOfArray){        // Fill section and scroll remaining
      if (wipeLHS <= preFill) {
        wipeLHS = preFill;
      }
      stripLHS.fill(colAmber, 0, preFill);
      stripLHS.setPixelColor(wipeLHS, colAmber);
      if (wipeLHS+1 <= lengthOfArray){    // Lead scroll with none
        stripLHS.fill(colNone, wipeLHS+1, 3);
      }
      wipeLHS++;
    }
  }
  
// xx10/x Right indicator on, brake off
  if (stateRGT == 1 && stateBRK == 0){
    if (wipeRHS <= lengthOfArray){        // Fill section and scroll remaining
      if (wipeRHS <= preFill) {
        wipeRHS = preFill;
      }
      stripRHS.fill(colAmber, 0, preFill);
      stripRHS.setPixelColor(wipeRHS, colAmber);
      if (wipeRHS+1 <= lengthOfArray){    // Lead scroll with none
        stripRHS.fill(colNone, wipeRHS+1, 3);
      }
      wipeRHS++;
    }
  }

// x1x1/0 Left indicator on, brake on, rear
  if (stateLFT == 1 && stateBRK == 1 && dipFront == 0){
    if (wipeLHS <= halfArray +1) {
      wipeLHS = halfArray +1 ;            // Start at last half
    }                                     // Fill section and scroll remaining
    stripLHS.fill(colNone, halfArray);    // Clear scrolling end
    stripLHS.fill(colAmber, halfArray +1, wipeLHS-halfArray);
    wipeLHS++;
  }

// xx11/0 Right indicator on, brake on, rear
  if (stateRGT == 1 && stateBRK == 1 && dipFront == 0){
    if (wipeRHS <= halfArray +1) {
      wipeRHS = halfArray +1;             // Start at last half
    }                                     // Fill section and scroll remaining
    stripRHS.fill(colNone, halfArray);    // Clear scrolling end
    stripRHS.fill(colAmber, halfArray +1, wipeRHS-halfArray);
    wipeRHS++;
  }
  

// x0xx/x Left indicator off
  if (stateLFT == 0){                     // Reset wipe
    wipeLHS = 0;
  }
  
// xx0x/x Right indicator off
  if (stateRGT == 0){                     // Reset wipe
    wipeRHS = 0;
  }

// 1000/x Parker only
  if (statePRK == 1 && stateLFT == 0 && stateRGT == 0 && stateBRK == 0){
    stripLHS.fill(colPark);
    stripRHS.fill(colPark);
          
    if (cycleCount > cycleTrigger && cycleCount <= cycleTrigger + lengthOfArray){
      if (dipFront){ stripLHS.setPixelColor(wipeNeg, colNone); wipeNeg--; }
      else {      stripLHS.setPixelColor(wipePos, colNone);   wipePos++; }
    }
    else if (cycleCount > cycleTrigger + lengthOfArray && cycleCount < cycleTrigger + lengthOfArray*2){
      if (dipFront){ stripRHS.setPixelColor(wipePos, colNone); wipePos++; }
      else {      stripLHS.setPixelColor(wipeNeg, colNone);   wipeNeg--; }
    }
    else if (cycleCount > cycleTrigger + lengthOfArray*2 && cycleCount < cycleTrigger + lengthOfArray*3){
      if (dipFront){ stripLHS.setPixelColor(wipeNeg, colNone); wipeNeg--; }
      else {      stripLHS.setPixelColor(wipePos, colNone);   wipePos++; }
    }
    else if (cycleCount > cycleTrigger + lengthOfArray*3 && cycleCount < cycleTrigger + lengthOfArray*4){
      if (dipFront){ stripRHS.setPixelColor(wipePos, colNone); wipePos++; }
      else {      stripLHS.setPixelColor(wipeNeg, colNone);   wipeNeg--; }
    }
    else if (cycleCount > cycleTrigger + lengthOfArray*3){
      cycleCount = 0; }
  }

// Action changes
  stripLHS.show();                        // Trigger LEDs
  stripRHS.show();                        // Trigger LEDs
  
// Reset wipePos
  if (wipePos > lengthOfArray) {
    wipePos = 0;
  }

// Reset wipeNeg
  if (wipeNeg < 1) {
    wipeNeg = lengthOfArray;
  }

// Debug
  while (dipDebug){
    dipStates[0] = digitalRead(F01);      // Read dip switch states
    dipStates[1] = digitalRead(F02);
    dipStates[2] = digitalRead(F03);
    dipStates[3] = digitalRead(F04);
    dipStates[4] = digitalRead(F05);
    dipFront = digitalRead(F06);
    dipDebug = digitalRead(F07);

    lengthOfArray = baseCount;            // Calculate length of LED array from dip positions                            
    for (int i = 0; i < sizeof(dipStates); i++) {
      if (dipStates[i]==true) {
        lengthOfArray = lengthOfArray + dipAddition[i];
      }
    }
    stripLHS.updateLength(lengthOfArray); // Change length of LED array
    stripRHS.updateLength(lengthOfArray);
    
    halfArray = int(lengthOfArray/2);     // Set mid point of LED array
    wipeNeg = lengthOfArray;              // Set reverse wipe counter
  
    if (divisor != 0) {                   // Set indicator solid fill length
      preFill = int(lengthOfArray/divisor);
    }
  
    if (dipFront){                        // Set front or rear
      colPark = colPurple;
    }
    else {
      colPark = colRedDim;
    }
    
    while (wipeNeg >= 0){                 // Intro annimation
      stripLHS.setPixelColor(wipeNeg, colPark);
      stripRHS.setPixelColor(wipeNeg, colPark);
      delay(cycleDelay);
      wipeNeg--;
    }
    wipeNeg = lengthOfArray;

    while (wipePos <= lengthOfArray){     // Intro annimation
      stripLHS.setPixelColor(wipePos, colPark);
      stripRHS.setPixelColor(wipePos, colPark);
      delay(cycleDelay);
      wipePos++;
    }
    wipePos = 0;
    
  // Print to serial for debug
    Serial.print("Length = ");
    Serial.print(lengthOfArray);
    Serial.print("   Delay = ");
    Serial.print(cycleDelay);
    Serial.print("   CycleNo = ");
    Serial.print(cycleCount);
    Serial.print("   PRK = ");
    Serial.print(statePRK);
    Serial.print("   LFT = ");
    Serial.print(stateLFT);
    Serial.print("   RGT = ");
    Serial.print(stateRGT);
    Serial.print("   BRK = ");
    Serial.print(stateBRK);
    Serial.print("   Dips = ");
    for (int i = 0; i < sizeof(dipStates); i++){
      Serial.print(dipStates[i]);
      Serial.print(", ");
    }
    Serial.println();
  }
  
// Time loop control for cycle
  lastStateBRK = stateBRK;                // Exit timer control if brake change
  while ((millis() - cycleTimer) < cycleDelay && stateBRK == lastStateBRK){
    delay(1);
    stateBRK = digitalRead(BRK);
  }
    
  cycleCount++;
}
