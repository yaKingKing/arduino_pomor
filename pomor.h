
enum task {
  work,
  pause,
  long_pause
};

String get_task_name(task t){
  switch(t){
    case pause: return "break";
    case work: return "work";
    case long_pause: return "big break";
    default: return "not found";
  }
}
