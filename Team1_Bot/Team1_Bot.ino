/*

 MSE 2202 Team 1 Bot Drive System Code
 Language: Arduino
 Author: Mia Macdonald-Walden

 Rev 1  - Initial Version March 20 2024
 Rev 2  - Changed drive system path

*/

//  To program and use ESP32-S3
//   
//  Tools->:
//  Board: "Adafruit Feather ESP32-S3 No PSRAM"
//  Upload Speed: "921600"
//  USB CDC On Boot: "Enabled"
//  USB Firmware MSC on Boot: "Disabled"
//  USB DFU On Bot: "Disabled"
//  Upload Mode:"UART0/Hardware CDC"
//  SPU Frequency: "240MHz (WiFi)"
//  Flash Mode: "QIO 80MHz"
//  Flash SIze: "4MB (32Mb)"
//  Partition Scheme: "Default 4MB with spiffs (1.2MB app/1.5MB SPIFFS)"
//  Core Debug Level: "Verbose"
//  PSRAM: 'Disabled"
//  Arduino Runs On: "Core 1"
//  Events Run On: "Core 1"
//
//  To program, press and hold the reset button then press and hold program button, release the reset button then 
//  release the program button 
//

//Debugging Keywords
//

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <MSE2202_Lib.h>


// Encoder structure
struct Encoder {
   const int chanA;                                                            // GPIO pin for encoder channel A
   const int chanB;                                                            // GPIO pin for encoder channel B
   long pos;                                                                   // current encoder position
};

// Port pin constants
#define LEFT_MOTOR_A        35                                                 // GPIO35 pin 28 (J35) Motor 1 A
#define LEFT_MOTOR_B        36                                                 // GPIO36 pin 29 (J36) Motor 1 B
#define RIGHT_MOTOR_A       37                                                 // GPIO37 pin 30 (J37) Motor 2 A
#define RIGHT_MOTOR_B       38                                                 // GPIO38 pin 31 (J38) Motor 2 B
#define ENCODER_LEFT_A      15                                                 // left encoder A signal is connected to pin 8 GPIO15 (J15)
#define ENCODER_LEFT_B      16                                                 // left encoder B signal is connected to pin 8 GPIO16 (J16)
#define ENCODER_RIGHT_A     11                                                 // right encoder A signal is connected to pin 19 GPIO11 (J11)
#define ENCODER_RIGHT_B     12                                                 // right encoder B signal is connected to pin 20 GPIO12 (J12)
#define MODE_BUTTON         0                                                  // GPIO0  pin 27 for Push Button 1
#define MOTOR_ENABLE_SWITCH 3                                                  // DIP Switch S1-1 pulls Digital pin D3 to ground when on, connected to pin 15 GPIO3 (J3)
#define SMART_LED           21                                                 // when DIP Switch S1-4 is on, Smart LED is connected to pin 23 GPIO21 (J21)
#define SMART_LED_COUNT     1 
#define TRIG_PIN            6
#define ECHO_PIN            7
#define PI                  3.1415926535897932384626433832795

// Constants
const int cDisplayUpdate = 100;                                                // update interval for Smart LED in milliseconds
const int cPWMRes = 4;                                                         // bit resolution for PWM
const int cMinPWM = 150;                                                       // PWM value for minimum speed that turns motor
const int cMaxPWM = pow(2, cPWMRes) - 1;                                       // PWM value for maximum speed
const int cCountsRev = 1096;                                                   // encoder pulses per motor revolution
const int cReturnTime = 100000;                                                // time for drive system, before returning to base
const long cTurnRadius = 6.2;                                                     // bot's turning radius
const int cRevDistance = 13.195;                                               // distance traversed by the bot for 1 wheel revolution

// adjustment variables and drive speed
const int cLeftAdjust = 3;                                                     // Amount to slow down left motor relative to right
const int cRightAdjust = 0;                                                    // Amount to slow down right motor relative to left

