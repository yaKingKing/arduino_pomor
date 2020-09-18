#define DEBUG
//#define DEBUG_SHORT_POMOR_TIMES
#include "notes.h"
#include <avr/sleep.h>
#include <RTClib.h>
#include <ezButton.h>
#include "rtc_functionallities.h"


// RTC Variables
RTC_DS3231 rtc;
#define CLOCK_INTERRUPT_PIN 3
boolean timerTimedOut = false;

// button variables
const int buttonPin = 2;
ezButton button(buttonPin); 
const int SHORT_PRESS_TIME = 1000; // 1000 milliseconds
const int LONG_PRESS_TIME  = 1000; // 1000 milliseconds
unsigned long pressedTime  = 0;
unsigned long releasedTime = 0;

boolean buttonClicked = false; // clicked: ISR was fired
boolean buttonPressed = false; // pressed: Button was hold down for a short time
boolean buttonHold = false;    // hold:    Button was hold for a long time

// timer variables
boolean endSleep = false;

// pomor variables
#ifdef DEBUG_SHORT_POMOR_TIMES
const unsigned long task_times[] = {25L*1000L,5L*1000L,25L*1000L,5L*1000L,25L*1000L,5L*1000L,25L*1000L,20L*1000L};
#else
const unsigned long task_times[] = {25L*60L*1000L,5L*60L*1000L,25L*60L*1000L,5L*60L*1000L,25L*60L*1000L,5L*60L*1000L,25L*60L*1000L,20L*60L*1000L};
#endif
int current_task = 0;
DateTime taskStartTime; // time the current task was started 
const int showCurrentTaskProgressLedTime = 1000;   // ms led show: how long do the leds shine when showing the current progress of a task
boolean currentlyShowingTaskProgress = false;
boolean currentlyShowingCurrentTask = false;


// Led variables
const int led_array_pins[] = {14,15,16,17};
const int redLedPin = 5;
const int greenLedPin = 4;
unsigned long turnOfLedsAtTime = 0;



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
  initLeds();
  initBuzzer();
  initRTC();
  initButton();
  setTimerForCurrentTask(); // directly start pomodoro task
  iterateAllLeds(); // show some startup animation
}


void loop() {

  if(buttonPressed){
    buttonPressed = false;
    if(notify){
      debug("-> stop Notifying, start next task");
      stopNotifying();
      startNextTask();
      showCurrentTask();
    } else if (!currentlyShowingTaskProgress && !currentlyShowingCurrentTask ){
      debug("-> show current task progress");
      showCurrentTaskProgress();
    }else  if ( currentlyShowingTaskProgress || currentlyShowingCurrentTask ){
      debug("-> skip current task");
      startNextTask();
      showCurrentTask();
    }
  }

  if(buttonHold){
    buttonHold = false;
    debug("-> start pause");
    showPause();
    pauseUntilInterrupt();
    turnOfLeds();
  }


  // update shit
  ledUpdate();
  notifyUpdate();
  rtcUpdate();
  
  
  if (!notify && !currentlyShowingTaskProgress && !currentlyShowingCurrentTask){
    debug("/- sleep");
    sleep();
    #ifdef DEBUG
    if(buttonPressed) Serial.println("\\- woke up (button press)");
    if(buttonHold) Serial.println("\\- woke up (button hold)");
    if(timerTimedOut) Serial.println("\\- woke up (timer)");
    if(!buttonPressed && !buttonHold && !timerTimedOut) Serial.println("\\- woke up (unknown)");
    #endif
  }
  
}

void rtcUpdate(){
  if (rtc.alarmFired(1)){
    debug("-> notify end of task");
    rtc.clearAlarm(1);
    startNotify();
  }
}




void debug(char* message){
  #ifdef DEBUG
  Serial.println(message);
  #endif
}


void buttonUpdate() {
  button.loop(); // MUST call the loop() function first

  if(button.isPressed())
    pressedTime = millis();

  if(button.isReleased()) {
    releasedTime = millis();

    long pressDuration = releasedTime - pressedTime;

    if( pressDuration < SHORT_PRESS_TIME )
      Serial.println("A short press is detected");
      endSleep = true;
      buttonPressed = true;

    if( pressDuration > LONG_PRESS_TIME )
      Serial.println("A long press is detected");
      endSleep = true;
      buttonHold = true;
  }
}







