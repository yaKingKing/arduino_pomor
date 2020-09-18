#define DEBUG
//#define DEBUG_SHORT_POMOR_TIMES
#include "notes.h"
#include <avr/sleep.h>
#include <RTClib.h>


// RTC Variables
RTC_DS3231 rtc;
#define CLOCK_INTERRUPT_PIN 3
boolean timerTimedOut = false;

// button variables
const int buttonPin = 2;
unsigned long pressedAt = 0;
unsigned long buttonDownCnt = 0;
boolean buttonPressed = false;
boolean buttonHold = false;

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
  initRTC();
  attachInterrupt(digitalPinToInterrupt(buttonPin),buttonPressedISR,FALLING);
  setTimerForCurrentTask();
}

void initRTC(){
    // initializing the rtc
    if(!rtc.begin()) {
        Serial.println("Couldn't find RTC!");
        Serial.flush();
        abort();
    }
    
    if(rtc.lostPower()) {
        // this will adjust to the date and time at compilation
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    
    //we don't need the 32K Pin, so disable it
    rtc.disable32K();
    
    // Making it so, that the alarm will trigger an interrupt
    pinMode(CLOCK_INTERRUPT_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(CLOCK_INTERRUPT_PIN), timerTimeoutISR, FALLING);
    
    // set alarm 1, 2 flag to false (so alarm 1, 2 didn't happen so far)
    // if not done, this easily leads to problems, as both register aren't reset on reboot/recompile
    rtc.clearAlarm(1);
    rtc.clearAlarm(2);
    
    // stop oscillating signals at SQW Pin
    // otherwise setAlarm1 will fail
    rtc.writeSqwPinMode(DS3231_OFF);
    
    // turn off alarm 2 (in case it isn't off already)
    // again, this isn't done at reboot, so a previously set alarm could easily go overlooked
    rtc.disableAlarm(2);
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


  if(buttonPressed){
    buttonPressed = false;
    if(notify){
      #ifdef DEBUG
      Serial.println("-> stop Notifying, start next task");
      #endif
      stopNotifying();
      startNextTask();
      showCurrentTask();
    } else if (!currentlyShowingTaskProgress && !currentlyShowingCurrentTask ){
      #ifdef DEBUG
      Serial.println("-> show current task progress");
      #endif
      showCurrentTaskProgress();
    }else  if ( currentlyShowingTaskProgress || currentlyShowingCurrentTask ){
      #ifdef DEBUG
      Serial.println("-> skip current task");
      #endif
      startNextTask();
      showCurrentTask();
    }
  }

  if(buttonHold){
    buttonHold = false;
    #ifdef DEBUG
    Serial.println("-> start pause");
    #endif
    showPause();
    pauseUntilInterrupt();
    turnOfLeds();
  }


  // update shit
  
  if (rtc.alarmFired(1)){
    #ifdef DEBUG
    Serial.println("-> notify end of task");
    #endif
    rtc.clearAlarm(1);
    startNotify();
  }

  if (notify){
    notify_update();
  }else  if (currentlyShowingTaskProgress || currentlyShowingCurrentTask){
    ledUpdate();
  }else  if (!notify && !currentlyShowingTaskProgress && !currentlyShowingCurrentTask){
    #ifdef DEBUG
    Serial.println("/- sleep");
    #endif
    sleep();
    #ifdef DEBUG
    if(buttonPressed) Serial.println("\\- woke up (button press)");
    else if(buttonHold) Serial.println("\\- woke up (button hold)");
    else if(timerTimedOut) Serial.println("\\- woke up (timer)");
    else Serial.println("\\- woke up (unknown)");
    #endif
  }
  

  
}













void ledUpdate(){
  if (turnOfLedsAtTime <= millis()){
      currentlyShowingTaskProgress = false;
      currentlyShowingCurrentTask = false;
      turnOfLeds();
    }
}



// ----------------------------- //
//                               //
//    arduino sleep functions    //
//                               //
// ----------------------------- //


void pauseUntilInterrupt(){
  rtc.clearAlarm(1);
  // time that we already did the task
  TimeSpan processedTime =  rtc.now() - taskStartTime;
  TimeSpan taskTime = TimeSpan((task_times[current_task]/1000L));
  delay(100);
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
    #ifdef DEBUG
    Serial.println("|- interrupt");
    #endif
  }
}


void setTimerForCurrentTask(){
  setTimerIn(TimeSpan(task_times[current_task]/1000L));
}
void setTimerIn(TimeSpan timerTime){
  #ifdef DEBUG
  Serial.println("  Set timer in "+String(timerTime.minutes())+" min, "+String(timerTime.seconds())+" sec");
  #endif
  rtc.clearAlarm(1);
  taskStartTime = rtc.now();
  timerTimedOut = false;
  if(!rtc.setAlarm1(
          taskStartTime + timerTime,
          DS3231_A1_Hour
  )) {
    #ifdef DEBUG
    Serial.println("Error, alarm wasn't set!");
    #endif
  }
}



// --------------------------------- //
//                                   //
//    arduino INTERRUPT functions    //
//                                   //
// --------------------------------- //



void buttonPressedISR(){
  if( false && (millis()-pressedAt) <= 80){
     #ifdef DEBUG
    Serial.println("-- B: To close button clicks! "+String((millis()-pressedAt)));
    #endif
  }else{
    //digitalWrite(greenLedPin, LOW);
    buttonDownCnt = 0;
    while( (digitalRead(buttonPin) == LOW) && (buttonDownCnt < 200000) ){
      buttonDownCnt++;
    }
    if(buttonDownCnt >= 200000){
      buttonHold = true;
      buttonPressed = false;
      endSleep = true;
      pressedAt = millis();
      #ifdef DEBUG
      Serial.println("-- B: hold for "+String(buttonDownCnt));
      #endif
    }else if(buttonDownCnt >= 500){
      buttonHold = false;
      buttonPressed = true;
      endSleep = true;
      pressedAt = millis();
      #ifdef DEBUG
      Serial.println("-- B: click for "+String(buttonDownCnt));
      #endif
    }else{
      #ifdef DEBUG
      Serial.println("-- B: To Short Button Down: "+String(buttonDownCnt));
      #endif
    }
  }
}

void timerTimeoutISR(){
  //detachInterrupt(digitalPinToInterrupt(buttonPin));
  #ifdef DEBUG
  Serial.println("-- Timer Timeout --");
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
