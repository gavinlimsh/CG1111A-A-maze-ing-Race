#include <MeMCore.h>

int status = 0;  // global status; 0 = do nothing, 1 = mBot runs

//DEBUGMODE
bool SERIAL_MONITOR_ON = true;  //Enable Serial Monitor
bool DEBUGMODE = false;         //Enable DEBUG Mode

//--COLOUR CALIBRATION AND READING--

//INITIALISATION

//LED controls
#define RGBWait 50  //Define time delay before changing to another LED colour
#define LDRWait 10  //Define time delay before taking another LDR reading in milliseconds

#define DECODER_SELECT_1 11  //D11 = DECODER PIN 2
#define DECODER_SELECT_2 12  //D12 = DECODER PIN 3
MeRGBLed RGBled(0, 30);
MePort ldr_adaptor(1);

#define LDR_DETECTOR 0  //A0
#define LDR 0           //LDR sensor pin at A0

//LED readings
#define RED_RGB 0
#define BLUE_RGB 1
#define GREEN_RGB 2

typedef enum {
  WHITE_PAPER = 1,
  BLACK_PAPER = 2,
  RED_PAPER = 3,
  GREEN_PAPER = 4,
  BLUE_PAPER = 5,
  PINK_PAPER = 6,
  ORANGE_PAPER = 7,
} PaperColours;

//floats to hold colour arrays.
//white, black and grey arrays are calibrated at the start of the day to account for discrepancies in sensor readings
float colourArray[] = { 0, 0, 0 };
float whiteArray[] = { 959.00, 983.00, 947.00 };
float blackArray[] = { 683.00, 821.00, 728.00 };
float greyDiff[] = { 390.00, 179.00, 218.00 };

//FUNCTIONS

//helper function to get average LDR readings
int getAvgReading(int times) {
  //find the average reading for the requested number of times of scanning LDR
  int reading;
  int total = 0;
  //take the reading as many times as requested and add them up
  for (int i = 0; i < times; i++) {
    reading = analogRead(LDR);
    total = reading + total;
    delay(LDRWait);
  }
  //calculate the average and return it
  return total / times;
}

//helper functions to control the LED lights:

//code for turning on the red LED only
void shineRed() {
  digitalWrite(DECODER_SELECT_1, HIGH);
  digitalWrite(DECODER_SELECT_2, LOW);
}

//code for turning on the green LED only
void shineGreen() {
  digitalWrite(DECODER_SELECT_1, HIGH);
  digitalWrite(DECODER_SELECT_2, HIGH);
}

//code for turning on the blue LED only
void shineBlue() {
  digitalWrite(DECODER_SELECT_1, LOW);
  digitalWrite(DECODER_SELECT_2, HIGH);
}

//code for turning off all the LEDs
void offLED() {
  digitalWrite(DECODER_SELECT_1, LOW);
  digitalWrite(DECODER_SELECT_2, LOW);
}

//function for colour calibration
void setBalance() {
  //set white balance
  Serial.println("Put White Sample For Calibration ...");
  delay(5000);  //delay for five seconds for getting sample ready

  //scan the white sample.
  //go through one colour at a time, set the maximum reading for each colour -- red, green and blue to the white array
  for (int i = 0; i <= 2; i++) {
    if (i == 0) shineRed();
    else if (i == 1) shineGreen();
    else shineBlue();
    delay(RGBWait);
    whiteArray[i] = getAvgReading(30);  //scan 30 times and return the average,
    offLED();
    delay(RGBWait);
  }

  //done scanning white, time for the black sample.
  //set black balance
  Serial.println("Put Black Sample For Calibration ...");
  delay(5000);  //delay for five seconds for getting sample ready
  //scan the black sample
  //go through one colour at a time, set the minimum reading for red, green and blue to the black array
  for (int i = 0; i <= 2; i++) {
    if (i == 0) shineRed();
    else if (i == 1) shineGreen();
    else shineBlue();
    delay(RGBWait);
    blackArray[i] = getAvgReading(30);
    offLED();
    delay(RGBWait);

    //the difference between the maximum and the minimum gives the range
    greyDiff[i] = whiteArray[i] - blackArray[i];
  }

  if (DEBUGMODE) {
    Serial.print("White Array: ");
    for (int i = 0; i < 3; i++) {
      Serial.print(whiteArray[i]);
      Serial.print(" ");
    }
    Serial.println();

    Serial.print("Black array: ");
    for (int i = 0; i < 3; i++) {
      Serial.print(blackArray[i]);
      Serial.print(" ");
    }
    Serial.println();

    Serial.print("Grey Array: ");
    for (int i = 0; i < 3; i++) {
      Serial.print(greyDiff[i]);
      Serial.print(" ");
    }
    Serial.println();
  }

  //delay another 5 seconds for getting ready colour objects
  Serial.println("Colour Sensor Is Ready.");
  delay(5000);
}

