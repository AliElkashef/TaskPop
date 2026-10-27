#pragma once
#include <stdint.h>
#include <math.h>

// Pure animation engine: no waits and no dependency on display or task storage.
class LedFlow {
public:
  enum Event { IDLE, ADD, DONE, DELETE_TASK, EDIT_TASK, UNDO, REOPEN, ERROR, WELCOME, PAGE };
  struct Color { float r,g,b; };
  struct Key { uint16_t ms; float level; };
  Event event=IDLE;
  uint32_t started=0, idleStarted=0;
  uint32_t eventCount=0;
  void trigger(Event next,uint32_t now) { event=next;started=now;eventCount++; }
  static float ease(float t) { return t*t*(3.f-2.f*t); }
  static Color mix(Color a,Color b,float t) {
    return {a.r+(b.r-a.r)*t,a.g+(b.g-a.g)*t,a.b+(b.b-a.b)*t};
  }
  static Color scale(Color c,float k) {return {c.r*k,c.g*k,c.b*k};}
  static uint8_t pwm(float value) {
    value=value<0?0:(value>1?1:value);
    return (uint8_t)lroundf(powf(value,2.2f)*255.f);
  }
  Color ambient(uint32_t now) const {
    // Blue -> pink -> cyan -> yellow -> teal: one soft 30-second orbit.
    const Color stops[]={{.13f,.27f,.87f},{.94f,.33f,.84f},{.18f,.78f,.90f},
                         {.97f,.90f,.42f},{.10f,.56f,.52f}};
    uint32_t phase=(now-idleStarted)%30000;
    int segment=phase/6000;
    float t=(phase%6000)/6000.f;
    float breath=.33f+.025f*sinf((now-idleStarted)*.0009f);
    return scale(mix(stops[segment],stops[(segment+1)%5],ease(t)),breath);
  }
  Color sample(uint32_t now) {
    if(event==IDLE)return ambient(now);
    // Unequal attack, rest, echo and decay create distinct gestures rather than a blink loop.
    static const Key add[]={{0,0},{65,1},{185,.12f},{245,0},{335,.78f},{590,0},{850,0}};
    static const Key done[]={{0,0},{55,.48f},{125,0},{215,1},{445,.82f},{940,0},{1200,0}};
    static const Key del[]={{0,0},{40,1},{125,0},{225,.72f},{300,0},{440,.35f},{650,0},{950,0}};
    static const Key edit[]={{0,0},{110,.9f},{260,.14f},{390,.55f},{740,0},{980,0}};
    static const Key undo[]={{0,0},{100,.35f},{165,.1f},{335,1},{800,0},{1060,0}};
    static const Key soft[]={{0,0},{130,.6f},{480,0},{700,0}};
    const Key* keys=soft;int n=4;
    Color color={.13f,.27f,.87f};
    switch(event) {
      case ADD: keys=add;n=7;color={1,.83f,.15f};break;
      case DONE:keys=done;n=7;color={.12f,1,.32f};break;
      case DELETE_TASK:keys=del;n=8;color={1,.04f,.05f};break;
      case EDIT_TASK:keys=edit;n=6;color={.94f,.33f,.84f};break;
      case UNDO:keys=undo;n=6;color={.1f,.85f,1};break;
      case ERROR:keys=del;n=8;color={1,.18f,.03f};break;
      case WELCOME:keys=undo;n=6;color={.94f,.33f,.84f};break;
      default:break;
    }
    uint32_t t=now-started;
    uint32_t end=keys[n-1].ms;
    // Explicit dark rest, then ease back into the ambient palette over 600 ms.
    if(t>=end+600){event=IDLE;return ambient(now);}
    if(t>=end)return scale(ambient(now),ease((t-end)/600.f));
    int i=0;while(i<n-2 && t>=keys[i+1].ms)i++;
    float f=ease((t-keys[i].ms)/float(keys[i+1].ms-keys[i].ms));
    float level=keys[i].level+(keys[i+1].level-keys[i].level)*f;
    return scale(color,.90f*level);
  }
};