// Variables
boolean motorsEnabled = true;                                                  // motors enabled flag
boolean timeUpReturn = false;                                                  // drive timer elapsed flag
boolean turnDir = false;                                                       // index to indicate what the turn direction should be (left is false, right is true);
boolean wait = true;
unsigned char leftDriveSpeed;                                                  // motor drive speed (0-255)
unsigned char rightDriveSpeed;                                                 // motor drive speed (0-255)
unsigned int robotModeIndex = 0;                                               // state index for run mode
unsigned int driveIndex = 0;                                                   // state index for drive
unsigned int homeIndex = 0;
unsigned int  modePBDebounce;                                                  // pushbutton debounce timer count
unsigned long timerCountReturn = 0;                                            // return time counter
unsigned long displayTime;                                                     // heartbeat LED update timer
unsigned long previousMicros;                                                  // last microsecond count
unsigned long currentMicros;                                                   // current microsecond count
long xFromBase;
long yFromBase;
long totalYFromBase;
unsigned int turnNo = 0;
float us_Duration;
float cm_Distance;

// Declare SK6812 SMART LED object
//   Argument 1 = Number of LEDs (pixels) in use
//   Argument 2 = ESP32 pin number 
//   Argument 3 = Pixel type flags, add together as needed:
//     NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//     NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//     NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//     NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//     NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)
Adafruit_NeoPixel SmartLEDs(SMART_LED_COUNT, SMART_LED, NEO_RGB + NEO_KHZ800);

// smart LED brightness for heartbeat
unsigned char LEDBrightnessIndex = 0; 
unsigned char LEDBrightnessLevels[] = {5,15,30,45,60,75,90,105,120,135,150,165,180,195,210,225,240,255,
                                       240,225,210,195,180,165,150,135,120,105,90,75,60,45,30,15};
                             
unsigned int  modeIndicator[6] = {                                             // colours for different modes
   SmartLEDs.Color(255,0,0),                                                   // red - stop
   SmartLEDs.Color(0,255,0),                                                   // green - drive
   SmartLEDs.Color(0,0,255),                                                   // blue - pick up
   SmartLEDs.Color(255,255,0),                                                 // yellow - navigate home
   SmartLEDs.Color(0,255,255),                                                 // cyan - release
   SmartLEDs.Color(255,0,255)                                                  // magenta - empty case
};                                                                            

// Motor, encoder, and IR objects (classes defined in MSE2202_Lib)
Motion Bot = Motion();                                                         // Instance of Motion for motor control
Encoders LeftEncoder = Encoders();                                             // Instance of Encoders for left encoder data
Encoders RightEncoder = Encoders();                                            // Instance of Encoders for right encoder data
 
void setup() {
   // Set up motors and encoders
   Bot.driveBegin("D1", LEFT_MOTOR_A, LEFT_MOTOR_B, RIGHT_MOTOR_A, RIGHT_MOTOR_B);  // set up motors as Drive 1

   LeftEncoder.Begin(ENCODER_LEFT_A, ENCODER_LEFT_B, &Bot.iLeftMotorRunning );      // set up left encoder
   RightEncoder.Begin(ENCODER_RIGHT_A, ENCODER_RIGHT_B, &Bot.iRightMotorRunning );  // set up right encoder

   // Set up ultrasonic sensor
   pinMode(TRIG_PIN, OUTPUT);
   pinMode(ECHO_PIN, INPUT);

   // Set up SmartLED
   SmartLEDs.begin();                                                          // initialize smart LEDs object (REQUIRED)
   SmartLEDs.clear();                                                          // clear pixel
   SmartLEDs.setPixelColor(0,SmartLEDs.Color(0,0,0));                          // set pixel colors to 'off'
   SmartLEDs.show();                                                           // send the updated pixel colors to the hardware

   pinMode(MOTOR_ENABLE_SWITCH, INPUT_PULLUP);                                 // set up motor enable switch with internal pullup
   pinMode(MODE_BUTTON, INPUT_PULLUP);                                         // Set up mode pushbutton
   modePBDebounce = 0;                                                         // reset debounce timer count

}

