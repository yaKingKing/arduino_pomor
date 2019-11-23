#include <SPI.h>
#include <Wire.h>
#include "pomor.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// arduino setup variabes
const int buttonPin = 2;
const int buzzerPin = 4;
const int display_update_ms = 1000;

// buzzer options
const unsigned int buzzer_gentle_ms = 3*1000;
const unsigned int buzzer_medium_ms = 3*1000;

// pomor setup variables
const unsigned long work_time_ms = 20L*60L*1000L;
const unsigned long pause_time_ms = 5L*60L*1000L;
const unsigned long long_pause_time_ms = 20L*60L*1000L;


#define OLED_RESET -1 //  Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(OLED_RESET);

void setup(){
  pinMode(buzzerPin, OUTPUT);
  pinMode(buttonPin, INPUT);

  // initialize with the I2C with addr 0x3C
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
}


int current_cycle = 0;

// set by set_current_task()
task current_task = work;
unsigned long cycle_time = 0;
String task_name = "";
unsigned long task_start_time = 0;


void draw_head(){
  // print current task name
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(0,0);
  display.println(task_name);
  // print current cycle
  display.setCursor(128-(6*2*3),0);
  display.print(String(current_cycle+1));
  display.println("/4");
}
int last_progress_bar_length = 0;
void draw_progress_bar(){
  display.drawRect(0,8*2+4,128,32, WHITE);   
}
void fill_progress_bar(){
  unsigned long time_passed = (millis() - task_start_time);
  int bar_length = (128*time_passed) / cycle_time;
  if(bar_length > 128){
    bar_length = 128;
  }
  if( bar_length != last_progress_bar_length){
    display.fillRect(last_progress_bar_length,8*2+4,bar_length-last_progress_bar_length,32, WHITE);
    last_progress_bar_length = bar_length;
  }
}
void update_progress_bar(){
  fill_progress_bar();
  display.display();
}
void draw_remaining_time(){
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(128-(6*5),64-(8));
  unsigned long time_passed = (millis() - task_start_time);
  long ms_left = cycle_time - time_passed;
  if(ms_left > 0){
    unsigned long seconds_left = (ms_left/(1000))%60;
    unsigned long minutest_left = (ms_left/1000)/60;
    String minutes_string = ((minutest_left < 10) ? "0": "" ) + String(minutest_left);
    String seconds_string = ((seconds_left < 10) ? "0": "") + String(seconds_left);
    display.print(minutes_string+":"+seconds_string);
  }else{
    display.print("00:00");
  }
 }
void update_remaining_time(){
  display.fillRect(128-(6*5),64-(8),128,8, BLACK);
  draw_remaining_time();
  display.display();
}

void draw_display(){
  display.clearDisplay();
  draw_head();
  draw_progress_bar();
  fill_progress_bar();
  draw_remaining_time();
  display.display();
}


boolean wait_with_interrupt(unsigned long ms){
  // wait for ms milli seconds or until button is pressed
  // return true if button is pressed, otherwise false
  bool button_pressed = false;
  bool time_exceeded = false;
  unsigned long internal_start_time = millis();
  while (not button_pressed and not time_exceeded) {
    button_pressed = (digitalRead(buttonPin) == LOW);
    time_exceeded = (millis()-internal_start_time) >= ms;
  }
  return button_pressed;
}



bool wait_and_update(){
  unsigned long time_passed = (millis() - task_start_time);
  long rest_time = cycle_time - time_passed;
  while(rest_time > 0){
    long ttw = (rest_time < display_update_ms) ? rest_time : display_update_ms;
    if(ttw < 0){
      continue;
    }
    update_progress_bar();
    update_remaining_time();
    bool button_pressed = wait_with_interrupt(ttw);
    if(button_pressed){
      return true;
    }
    time_passed = (millis() - task_start_time);
    rest_time = cycle_time - time_passed;
  }
  update_progress_bar();
  return false;
}

void wait_for_button(){
  bool button_pressed = false;
  int buzzer_times[] = {100,100,500,500};
  int buzzer_lines_lenght = 4;
  while (true){
    for(int i=0; i< buzzer_lines_lenght; i++){
      digitalWrite(buzzerPin, !digitalRead(buzzerPin));
      button_pressed = wait_with_interrupt(buzzer_times[i]);
      if(button_pressed){
        digitalWrite(buzzerPin, LOW);
        return;
      }
    }
  }
}

void set_current_task(task t){
  current_task = t;
  task_start_time = millis();
  switch(t){
    case pause: 
      cycle_time = pause_time_ms;
      task_name = "break";
      break;
    case work: 
      cycle_time = work_time_ms;
      task_name = "work";
      break;
    case long_pause: 
      cycle_time = long_pause_time_ms;
      task_name = "BREAK";
      break;
  }
  draw_display();
}

void loop() {

  bool skip = false;
  for(current_cycle=0; current_cycle<4; current_cycle++){
    set_current_task(work);
    skip = wait_and_update();
    if(not skip){
      wait_for_button();
    }else{
      skip = false;
    }
    if(current_cycle != 3){
      set_current_task(pause);
    }else{
      set_current_task(long_pause);
    }
    skip = wait_and_update();
    if(not skip){
      wait_for_button();
    }else{
      skip = false;
    }
  }
}