PaperColours detectColor() {
  //filling in the array of colour readings in R, G, B order {R,G,B}
  for (int c = 0; c <= 2; c++) {
    //turn on only one LED per loop
    if (c == 0) shineRed();
    else if (c == 1) shineGreen();
    else shineBlue();
    delay(RGBWait);
    //get the average of 5 consecutive LDR readings for the current colour and return an average
    colourArray[c] = getAvgReading(5);
    //normalize the obtained values to give values:
    //((average reading returned - lowest value) / maximum possible range)*255 to give a value between 0-255, representing the value for the current reflectivity (i.e. the colour LDR is exposed to)
    colourArray[c] = (colourArray[c] - blackArray[c]) / (greyDiff[c]) * 255;
    offLED();
    delay(RGBWait);

    //show the value for the current colour LED, which corresponds to either the R, G or B of the RGB code
    if (DEBUGMODE) {
      Serial.println(int(colourArray[c]));
    }
  }
  return getColour();
}

//function to determine colour underneath using data obtained during colour calibration
//values for conditions obtained during colour calibration previously
PaperColours getColour() {
  int threshold = 5;
  if (colourArray[RED_RGB] > 160) {
    if (colourArray[BLUE_RGB] > (230 - threshold)) {
      return WHITE_PAPER;
    } else if (colourArray[BLUE_RGB] > (195 - threshold)) {
      return PINK_PAPER;
    } else if (colourArray[BLUE_RGB] > (180 - threshold)) {
      return ORANGE_PAPER;
    } else {
      return RED_PAPER;
    }
  } else {
    if (colourArray[GREEN_RGB] > (240 - threshold)) {
      return BLUE_PAPER;
    } else {
      return GREEN_PAPER;
    }
  }
}

//--PROXIMITY SENSORS (ULTRASOUND AND IR)--

//INITIALISATION (ULTRASOUND)
#define TIMEOUT 1500        // Max microseconds to wait; choose according to max dist of wall
#define SPEED_OF_SOUND 340  //Update according to experiment
#define ULTRASONIC 10       //D10 = Ultrasonic
#define ULTRASONIC_DELAY 500

//FUNCTIONS (ULTRASOUND)

//function to get values for distance from right wall
double getUltrasonic() {
  //configure ultrasonic pin
  pinMode(ULTRASONIC, OUTPUT);

  //send ping
  digitalWrite(ULTRASONIC, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC, HIGH);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC, LOW);

  //configure ultrasonic pin
  pinMode(ULTRASONIC, INPUT);

  //record time taken for ping to return
  long duration = pulseIn(ULTRASONIC, HIGH, TIMEOUT);
  double distance_from_left = 0;
  //since the sound echos, time taken to travel to left wall is half
  //calculate the distance from the left wall
  if (duration > 0) {
    distance_from_left = duration / 2.0 / 1000000 * SPEED_OF_SOUND * 100;
  } else {
    //distance too far - error 
    return -1;
  }

  //return calculated distance
  return distance_from_left;
}

//INITIALISATION (IR DETECTOR)
#define IR_DETECTOR 1  //A1
#define IR_WAIT 1

MePort ir_adaptor(0);

//FUNCTIONS (IR DETECTOR)

//helper function to turn on the IR emitter only
void shineIR() {
  digitalWrite(DECODER_SELECT_1, LOW);
  digitalWrite(DECODER_SELECT_2, LOW);
}

//helper function to turn off the IR emitter
void offIR() {
  digitalWrite(DECODER_SELECT_1, HIGH);
  digitalWrite(DECODER_SELECT_2, HIGH);
}

