//#define DEBUG
#include "notes.h"


// Button variables
const int buttonPin = 2;
const int debounce = 20;          // ms debounce period to prevent flickering when pressing or releasing the button
const int DCgap = 500;            // max ms between clicks for a double click event
const int holdTime = 1000;        // ms hold period: how long to wait for press+hold event
const int longHoldTime = 3000;    // ms long hold period: how long to wait for press+hold event
boolean buttonVal = HIGH;   // value read from button
boolean buttonLast = HIGH;  // buffered value of the button's previous state
boolean DCwaiting = false;  // whether we're waiting for a double click (down)
boolean DConUp = false;     // whether to register a double click on next release, or whether to wait and click
boolean singleOK = true;    // whether it's OK to do a single click
long downTime = -1;         // time the button was pressed down
long upTime = -1;           // time the button was released
boolean ignoreUp = false;   // whether to ignore the button release because the click+hold was triggered
boolean waitForUp = false;        // when held, whether to wait for the up event
boolean holdEventPast = false;    // whether or not the hold event happened already
boolean longHoldEventPast = false;// whether or not the long hold event happened already

// Led variables
const int led_array_pins[] = {14,15,16,17};
const int redLedPin = 18;
const int greenLedPin = 4;
unsigned long turnOfLedsAtTime = 0;


// pomor variables
//const unsigned long task_times[] = {20L*1000L,5L*1000L,20L*1000L,5L*1000L,20L*1000L,5L*1000L,20L*1000L,20L*1000L};
const unsigned long task_times[] = {25L*60L*1000L,5L*60L*1000L,25L*60L*1000L,5L*60L*1000L,25L*60L*1000L,5L*60L*1000L,25L*60L*1000L,20L*60L*1000L};
int current_task = 0;
unsigned long task_start_time = millis();

// buzzer variables
const int buzzerPin = 8;
bool notify = false;
const int pulseTimes[] = {100,100,500,500};
unsigned long pulseStart = 0;
int pulseI = 0;
int pulseLoops = 0;
bool pause = false;
unsigned long pauseAt = 0;


