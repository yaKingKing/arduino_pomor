
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


//--------------------//
//                    //
//     Constants      //
//                    //
//--------------------//

#define OLED_RESET -1 //  Reset pin # (or -1 if sharing Arduino reset pin)
const String work_name = "work";
const String break_name = "break";
const String big_break_name = "BREAK";
const String task_names[] = {work_name,break_name,work_name,break_name,work_name,break_name,work_name,big_break_name};
const unsigned long task_times[] = {20L*1000L,5L*1000L,20L*1000L,5L*1000L,20L*1000L,5L*1000L,20L*1000L,20L*1000L};

//--------------------//
//                    //
//   Configuration    //
//                    //
//--------------------//

// buzzer
const int buzzerPin = 5; // must be pwm pin
const unsigned int buzzer_gentle_ms = 3*1000;
const unsigned int buzzer_medium_ms = 3*1000;
const unsigned long max_buzzing_time = 30*1000;

// button
const int buttonPin = 2; // must be interrupt pin
#define MIN_TICKS_BUTTON_UP 3000

//--------------------//
//                    //
//     Variables      //
//                    //
//--------------------//


int current_task = 0;
unsigned long current_task_start_time = 0;


//--------------------//
//                    //
//        Code        //
//                    //
//--------------------//


Adafruit_SSD1306 display(OLED_RESET);
// arduino setup variabes
const int display_update_ms = 1000;

boolean skipped_current_task = false;



void set_next_task(){
  Serial.println("next task");
  current_task_start_time = millis();
  current_task = (current_task+1)%4;;
}

void on_button_pressed(){
  //Serial.println("button");
  // disable buzzer directly
  digitalWrite(buzzerPin, LOW);
  set_next_task();
  skipped_current_task = true;
  // wait for button to be rised at least x ms
  unsigned int ticks_button_up = 0;
  while (ticks_button_up < MIN_TICKS_BUTTON_UP) {
    if (digitalRead(buttonPin) == LOW){
      ticks_button_up = 0;
    }
    ticks_button_up += 1;
  }
  //TODO: reset programm counter to loop
  asm volatile("ijmp" :: "z"(loop));
}

void setup(){
  pinMode(buzzerPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(buttonPin), on_button_pressed, LOW);
  Serial.begin(115200);
  Serial.println("start");
  // initialize with the I2C with addr 0x3C
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
}


int last_progress_bar_length = 0;
void draw_display(){
  // reset all 
  display.clearDisplay();
  last_progress_bar_length = 0;
  // print current task name
  //display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(0,0);
  display.println(task_names[current_task]);
  // print current cycle
  display.setCursor(128-(6*2*3),0);
  display.print(String((current_task/2)+1));
  display.println("/4");
  // draw progress bar outline
  display.drawRect(0,8*2+4,128,32, WHITE);   
  display.display();
}


void update_progress_bar(){
  
  unsigned long task_end_time = current_task_start_time + task_times[current_task];
  unsigned long remaining_time = max((task_end_time - millis()), 0);
  
  int bar_length = (128*remaining_time)/task_times[current_task];
  if( bar_length != last_progress_bar_length){
    display.fillRect(last_progress_bar_length,8*2+4,bar_length-last_progress_bar_length,32, WHITE);
    last_progress_bar_length = bar_length;
  }
}


void update_remaining_time(long remaining_ms){
  display.fillRect(128-(6*5),64-(8),128,8, BLACK);
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(128-(6*5),64-(8));
  long ms_left = remaining_ms;
  unsigned long seconds_left = (ms_left/(1000))%60;
  unsigned long minutest_left = (ms_left/1000)/60;
  String minutes_string = ((minutest_left < 10) ? "0": "" ) + String(minutest_left);
  String seconds_string = ((seconds_left < 10) ? "0": "") + String(seconds_left);
  display.print(minutes_string+":"+seconds_string);
 }






void update_display(long progress,long total){
  update_progress_bar();
  update_remaining_time(total-progress);
  display.display();
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
      delay(buzzer_times[buzzer_times_idx]);
      if(skipped_current_task){
        digitalWrite(buzzerPin, LOW);
        return;
      }
    }
    // increase volume
    volume_idx = min(volume_idx+1,volumes_lenth-1);
  }
 
  // waited to long, go to sleep mode
  digitalWrite(buzzerPin, LOW);
  while (not skipped_current_task){
    delay(1000*60*5);
    digitalWrite(buzzerPin, HIGH);
    delay(100);
    digitalWrite(buzzerPin, LOW);
  }
}



void wait_with_interrupt(){
  // wait for ms milli seconds or until button is pressed
  unsigned long waiting_time = task_times[current_task];
  Serial.println("wait "+String(waiting_time));
  delay(waiting_time);
}






void wait_and_fill_progress_bar(){
  long ttw = 1; // time-to-wait
  unsigned long task_time = task_times[current_task];
  unsigned long task_end_time = current_task_start_time + task_time;
  while( (not skipped_current_task) and (millis() < task_end_time) ){
    unsigned long remaining_time = max((task_end_time - millis()), 0);
    update_display(task_time-remaining_time,task_time);
    wait_with_interrupt();
  }
  if (not skipped_current_task){
    update_display(1,1);
  }
}








void loop() { // one loop equals one task
  Serial.println("start task: "+String(current_task));
  draw_display();
  wait_and_fill_progress_bar();
  delay(5000);
  //alert_user();
  set_next_task();
}