//function to get values for distance from left wall
double getIR() {
  offIR();  //to remove the emitted IR and only leave IR from the surroundings
  delay(IR_WAIT);

  double ambientIR = analogRead(IR_DETECTOR);  //reading only IR from the surroundings

  shineIR();
  delay(IR_WAIT);
  double reading = analogRead(IR_DETECTOR);  //reading IR from surroundings AND IR emitter
  double IR_val = ambientIR - reading; //calculate the reflected IR value by removing ambient light interference

  if (DEBUGMODE) {
    Serial.print("IR_val (0 - 1023): ");
    Serial.println(IR_val);
  }

  //return IR distance
  return IR_val;
}

//--MOVEMENT--

//INITIALISATION

MeDCMotor leftMotor(M1);   // assigning Left Motor to port M1
MeDCMotor rightMotor(M2);  // assigning Right Motor to port M2

//direction for turn function
#define TURN_LEFT 0
#define TURN_RIGHT 1

#define turning_time_ms 359.5    //the time duration (ms) for turning
uint8_t motorSpeed = 255;        //setting motor speed to an integer between 1 and 255
uint8_t nudgeSpeed = 182;        //nudgeSpeed<motorSpeed to make mbot swerve when too close to either wall
#define double_time_forward 777  //time taken to move one grid forward for compound functions

//FUNCTIONS

//function to stop both motors (i.e. make the mbot stop moving)
void stopMotor() {
  leftMotor.stop();
  rightMotor.stop();
}

//function to move forward
//since left and right motor are turning in opposite directions, sign for the motors are different
void moveForward() {
  leftMotor.run(-motorSpeed);
  rightMotor.run(motorSpeed);
}

//helper function to turn
void turn(int dir, double deg) {
  //0 for left, 1 for right
  if (dir == 0) {
    leftMotor.run(motorSpeed);            //+ve: wheel turns clockwise
    rightMotor.run(motorSpeed);           //+ve: wheel turns clockwise
    delay((turning_time_ms / 90) * deg);  //keep turning left for this time duration, adjust to set angle
    stopMotor();
  }
  if (dir == 1) {
    leftMotor.run(-motorSpeed);           //-ve: wheel turns anti-clockwise
    rightMotor.run(-motorSpeed);          //-ve: wheel turns anti-clockwise
    delay((turning_time_ms / 90) * deg);  //keep turning left for this time duration, adjust to set angle
    stopMotor();
  }
}

//function to turn left 90 deg within the same grid
void turnLeft() {
  turn(TURN_LEFT, 80);  //calibration for overturning
}

//function to turn right 90 deg within the same grid
void turnRight() {
  turn(TURN_RIGHT, 80);  //calibration for overturning
}

//function to turn 180 deg within the same grid
void uTurn() {
  turn(TURN_LEFT, 155);  //calibration for overturning
}

//function to swerve left
void nudgeLeft() {
  leftMotor.run(-nudgeSpeed);
  rightMotor.run(motorSpeed);  //left wheel moves slower than right wheel--will swerve left
}

//function to swerve right
void nudgeRight() {
  leftMotor.run(-motorSpeed);
  rightMotor.run(nudgeSpeed);  //right wheel moves slower than left wheel--will swerve right
}

//function to do double left turn
void doubleLeftTurn() {
  turnLeft();
  moveForward();  //
  delay(double_time_forward);
  turnLeft();
}

//function to do double right turn
void doubleRightTurn() {
  turnRight();
  moveForward();
  delay(double_time_forward);
  turnRight();
}

//INITIALISATION FOR MOVING STRAIGHT:
//is set at the beginning in setup
double threshold_left = 0;
double threshold_right = 0;

//function to move straight:
//code to move forward and nudge when too close to either wall
void moveStraight() {
  double distance_from_left = getUltrasonic();  //closer corresponds to smaller distance_from_left
  double distance_from_right = getIR();         //closer corresponds to larger distance_from_right

  //condition to account for no walls on either side:
  //if either wall too far, do not make false positive nudges and move forward
  if (distance_from_left == -1 || distance_from_right > threshold_right * 1.25) {
    moveForward();
  }

  //if too close to left wall, nudge right
  else if (distance_from_left > 0 && distance_from_left < threshold_left) {
    nudgeRight();
    delay(10);
  }

  //if too close to right wall, nudge left
  else if (distance_from_right > threshold_right) {
    nudgeLeft();
    delay(10);
  }

  //if in centre, do not nudge and move forward
  else {
    moveForward();
  }

  //print distances from either wall to check readings and variables in the condiitonals
  if (DEBUGMODE) {
    Serial.print("Right: ");
    Serial.println(distance_from_right);
    Serial.print("Left (cm): ");
    Serial.println(distance_from_left);
  }
}