void setup() {
  #ifdef DEBUG
  Serial.begin(115200);
  Serial.println("start");
  #endif
  pinMode(buttonPin, INPUT);
  pinMode(redLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  for (int i = 0; i<4; i++){
    pinMode(led_array_pins[i], OUTPUT);
    digitalWrite(led_array_pins[i], HIGH);
  }
  digitalWrite(buzzerPin, LOW);
  digitalWrite(redLedPin, HIGH);
  digitalWrite(greenLedPin, HIGH);
  iterateAllLeds(); // show some startup animation
}

void iterateAllLeds(){
  for (int i = 0; i<4; i++){
    digitalWrite(led_array_pins[i], LOW);
    delay(200);
    digitalWrite(led_array_pins[i], HIGH);
  }
  digitalWrite(redLedPin, LOW);
  delay(200);
  digitalWrite(redLedPin, HIGH);
  digitalWrite(greenLedPin, LOW);
  delay(200);
  digitalWrite(greenLedPin, HIGH);
}

void loop() {
  int event = checkButton();
  
  #ifdef DEBUG
  if (event == 1) Serial.println("click");
  if (event == 2) Serial.println("double click");
  if (event == 3) Serial.println("hold");
  if (event == 4) Serial.println("long hold");
  #endif
  
  if (event == 1){
    if(pause){
      endPause();
      showCurrentTaskProgress();
    }else if(notify){
      stopNotifying();
      startNextTask();
    } else {
      showCurrentTaskProgress();
    }
  }
  if (event == 2 && !pause && !notify){
    startNextTask();
  }
  if (event == 3 && !pause && !notify){
    startPause();
  }
  
  turnOfLedsUpdate();
  bool taskTimeOver = (millis()-task_start_time) >= task_times[current_task];
  if (taskTimeOver && !notify && !pause){
    startNotify();
  }
  if (notify && !pause){
    notify_update();
  }
}


// show task
void showCurrentTaskProgress(){
  int n_leds = min(((millis()-task_start_time)*5)/task_times[current_task],4);
  turnOfLeds();
  for (int i = 0; i<n_leds; i++){
    digitalWrite(led_array_pins[i], LOW);
  }
  if (current_task%2 == 0){
   // work
    digitalWrite(redLedPin, LOW);
  }else{
    // pause
    digitalWrite(greenLedPin, LOW);
  }
  turnOfLedsAtTime = millis() + 1000;
}
void showCurrentTask(){
  turnOfLeds();
  digitalWrite(led_array_pins[current_task/2], LOW);
  if (current_task%2 == 0){
   // work
    digitalWrite(redLedPin, LOW);
  }else{
    // pause
    digitalWrite(greenLedPin, LOW);
  }
  turnOfLedsAtTime = millis() + 3000;
  task_start_time = millis();
}

// skip task
void startNextTask(){
  current_task = (current_task+1) % 8;
  showCurrentTask();
}

// notify
void startNotify(){
  notify = true;
  pulseStart = millis();
}
void stopNotifying(){
  notify = false;
  turnOfLeds();
  digitalWrite(buzzerPin, LOW);
  pulseI = 0;
  pulseLoops = 0;
}
// pause
void startPause(){
  turnOfLeds();
  pause = true;
  pauseAt = millis();
  digitalWrite(greenLedPin, LOW);
  digitalWrite(redLedPin, LOW);
  digitalWrite(led_array_pins[current_task/2], LOW);
}
void endPause(){
  pause = false;
  task_start_time += (millis()-pauseAt);
}




// update functions
void notify_update(){
  // blink all leds
  while ((millis()-pulseStart) > pulseTimes[pulseI]){
    pulseStart += pulseTimes[pulseI];
    pulseI += 1;
    if (pulseI == 4){
      pulseI = 0;
      pulseLoops += 1;
    }
    if (pulseLoops == 2){
      tone(buzzerPin, NOTE_D1, 100);
    }
    if (pulseLoops >= 10 && pulseLoops <= 14 && pulseLoops%2 == 0){
      tone(buzzerPin, NOTE_A2, 100);
    }
  }
  if (pulseLoops > 35){
     stopNotifying();
     startNextTask();
     startPause();
     return;
  }
  bool pulseAmplitude = pulseI%2;
  if (pulseAmplitude){
    turnOnLeds();
    if (pulseLoops >= 23){
        digitalWrite(8, HIGH);
      }
  }else{
    turnOfLeds();
    if (pulseLoops >= 23){
        digitalWrite(8, LOW);
    }
  }
}

void turnOfLedsUpdate(){
  // if other functionallity wants to turn off leds after a given time
  if (turnOfLedsAtTime != 0){
    if (millis() > turnOfLedsAtTime){
      turnOfLeds();
      turnOfLedsAtTime = 0;
    }
  }
}


// other functionallities
void turnOfLeds(){
  writeAllLeds(HIGH);
}
void turnOnLeds(){
  writeAllLeds(LOW);
}


void writeAllLeds(bool state){
  for (int i = 0; i<4; i++){
    digitalWrite(led_array_pins[i], state);
  }
  digitalWrite(redLedPin, state);
  digitalWrite(greenLedPin, state);
}

int checkButton() {    
   int event = 0;
   buttonVal = digitalRead(buttonPin);
   // Button pressed down
   if (buttonVal == LOW && buttonLast == HIGH && (millis() - upTime) > debounce)
   {
       downTime = millis();
       ignoreUp = false;
       waitForUp = false;
       singleOK = true;
       holdEventPast = false;
       longHoldEventPast = false;
       if ((millis()-upTime) < DCgap && DConUp == false && DCwaiting == true)  DConUp = true;
       else  DConUp = false;
       DCwaiting = false;
   }
   // Button released
   else if (buttonVal == HIGH && buttonLast == LOW && (millis() - downTime) > debounce)
   {        
       if (not ignoreUp)
       {
           upTime = millis();
           if (DConUp == false) DCwaiting = true;
           else
           {
               event = 2;
               DConUp = false;
               DCwaiting = false;
               singleOK = false;
           }
       }
   }
   // Test for normal click event: DCgap expired
   if ( buttonVal == HIGH && (millis()-upTime) >= DCgap && DCwaiting == true && DConUp == false && singleOK == true && event != 2)
   {
       event = 1;
       DCwaiting = false;
   }
   // Test for hold
   if (buttonVal == LOW && (millis() - downTime) >= holdTime) {
       // Trigger "normal" hold
       if (not holdEventPast)
       {
           event = 3;
           waitForUp = true;
           ignoreUp = true;
           DConUp = false;
           DCwaiting = false;
           //downTime = millis();
           holdEventPast = true;
       }
       // Trigger "long" hold
       if ((millis() - downTime) >= longHoldTime)
       {
           if (not longHoldEventPast)
           {
               event = 4;
               longHoldEventPast = true;
           }
       }
   }
   buttonLast = buttonVal;
   return event;
}
