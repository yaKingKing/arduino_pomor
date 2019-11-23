
enum task {
  work,
  pause,
  long_pause
};

String get_task_name(task t){
  switch(t){
    case pause: return "break";
    case work: return "work";
    case long_pause: return "BREAK";
    default: return "not found";
  }
}

task get_next_task(task t,int cycle){
  if( (cycle == 3) and t == work){
    return long_pause;
  }
  switch(t){
    case work: return pause;
    case pause: return work;
    case long_pause: return work;
  }
}

int get_next_cycle(task t, int cycle){
  switch(t){
    case work: return cycle;
    case pause:;
    case long_pause: return (cycle+1)%4;
  }
}


task get_task_time(task t){
  switch(t){
    case work: return 20L*60L*1000L;
    case pause: return  5L*60L*1000L;
    case long_pause: return 20L*60L*1000L;
  }
}