//--CELEBRATION--

//INITIALISATION
MeBuzzer buzzer;  //create buzzer object

//FUNCTION

//function to play celebratory tune
void celebrate() {
  // Justin Bieber - "Baby" (short buzzer version)
  int tempo = 250;
  int gap = 20;

  // ---- Part 1: "Baby, baby, baby oh~" ----
  buzzer.tone(392, tempo);
  delay(tempo + gap);  // G4 "Ba"
  buzzer.tone(392, tempo);
  delay(tempo + gap);  // G4 "by"
  buzzer.tone(440, tempo);
  delay(tempo + gap);  // A4 "ba"
  buzzer.tone(392, tempo);
  delay(tempo + gap);  // G4 "by"
  buzzer.tone(349, tempo * 1.5);
  delay(tempo * 1.5 + gap);  // F4 "oh~"
  buzzer.tone(330, tempo);
  delay(tempo + gap);  // E4
  buzzer.tone(294, tempo);
  delay(tempo + gap);  // D4
  buzzer.tone(392, tempo * 1.5);
  delay(tempo * 1.5 + gap);  // G4 ending

  // ---- Part 2: "Baby, baby, baby no~" ----
  buzzer.tone(392, tempo);
  delay(tempo + gap);  // G4 "Ba"
  buzzer.tone(392, tempo);
  delay(tempo + gap);  // G4 "by"
  buzzer.tone(440, tempo);
  delay(tempo + gap);  // A4 "ba"
  buzzer.tone(392, tempo);
  delay(tempo + gap);  // G4 "by"
  buzzer.tone(349, tempo * 1.5);
  delay(tempo * 1.5 + gap);  // F4 "no~"
  buzzer.tone(330, tempo);
  delay(tempo + gap);  // E4
  buzzer.tone(294, tempo);
  delay(tempo + gap);  // D4
  buzzer.tone(392, tempo * 1.5);
  delay(tempo * 1.5 + gap);  // G4 ending

  buzzer.noTone();
}

//--CHALLENGE MODE--

//INITIALISATION

//line detector (entering of challenge mode)
MeLineFollower lineFinder(PORT_3);  // assigning lineFinder to RJ25 port 2
int sensorState;

//FUNCTION

//function to decide what action to do
void action(PaperColours c) {
  if (c == RED_PAPER) {
    turnLeft();
  } else if (c == BLUE_PAPER) {
    doubleRightTurn();
  } else if (c == GREEN_PAPER) {
    turnRight();
  } else if (c == PINK_PAPER) {
    doubleLeftTurn();
  } else if (c == ORANGE_PAPER) {
    uTurn();
  } else if (c == WHITE_PAPER) {
    celebrate();
    status = 1;  //end of maze--when loop runs again, mbot does nothing as status redfined as 1
  } else {
    return;
  }
}

//--RUNNING THE MBOT--

void setup() {  //Begin serial communication
  if (SERIAL_MONITOR_ON) {
    Serial.begin(9600);
  }
  //Configure LDR pins
  pinMode(LDR_DETECTOR, INPUT);

  //Configure IR pins
  pinMode(IR_DETECTOR, INPUT);

  //Configure decoder pins
  pinMode(DECODER_SELECT_1, OUTPUT);
  pinMode(DECODER_SELECT_2, OUTPUT);

  //delay to give time to react/move hands away before mbot starts moving
  delay(100);
  double offset = 2;
  //setting threshold values that represent being centre of the path
  //offset to allow mbot some leeway from the centre
  threshold_right = getIR() - offset;
  threshold_left = getUltrasonic() + offset;
}

void loop() {
  sensorState = lineFinder.readSensors();
  if (status == 0) {                     //if mbot did not reach the end of the maze yet, continue moving straight and checking for challenges
    if (sensorState != S1_OUT_S2_OUT) {  //so long as one sensor is on the black strip, enter CHALLENGE MODE
      stopMotor();
      action(detectColor());  //mbot goes into CHALLENGE MODE
    } else {
      moveStraight();  //continue moving striaght if no coloured paper underneath
    }
  }
  //if the mbot was already at the end of the maze (white paper underneath), it stops moving and stops checking for challenges
  else { stopMotor(); }
}
