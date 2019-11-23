#include <Adafruit_SSD1306.h>

#define OLED_RESET -1 //  Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(OLED_RESET);


int last_progress_bar_length = 0;


void draw_display(String task_string,int cycle){
  // reset all 
  display.clearDisplay();
  last_progress_bar_length = 0;
  // print current task name
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(0,0);
  display.println(task_string);
  // print current cycle
  display.setCursor(128-(6*2*3),0);
  display.print(String(cycle+1));
  display.println("/4");
  // draw progress bar outline
  display.drawRect(0,8*2+4,128,32, WHITE);   
  display.display();
}


void update_progress_bar(long progress,long total){
  int bar_length = (128*progress)/total;
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
  update_progress_bar(progress,total);
  update_remaining_time(total-progress);
  display.display();
}
