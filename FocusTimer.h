#pragma once
#include <stdint.h>

// Monotonic countdown: rendering delays never change the duration of a session.
class FocusTimer {
public:
  enum State { READY, RUNNING, PAUSED, FINISHED };
  State state=READY;
  uint16_t minutes=25;
  uint32_t totalMs=25UL*60000, budgetMs=25UL*60000, started=0, finishedAt=0;
  void configure(int value) {
    minutes=value<1?1:(value>90?90:value);
    totalMs=budgetMs=(uint32_t)minutes*60000UL;state=READY;
  }
  void start(uint32_t now) {totalMs=budgetMs=(uint32_t)minutes*60000UL;started=now;state=RUNNING;}
  uint32_t remaining(uint32_t now) const {
    if(state==FINISHED)return 0;
    if(state!=RUNNING)return budgetMs;
    uint32_t elapsed=now-started;
    return elapsed>=budgetMs?0:budgetMs-elapsed;
  }
  bool update(uint32_t now) {
    if(state!=RUNNING || remaining(now)>0)return false;
    finishedAt=started+budgetMs;budgetMs=0;state=FINISHED;return true;
  }
  void pause(uint32_t now) {
    if(state!=RUNNING)return;
    if(update(now))return;
    budgetMs=remaining(now);state=PAUSED;
  }
  void resume(uint32_t now) {if(state==PAUSED){started=now;state=RUNNING;}}
  void stop() {configure(minutes);}
  float progress(uint32_t now) const {return totalMs?1.f-(float)remaining(now)/totalMs:0;}
  bool alarmPulse(uint32_t now) const {
    if(state!=FINISHED)return false;
    uint32_t phase=(now-finishedAt)%2800;
    return phase<200 || (phase>=440 && phase<700);
  }
};