void loop() {
  long pos[] = {0, 0};                                                        // current motor positions
   
  currentMicros = micros();                                                   // get current time in microseconds
  if((currentMicros - previousMicros) >= 1000){                               // enter if 1 millisecond has passed since last entry
    previousMicros = currentMicros;                                           // record current time in microseconds 

    // track 100 second (100000 milliseconds)
    timerCountReturn++;                                                       // increment the return timer counter
    if(timerCountReturn > cReturnTime){                                       // if the returm timer counter has counter the correct time
      timerCountReturn = 0;                                                   // reset the value of the timer counter
      timeUpReturn = true;                                                    // set the flag for the timer to true (time is up)
    }


    // Mode pushbutton debounce and toggle
    if (!digitalRead(MODE_BUTTON)) {                                            // if pushbutton GPIO goes LOW (nominal push)
    
      // Start debounce
      if (modePBDebounce <= 25) {                                               // 25 millisecond debounce time
        modePBDebounce = modePBDebounce + 1;                                    // increment debounce timer count
        if (modePBDebounce > 25) {                                              // if held for at least 25 mS
          modePBDebounce = 1000;                                                // change debounce timer count to 1 second
        }
      }

      if (modePBDebounce >= 1000) {                                             // maintain 1 second timer count until release
        modePBDebounce = 1000;
      }
    }

    else {                                                                      // pushbutton GPIO goes HIGH (nominal release)
      if(modePBDebounce <= 26) {                                                // if release occurs within debounce interval
        modePBDebounce = 0;                                                     // reset debounce timer count
      }
      else {
        modePBDebounce = modePBDebounce + 1;                                    // increment debounce timer count
        if(modePBDebounce >= 1025) {                                            // if pushbutton was released for 25 mS
          modePBDebounce = 0;                                                   // reset debounce timer count
          robotModeIndex++;                                                     // move robot to next mode
          robotModeIndex = robotModeIndex & 7;                                  // keep mode index between 0 and 7
          timeUpReturn = false;                                                 // reset return timer
        }
      }
    }

    // check if drive motors should be powered REMOVE?? NECESSARY???
    motorsEnabled = !digitalRead(MOTOR_ENABLE_SWITCH);                          // if SW1-1 is on (low signal), then motors are enabled\

    // drive and operation modes
    // 0 = Default after power up/reset. Robot is stopped.
    // 1 = Drive 
    // 2 = Pick up gems
    // 3 = Navigate to home base
    // 4 = Open back hatch
    switch(robotModeIndex){
      case 0: // robot stopped
        Bot.Stop("D1");                                                         // Stop the wheels
        LeftEncoder.clearEncoder();                                             // reset left encoder count
        RightEncoder.clearEncoder();                                            // reset right encoder count
        driveIndex = 1;                                                         // set the drive index to 1 (first driving state)
        break;

      case 1: // drive 
        if(timeUpReturn && driveIndex != 2 && driveIndex != 5){                                                      
          LeftEncoder.clearEncoder();
          RightEncoder.clearEncoder();
          Serial.println("timer is up, returning home");
          robotModeIndex = 3;
        }

        leftDriveSpeed = 255 - cLeftAdjust;
        rightDriveSpeed = 255 - cRightAdjust;

        if(motorsEnabled){

          switch(driveIndex){
            case 0: // temporary stop
              Bot.Stop("D1");
              LeftEncoder.clearEncoder();
              RightEncoder.clearEncoder();
              robotModeIndex = 2;
              break;
            
            case 1: // drive forward to the sweep area
              Bot.Forward("D1", leftDriveSpeed, rightDriveSpeed);

              RightEncoder.getEncoderRawCount();
              xFromBase = RightEncoder.lRawEncoderCount;
              if(RightEncoder.lRawEncoderCount >= cCountsRev * (375 / cRevDistance)){ 
                LeftEncoder.clearEncoder();
                RightEncoder.clearEncoder();
                Serial.println("moved x into sweep area");
                driveIndex = 2;
              }
              break;

            case 2: // turn to face sweep area
              Bot.Left("D1", leftDriveSpeed, rightDriveSpeed);

              RightEncoder.getEncoderRawCount();
              if(RightEncoder.lRawEncoderCount <= cCountsRev * -1 * (0.5 * PI * cTurnRadius / cRevDistance)){
                LeftEncoder.clearEncoder();
                RightEncoder.clearEncoder();
                Serial.println("turned into sweep area");
                driveIndex = 3;
              }
              break;

            case 3: // drive forward to the sweep area
              Bot.Forward("D1", leftDriveSpeed, rightDriveSpeed);

              RightEncoder.getEncoderRawCount();
              if(RightEncoder.lRawEncoderCount >= cCountsRev * (125 / cRevDistance)){ 
                LeftEncoder.clearEncoder();
                RightEncoder.clearEncoder();
                Serial.println("moved y into sweep area");
                driveIndex = 4;
              }
              break;

            case 4: // sweep forwards
              Bot.Forward("D1", leftDriveSpeed, rightDriveSpeed);
              RightEncoder.getEncoderRawCount();

              if(!turnDir){
                yFromBase = RightEncoder.lRawEncoderCount;
                totalYFromBase = RightEncoder.lRawEncoderCount;
              }
              
              if(turnDir){
                yFromBase = totalYFromBase - RightEncoder.lRawEncoderCount;
              }


              if(RightEncoder.lRawEncoderCount >= cCountsRev * (250 / cRevDistance)){
                LeftEncoder.clearEncoder();
                RightEncoder.clearEncoder();
                Serial.println("sweeped forward");
                driveIndex = 0;
              }
              break;

            case 5: // turn 90 degrees
              leftDriveSpeed = 200 - cLeftAdjust;
              rightDriveSpeed = 200 - cRightAdjust;
              if (turnDir){
                Bot.Right("D1", leftDriveSpeed, rightDriveSpeed);

                RightEncoder.getEncoderRawCount();
                if(RightEncoder.lRawEncoderCount >= cCountsRev * (0.5 * PI * cTurnRadius / cRevDistance)){
                  LeftEncoder.clearEncoder();
                  RightEncoder.clearEncoder();
                  
                  if(turnNo == 0) {
                    driveIndex = 6;
                    turnNo = 1;
                    Serial.println("right turn 1");
                  }
                  
                  else if(turnNo == 1) {
                    driveIndex = 4;
                    turnNo = 0;
                    turnDir = false;
                    Serial.println("right turn 2");
                  }
                }
                break;
              }

              if(!turnDir) {
                Bot.Left("D1", leftDriveSpeed, rightDriveSpeed);

                RightEncoder.getEncoderRawCount();
                if(RightEncoder.lRawEncoderCount <= cCountsRev * -1 * (0.5 * PI * cTurnRadius / cRevDistance)){
                  LeftEncoder.clearEncoder();
                  RightEncoder.clearEncoder();
                  if(turnNo == 0){
                    driveIndex = 6;
                    turnNo = 1;
                    Serial.println("left turn 1");
                  }

                  else if(turnNo == 1){
                    driveIndex = 4;
                    turnNo = 0;
                    turnDir = true;
                    Serial.println("left turn 2");
                  }
                }
                break;
              } 

            case 6: // move forward slightly (during turn)
              Bot.Forward("D1", leftDriveSpeed, rightDriveSpeed);

              RightEncoder.getEncoderRawCount();
              xFromBase -= RightEncoder.lRawEncoderCount;
              if(RightEncoder.lRawEncoderCount >= cCountsRev * (10 / cRevDistance)){
                LeftEncoder.clearEncoder();
                RightEncoder.clearEncoder();
                Serial.println("moved forward for turn");
                driveIndex = 5;
              }
              break;  
          }
        }
        break;

      case 2: // operate pick up
        // code for pick up
        Serial.println("pick up");
        driveIndex = 5;
        robotModeIndex = 1;
        break;

      case 3: // navigate to home base
        switch(homeIndex){

          case 0: // go y back to base
            if(turnDir){
              Bot.Forward("D1", leftDriveSpeed, rightDriveSpeed);
              RightEncoder.getEncoderRawCount();

              if(RightEncoder.lRawEncoderCount >= yFromBase){
                RightEncoder.clearEncoder();
                LeftEncoder.clearEncoder();
                Serial.println("moved forward in y to base");
                homeIndex = 1;
              }
            }

            if(!turnDir){
              Bot.Reverse("D1", leftDriveSpeed, rightDriveSpeed);
              RightEncoder.getEncoderRawCount();

              if(RightEncoder.lRawEncoderCount <= -1 * yFromBase){
                RightEncoder.clearEncoder();
                LeftEncoder.clearEncoder();
                Serial.println("moved backward in y to base");
                homeIndex = 1;
              }
            }
            break;

          case 1: //turn backward 90 degrees (direction depends on case 0)
            if(turnDir){
              Bot.Left("D1", leftDriveSpeed, rightDriveSpeed);
              RightEncoder.getEncoderRawCount();

              if(RightEncoder.lRawEncoderCount <= cCountsRev * -1 * (0.5 * PI * cTurnRadius / cRevDistance)){
                RightEncoder.clearEncoder();
                LeftEncoder.clearEncoder();
                Serial.println("turned left back to base");
                homeIndex = 2;
              }
            }

            if(!turnDir){
              Bot.Right("D1", leftDriveSpeed, rightDriveSpeed);
              RightEncoder.getEncoderRawCount();

              if(RightEncoder.lRawEncoderCount >= cCountsRev * (0.5 * PI * cTurnRadius / cRevDistance)){
                RightEncoder.clearEncoder();
                LeftEncoder.clearEncoder();
                Serial.println("turned right back to base");
                homeIndex = 2;
              }
            }
            break;

          case 2: // reverse back to base
            Bot.Reverse("D1", leftDriveSpeed, rightDriveSpeed);
            RightEncoder.getEncoderRawCount();
            
            if(RightEncoder.lRawEncoderCount <= -1 * xFromBase + (cCountsRev * 10 / cRevDistance)){
              Bot.Stop("D1");
              Serial.println("reverse in x back to base");
              homeIndex = 3;
            }
            break; 

          case 3: // reverse precisely onto container  
            Bot.Reverse("D1", leftDriveSpeed, rightDriveSpeed);
            Serial.println("precise reversing started");
            
            if(wait){
              digitalWrite(TRIG_PIN, HIGH);
              Serial.println("send out pulse");
              wait = false;
              break;
            }

            if(!wait){
              digitalWrite(TRIG_PIN, LOW);
              wait = true;
              us_Duration = pulseIn(ECHO_PIN, HIGH);
              cm_Distance = 0.017 * us_Duration;
              Serial.printf("read pulse, %f away from container\n", cm_Distance);
              
              if(cm_Distance <= 5){
                Bot.Stop("D1");
                Serial.println("At container");
                robotModeIndex = 4;
              }
              break;
            }
        } 
        break;

      case 4: // open back hatch
        //code for hatch
        Serial.println("Dumping contents");
        robotModeIndex = 0;
        break;
    }

    // Update brightness of heartbeat display on SmartLED
    displayTime++;                                                            // count milliseconds
    if (displayTime > cDisplayUpdate) {                                       // when display update period has passed
      displayTime = 0;                                                        // reset display counter
      LEDBrightnessIndex++;                                                   // shift to next brightness level
      if (LEDBrightnessIndex > sizeof(LEDBrightnessLevels)) {                 // if all defined levels have been used
        LEDBrightnessIndex = 0;                                               // reset to starting brightness
      }
      SmartLEDs.setBrightness(LEDBrightnessLevels[LEDBrightnessIndex]);       // set brightness of heartbeat LED
      Indicator();                                                            // update LED
    }
  }
}

// sorting system operation
void Sorting() {
  // code to operate sorting and sorting servos
}

// Set colour of Smart LED depending on robot mode (and update brightness)
void Indicator() {
  SmartLEDs.setPixelColor(0, modeIndicator[robotModeIndex]);                  // set pixel colors to = mode 
  SmartLEDs.show();                                                           // send the updated pixel colors to the hardware
}