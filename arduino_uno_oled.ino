#define DEBUG 1
#undef DEBUG

#include <Wire.h>
#include "pomor_task.h"
#include "pomor_display.h"
#include <Adafruit_GFX.h>
#ifdef DEBUG
#include <SPI.h>
#endif


// arduino setup variabes
const int buttonPin = 2; // must be interrupt pin
const int buzzerPin = 5; // must be pwm pin
const int display_update_ms = 1000;

// buzzer options
const unsigned int buzzer_gentle_ms = 3*1000;
const unsigned int buzzer_medium_ms = 3*1000;
#ifdef DEBUG
const unsigned long max_buzzing_time = 3*1000;
#else
const unsigned long max_buzzing_time = 30*1000;
#endif
// non-constant variables
task current_task = work;
unsigned long current_task_start_time = 0;
int current_pomor_cycle = 0;
boolean button_pressed = false;


void setup(){
  pinMode(buzzerPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(buttonPin), on_button_pressed, FALLING);
  #ifdef DEBUG
  Serial.begin(115200);
  #endif
  // initialize with the I2C with addr 0x3C
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
}

unsigned long last_time_pressed = 0;
void on_button_pressed(){
  // wait for button to rise at leas 500 ms
  if(millis()>=(last_time_pressed+300)){
    last_time_pressed=millis();
    button_pressed = true;
  }
}


void loop() {
  #ifdef DEBUG
  Serial.println(get_task_name(current_task)+"  "+String(current_pomor_cycle)+"/4");
  #endif
  button_pressed = false;
  draw_display(get_task_name(current_task), current_pomor_cycle);
  wait_and_fill_progress_bar();
  alert_user();
  set_next_task();
}



void wait_and_fill_progress_bar(){
  long ttw = 1; // time-to-wait
  unsigned long task_time = get_task_time(current_task);
  unsigned long task_end_time = current_task_start_time + task_time;
  #ifdef DEBUG
  Serial.println("wait_and_fill_progress_bar(); // task_time="+String(task_time));
  #endif
  while( (not button_pressed) and (millis() < task_end_time) ){
    unsigned long remaining_time = max((task_end_time - millis()), 0);
    update_display(task_time-remaining_time,task_time);
    ttw = min(remaining_time, display_update_ms);
    wait_with_interrupt(ttw);
  }
  if (not button_pressed){
    update_display(1,1);
  }
  #ifdef DEBUG
  if (button_pressed){
    Serial.println("\- button pressed");
  }else{
    Serial.println("\- time exceeded");
  }
  #endif
}


void set_next_task(){
  current_task_start_time = millis();
  task next_task = get_next_task(current_task, current_pomor_cycle);
  int next_pomor_cycle = get_next_cycle(current_task, current_pomor_cycle);
  current_task = next_task;
  current_pomor_cycle = next_pomor_cycle;
}


void alert_user(){
  int buzzer_times[] = {100,100,500,500};
  int buzzer_times_lenght = 4;
  int volumes[] = {1,1,1,1,10,10,20,20,50,50,255};
  int volume_idx = 0;
  int volumes_lenth = 11;
  int state = 0;
  unsigned long start_time = millis();
  while (millis()-start_time <= max_buzzing_time){
    // iterate throug buzzer_times to get specific sound
    for(int buzzer_times_idx=0; buzzer_times_idx< buzzer_times_lenght; buzzer_times_idx++){
      state = (state==0)?1:0;
      analogWrite(buzzerPin, volumes[volume_idx]*state);
      wait_with_interrupt(buzzer_times[buzzer_times_idx]);
      if(button_pressed){
        digitalWrite(buzzerPin, LOW);
        return;
      }
    }
    // increase volume
    volume_idx = min(volume_idx+1,volumes_lenth-1);
  }
  #ifdef DEBUG
  Serial.println("buzzing time exceeded");
  #endif
  // waited to long, go to sleep mode
  digitalWrite(buzzerPin, LOW);
  while (not button_pressed){
    #ifdef DEBUG
    wait_with_interrupt(1000*5);
    #else
    wait_with_interrupt(1000*60*5);
    #endif
    digitalWrite(buzzerPin, HIGH);
    wait_with_interrupt(100);
    digitalWrite(buzzerPin, LOW);
  }
}



void wait_with_interrupt(unsigned long ms){
  // wait for ms milli seconds or until button is pressed
  // return true if button is pressed, otherwise false
  if (button_pressed){
    return;
  }
  unsigned long internal_start_time = millis();
  bool time_exceeded = false;
  while (not button_pressed and not time_exceeded) {
    time_exceeded = (millis()-internal_start_time) >= ms;
  }
}
