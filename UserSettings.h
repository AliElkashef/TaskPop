#pragma once
#include <stdint.h>
struct UserSettings {
  uint32_t magic=0x54505331;
  uint8_t brightness=100,rgbLevel=100,focusMinutes=25;
  bool rgbOn=true,idleOn=true,screenBlink=true;
  void normalize() {
    if(brightness<10)brightness=10;if(brightness>100)brightness=100;
    if(rgbLevel>100)rgbLevel=100;
    if(focusMinutes<1)focusMinutes=1;if(focusMinutes>90)focusMinutes=90;
  }
};
