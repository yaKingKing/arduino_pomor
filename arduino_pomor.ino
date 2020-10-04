#include "notes.h"
#include <avr/sleep.h>
#include <RTClib.h>


//debug variables
#define DEBUG
//#define DEBUG_SHORT_POMOR_TIMES


// pin setup
#define CLOCK_INTERRUPT_PIN 3
#define BUTTON_PIN 2
#define RED_LED_PIN 6
#define GREEN_LED_PIN 5
#define BUZZER_PIN 4
const int led_array_pins[] = {9,10,11,12};



// RTC Variables
RTC_DS3231 rtc;
boolean timerTimedOut = false;
boolean endSleep = false;


// button variables
#define SHORT_BUTTON_DOWN_CNT 500
#define LONG_BUTTON_DOWN_CNT 200000
unsigned long buttonDownCnt = 0;
boolean buttonPressed = false; // pressed: Button was hold down for a short time
boolean buttonHold = false;    // hold:    Button was hold for a long time


// pomor variables
const unsigned long task_times[] = 
#ifdef DEBUG_SHORT_POMOR_TIMES
  {25L,5L,25L,5L,25L,5L,25L,20L};
#else
  {25L*60L,5L*60L,25L*60L,5L*60L,25L*60L,5L*60L,25L*60L,20L*60L};
#endif
int currentTask = 0;
boolean pause = false;
DateTime taskStartTime; // time the current task was started 


// led output variables
const int showCurrentTaskProgressLedTime = 1000;   // ms led show: how long do the leds shine when showing the current progress of a task
boolean currentlyShowingTaskProgress = false;
boolean currentlyShowingCurrentTask = false;
unsigned long turnOfLedsAtTime = 0;


// buzzer variables
bool notify = false;
const int pulseTimes[] = {100,100,500,500};
unsigned long pulseStart = 0;
int pulseI = 0;
int pulseLoops = 0;











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

  // handle button events 
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
    showCurrentTaskProgress();
  }


  // update shit
  ledUpdate();
  notifyUpdate();
  rtcUpdate();
  
  // if nothing else is to do, go sleeping
  if (!notify && !currentlyShowingTaskProgress && !currentlyShowingCurrentTask){
    sleep();
  }
  
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


void setTimerIn(TimeSpan timerTime){
  debug("  Set timer in "+String(timerTime.minutes())+" min, "+String(timerTime.seconds())+" sec");
  rtc.clearAlarm(1);
  taskStartTime = rtc.now();
  timerTimedOut = false;
  if(!rtc.setAlarm1(
          taskStartTime + timerTime,
          DS3231_A1_Hour
  )) {
    debug("Error, alarm wasn't set!");
  }
}


void debug(char* message){
  debug(message);
}



// ----------------------------- //
//                               //
//        update functions       //
//                               //
// ----------------------------- //


void rtcUpdate(){
  if (rtc.alarmFired(1)){
    debug("-> notify end of task");
    rtc.clearAlarm(1);
    startNotify();
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
  TimeSpan taskTime = TimeSpan((task_times[currentTask]));
  sleep();
  // remove button events that woke up the Interrupt
  buttonPressed = false;
  buttonHold = false;
  
  setTimerIn( taskTime - processedTime );
  // override task start time, so that later calculation is successfull
  taskStartTime = rtc.now() - processedTime;

}


void sleep(){
  #ifdef DEBUG
  // delay for last serial println to finish
  Serial.println("/- sleep");
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
  }
  #ifdef DEBUG
  if(buttonPressed) Serial.println("\\- woke up (button press)");
  if(buttonHold) Serial.println("\\- woke up (button hold)");
  if(timerTimedOut) Serial.println("\\- woke up (timer)");
  if(!buttonPressed && !buttonHold && !timerTimedOut) Serial.println("\\- woke up (unknown)");
  #endif
}


void setTimerForCurrentTask(){
  setTimerIn(TimeSpan(task_times[currentTask]));
}



// --------------------------------- //
//                                   //
//    arduino INTERRUPT functions    //
//                                   //
// --------------------------------- //


void timerTimeoutISR(){
  //detachInterrupt(digitalPinToInterrupt(buttonPin));
  #ifdef DEBUG
  Serial.println("_ISR_: timed out");
  #endif
  timerTimedOut = true;
  endSleep = true;
}