// ----------------------------- //
//                               //
//    arduino sleep functions    //
//                               //
// ----------------------------- //


void pauseUntilInterrupt(){
  // we do not want to weak up when the task is done
  rtc.clearAlarm(1);
  // time that we already did the task
  TimeSpan processedTime =  rtc.now() - taskStartTime;
  TimeSpan taskTime = TimeSpan((task_times[current_task]/1000L));
  sleep();
  setTimerIn( taskTime - processedTime );
}


void sleep(){
  #ifdef DEBUG
  // delay for last serial println to finish
  delay(20);
  #endif
  // sleep until some functionallity ends the sleep
  endSleep = false;
  while (!endSleep){
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    sleep_mode();
    sleep_disable();
    delay(100);
    #ifdef DEBUG
    Serial.println("|- interrupt");
    #endif
    buttonUpdate();
  }
}





// --------------------------------- //
//                                   //
//    arduino INTERRUPT functions    //
//                                   //
// --------------------------------- //


void buttonPressedISR(){
  #ifdef DEBUG
  Serial.println("_ISR_: Button clicked");
  #endif
 buttonClicked = true;
}

void timerTimeoutISR(){
  //detachInterrupt(digitalPinToInterrupt(buttonPin));
  #ifdef DEBUG
  Serial.println("_ISR_: timed out");
  #endif
  timerTimedOut = true;
  endSleep = true;
}







// show task
void showCurrentTaskProgress(){
  TimeSpan passedTime = rtc.now() - taskStartTime;
  int n_leds = min( ( (passedTime.totalseconds()*5)  /   (task_times[current_task]/1000) )  ,4);
  #ifdef DEBUG
  Serial.println("   passedTime: "+String(passedTime.minutes())+" min, "+String(passedTime.seconds())+" sec ("+String(n_leds)+" leds)");
  #endif
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
  turnOfLedsAtTime = millis() + showCurrentTaskProgressLedTime;
  currentlyShowingTaskProgress = true;
  currentlyShowingCurrentTask = false;
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
  currentlyShowingTaskProgress = false;
  currentlyShowingCurrentTask = true;
}
void showPause(){
  currentlyShowingTaskProgress = false;
  currentlyShowingCurrentTask = false;
  turnOfLeds();
  digitalWrite(greenLedPin, LOW);
  digitalWrite(redLedPin, LOW);
  digitalWrite(led_array_pins[current_task/2], LOW);
}



// skip task
void startNextTask(){
  current_task = (current_task+1) % 8;
  setTimerForCurrentTask();
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




// update functions
void notifyUpdate(){
  if (notify){
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
       pauseUntilInterrupt();
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
}



// --------------------------------- //
//                                   //
//   arduino led/output functions    //
//                                   //
// --------------------------------- //

void initLeds(){
  pinMode(buttonPin, INPUT);
  pinMode(redLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);
  for (int i = 0; i<4; i++){
    pinMode(led_array_pins[i], OUTPUT);
    digitalWrite(led_array_pins[i], HIGH);
  }
  digitalWrite(redLedPin, HIGH);
  digitalWrite(greenLedPin, HIGH);
}

void initBuzzer(){
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);  
}

void initButton(){
  attachInterrupt(digitalPinToInterrupt(buttonPin),buttonPressedISR,FALLING);
}

void ledUpdate(){
  

  
  if (currentlyShowingTaskProgress || currentlyShowingCurrentTask){
    if (turnOfLedsAtTime <= millis()){
        currentlyShowingTaskProgress = false;
        currentlyShowingCurrentTask = false;
        turnOfLeds();
      }
  }
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




void turnOfLedsUpdate(){
  // if other functionallity wants to turn off leds after a given time
  if (turnOfLedsAtTime != 0){
    if (millis() > turnOfLedsAtTime){
      currentlyShowingTaskProgress = false;
      currentlyShowingCurrentTask = false;
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