void buttonPressedISR(){
  #ifdef DEBUG
  Serial.println("_ISR_: Button clicked");
  #endif

  buttonDownCnt = 0;
  while( (digitalRead(BUTTON_PIN) == LOW) && (buttonDownCnt < LONG_BUTTON_DOWN_CNT) ){
    buttonDownCnt++;
  }
  if(buttonDownCnt >= LONG_BUTTON_DOWN_CNT){
    buttonHold = true;
    buttonPressed = false;
    endSleep = true;
    #ifdef DEBUG
    Serial.println("-- B: hold for "+String(buttonDownCnt));
    #endif
  }else if(buttonDownCnt >= SHORT_BUTTON_DOWN_CNT){
    buttonHold = false;
    buttonPressed = true;
    endSleep = true;
    #ifdef DEBUG
    Serial.println("-- B: click for "+String(buttonDownCnt));
    #endif
  }else{
    #ifdef DEBUG
    Serial.println("-- B: To Short Button Down: "+String(buttonDownCnt));
    #endif
  }
  
}







// show task
void showCurrentTaskProgress(){
  TimeSpan passedTime = rtc.now() - taskStartTime;
  int n_leds = min( ( (passedTime.totalseconds()*5)  /   (task_times[currentTask]) )  ,4);
  #ifdef DEBUG
  Serial.println("   passedTime: "+String(passedTime.minutes())+" min, "+String(passedTime.seconds())+" sec ("+String(n_leds)+" leds)");
  #endif
  turnOfLeds();
  for (int i = 0; i<n_leds; i++){
    digitalWrite(led_array_pins[i], LOW);
  }
  if (currentTask%2 == 0){
   // work
    digitalWrite(RED_LED_PIN, LOW);
  }else{
    // pause
    digitalWrite(GREEN_LED_PIN, LOW);
  }
  turnOfLedsAtTime = millis() + showCurrentTaskProgressLedTime;
  currentlyShowingTaskProgress = true;
  currentlyShowingCurrentTask = false;
}
void showCurrentTask(){
  turnOfLeds();
  digitalWrite(led_array_pins[currentTask/2], LOW);
  if (currentTask%2 == 0){
   // work
    digitalWrite(RED_LED_PIN, LOW);
  }else{
    // pause
    digitalWrite(GREEN_LED_PIN, LOW);
  }
  turnOfLedsAtTime = millis() + 3000;
  currentlyShowingTaskProgress = false;
  currentlyShowingCurrentTask = true;
}
void showPause(){
  currentlyShowingTaskProgress = false;
  currentlyShowingCurrentTask = false;
  turnOfLeds();
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(led_array_pins[currentTask/2], LOW);
}



// skip task
void startNextTask(){
  currentTask = (currentTask+1) % 8;
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
  digitalWrite(BUZZER_PIN, LOW);
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
        tone(BUZZER_PIN, NOTE_D1, 100);
      }
      if (pulseLoops >= 10 && pulseLoops <= 14 && pulseLoops%2 == 0){
        tone(BUZZER_PIN, NOTE_A2, 100);
      }
    }
    if (pulseLoops > 35){
       stopNotifying();
       pauseUntilInterrupt();
       startNextTask();
       showCurrentTaskProgress();
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
  pinMode(BUTTON_PIN, INPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  for (int i = 0; i<4; i++){
    pinMode(led_array_pins[i], OUTPUT);
    digitalWrite(led_array_pins[i], HIGH);
  }
  digitalWrite(GREEN_LED_PIN, HIGH);
  digitalWrite(RED_LED_PIN, HIGH);
}

void initBuzzer(){
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);  
}

void initButton(){
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN),buttonPressedISR,FALLING);
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
  digitalWrite(RED_LED_PIN, LOW);
  delay(200);
  digitalWrite(RED_LED_PIN, HIGH);
  digitalWrite(GREEN_LED_PIN, LOW);
  delay(200);
  digitalWrite(GREEN_LED_PIN, HIGH);
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
  digitalWrite(RED_LED_PIN, state);
  digitalWrite(GREEN_LED_PIN, state);
}
