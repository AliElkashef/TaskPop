#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <Preferences.h>
#include "LedFlow.h"

LedFlow glow;
uint8_t ledDuty[3]={255,255,255};
const int ledPins[3]={22,16,17}; // Common-anode RGB: low = on.
const int ledChannels[3]={4,5,6};
void updateGlow() {
  static uint32_t last=0;
  uint32_t now=millis();if(now-last<16)return;last=now;
  LedFlow::Color c=glow.sample(now);
  float components[3]={c.r,c.g,c.b};
  for(int i=0;i<3;i++) {
    ledDuty[i]=255-LedFlow::pwm(components[i]);
    ledcWrite(ledChannels[i],ledDuty[i]);
  }
}
void beginGlow() {
  for(int i=0;i<3;i++) {
    pinMode(ledPins[i],OUTPUT);digitalWrite(ledPins[i],HIGH);
    ledcSetup(ledChannels[i],5000,8);
    ledcWrite(ledChannels[i],255);
    ledcAttachPin(ledPins[i],ledChannels[i]);
  }
  glow.idleStarted=millis();glow.trigger(LedFlow::WELCOME,millis());
}

// LCDWIKI E32R28T: LCD on HSPI; resistive touch on a separate VSPI bus.
TFT_eSPI lcd;
TFT_eSprite ui(&lcd);
SPIClass touchBus(VSPI);
XPT2046_Touchscreen touch(33);
Preferences store;
constexpr int W=240, H=320, MAX_TASKS=30, MAX_TEXT=64;
constexpr uint8_t SCREEN_ROTATION=0; // Portrait, USB connector at the bottom.
// A 4-bit indexed framebuffer needs only 38 KB and preserves these exact RGB565 colors.
const uint16_t TEAL=0, CREAM=1, INK=2, PINK=3, YELLOW=4, BLUE=5, GRID=6, MUTED=7;
const uint16_t palette[16]={0x1C70,0xF79B,0x2925,0xEABA,0xF72D,0x223B,0x1BAD,0x6BCC};
struct Task { char text[MAX_TEXT+1]; bool done; };
Task tasks[MAX_TASKS], removed;
int count=0, page=0, editing=-1, removedAt=-1;
String draft, serialLine;
bool upper=false, symbols=false, dirty=true, storageOK=true;
enum View { LIST, EDIT, CALIBRATE };
View view=LIST;
uint32_t undoUntil=0, noticeUntil=0;
String notice;
int xmin=562,xmax=3604,ymin=544,ymax=3720;
int calStep=0, calX[4],calY[4];
const int targetX[4]={22,217,217,22}, targetY[4]={44,44,285,285};

bool hit(int x,int y,int bx,int by,int bw,int bh) {
  return x>=bx && x<bx+bw && y>=by && y<by+bh;
}
void label(String s,int x,int y,int font=2,uint16_t color=INK) {
  ui.setTextColor(color); ui.setTextDatum(TL_DATUM); ui.drawString(s,x,y,font);
}
void center(String s,int x,int y,int font=2,uint16_t color=INK) {
  ui.setTextColor(color); ui.setTextDatum(MC_DATUM); ui.drawString(s,x,y,font);
}
void card(int x,int y,int w,int h,uint16_t color,uint16_t shadow=INK,int radius=9) {
  ui.fillRoundRect(x+2,y+4,w,h,radius,shadow);
  ui.fillRoundRect(x,y,w,h,radius,color);
  ui.drawRoundRect(x,y,w,h,radius,INK);
}
void background() {
  ui.fillSprite(TEAL);
  for(int x=0;x<W;x+=24) ui.drawFastVLine(x,0,H,GRID);
  for(int y=0;y<H;y+=24) ui.drawFastHLine(0,y,W,GRID);
}
void tell(String s) { notice=s;noticeUntil=millis()+2300;dirty=true; }
bool saveTasks() {
  // One blob keeps the task count and records in a single atomic NVS update.
  struct Data { uint32_t magic; uint16_t version; uint16_t size; Task rows[MAX_TASKS]; } data={};
  data.magic=0x54504F50;data.version=1;data.size=count;
  memcpy(data.rows,tasks,sizeof(tasks));
  storageOK=store.putBytes("tasks",&data,sizeof(data))==sizeof(data);
  if(!storageOK) {tell("Save failed - try again");glow.trigger(LedFlow::ERROR,millis());}
  return storageOK;
}
void loadTasks() {
  struct Data { uint32_t magic; uint16_t version; uint16_t size; Task rows[MAX_TASKS]; } data={};
  if(store.getBytesLength("tasks")==sizeof(data) && store.getBytes("tasks",&data,sizeof(data))==sizeof(data)
    && data.magic==0x54504F50 && data.version==1 && data.size<=MAX_TASKS) {
    count=data.size; memcpy(tasks,data.rows,sizeof(tasks));
    for(int i=0;i<count;i++) tasks[i].text[MAX_TEXT]=0;
  }
  xmin=store.getInt("xmin",562);xmax=store.getInt("xmax",3604);
  ymin=store.getInt("ymin",544);ymax=store.getInt("ymax",3720);
  int calRotation=store.getInt("calrot",2); // Existing calibration was recorded at rotation 2.
  if(xmax-xmin<500 || ymax-ymin<500) {xmin=562;xmax=3604;ymin=544;ymax=3720;calRotation=2;}
  if(calRotation!=SCREEN_ROTATION) {
    // XPT2046 rotation 0 is the inverse of both rotation-2 raw axes.
    int oldMin=xmin;xmin=4095-xmax;xmax=4095-oldMin;
    oldMin=ymin;ymin=4095-ymax;ymax=4095-oldMin;
  }
}
void drawList() {
  background();
  card(10,10,218,67,CREAM,PINK,12);
  ui.fillRoundRect(82,7,74,6,3,PINK);ui.drawRoundRect(82,7,74,6,3,INK);
  label("Task Pop",22,22,4);
  card(187,21,30,30,YELLOW,INK,7); center("+",202,36,4);
  int done=0;for(int i=0;i<count;i++) if(tasks[i].done) done++;
  label(String(count-done)+" to do  /  "+String(done)+" done",23,53,2);
  for(int row=0;row<3;row++) {
    int i=page*3+row, y=91+row*52;
    if(i>=count) break;
    card(10,y,218,45,CREAM,row%2?YELLOW:PINK,8);
    ui.fillRoundRect(19,y+11,23,23,5,tasks[i].done?YELLOW:CREAM);
    ui.drawRoundRect(19,y+11,23,23,5,INK);
    if(tasks[i].done) { ui.drawLine(24,y+22,29,y+28,INK);ui.drawLine(29,y+28,37,y+16,INK); }
    String s=tasks[i].text, a,b;
    while(s.length() && ui.textWidth(s,2)>164) s.remove(s.length()-1);
    a=s;
    if(a.length()<strlen(tasks[i].text)) {
      int split=a.lastIndexOf(' ');if(split>8) a=a.substring(0,split);
      b=String(tasks[i].text).substring(a.length()); b.trim();
      while(ui.textWidth(b,2)>151 && b.length()) b.remove(b.length()-1);
      if(a.length()+b.length()<strlen(tasks[i].text)) b+="..";
    }
    label(a,50,y+(b.length()?5:14),2,tasks[i].done?MUTED:INK);
    if(b.length())label(b,50,y+23,2,tasks[i].done?MUTED:INK);
  }
  if(!count) {
    card(20,105,198,124,CREAM,YELLOW,12);
    ui.drawRoundRect(96,119,46,43,7,INK);
    ui.fillRoundRect(107,115,24,8,3,PINK);ui.drawRoundRect(107,115,24,8,3,INK);
    ui.drawLine(105,141,113,149,INK);ui.drawLine(113,149,132,130,INK);
    center("A little space",119,181,2);center("for your big plans.",119,200,2);
    center("Tap + to add your first task",120,245,1,CREAM);
  }
  else {
    card(10,251,40,29,CREAM,BLUE,7);center("<",30,266,2,page?INK:MUTED);
    center(String(page+1)+" / "+String((count+2)/3),120,268,2,CREAM);
    card(188,251,40,29,CREAM,BLUE,7);center(">",208,266,2,(page+1)*3<count?INK:MUTED);
  }
  if(undoUntil && (int32_t)(undoUntil-millis())>0) {
    card(10,289,218,23,YELLOW,INK,7);center("Removed  -  tap to UNDO",119,301,1);
  } else if(noticeUntil && (int32_t)(noticeUntil-millis())>0) center(notice,120,302,1,CREAM);
  else {label("TAP TASK TO EDIT",11,300,1,CREAM); card(173,290,55,23,CREAM,INK,5);center("TOUCH",200,302,1);}
}
void drawInput() {
  ui.fillRoundRect(10,45,218,72,10,CREAM);ui.drawRoundRect(10,45,218,72,10,INK);
  String visible=draft;
  // Keep the last two lines visible as the caret advances.
  while(ui.textWidth(visible,2)>390 && visible.length()) visible.remove(0,1);
  String a=visible,b;
  while(ui.textWidth(a,2)>195 && a.length()) a.remove(a.length()-1);
  b=visible.substring(a.length());
  if(!draft.length()) label("What needs doing?",20,56,2,MUTED);
  else { label(a,20,53,2); if(b.length())label(b,20,76,2); }
  int caret=20+ui.textWidth(b.length()?b:a,2); int cy=b.length()?76:53;
  if(draft.length()) ui.drawFastVLine(min(caret+1,218),cy,17,INK);
  label(String(draft.length())+"/64",186,101,1,MUTED);
  if(noticeUntil && (int32_t)(noticeUntil-millis())>0) label(notice,20,101,1,INK);
}
void drawKey(int x,int y,int w,String text,uint16_t color=CREAM) {
  card(x,y,w,32,color,INK,5);center(text,x+w/2,y+16,2);
}
void drawEditor() {
  background();
  label(editing<0?"New task":"Edit task",12,13,4,CREAM);
  drawInput();
  drawKey(10,127,65,"Cancel",CREAM);
  drawKey(83,127,65,editing<0?"Clear":"Delete",PINK);
  drawKey(156,127,72,"Save",draft.length()?YELLOW:CREAM);
  const char* r0=symbols?"1234567890":(upper?"QWERTYUIOP":"qwertyuiop");
  const char* r1=symbols?"@#$%&*()-":(upper?"ASDFGHJKL":"asdfghjkl");
  const char* r2=symbols?".,?!/_:":(upper?"ZXCVBNM":"zxcvbnm");
  for(int i=0;i<10;i++)drawKey(1+i*24,170,22,String(r0[i]));
  for(int i=0;i<9;i++)drawKey(13+i*24,207,22,String(r1[i]));
  drawKey(1,244,31,symbols?"ABC":"Aa",YELLOW);
  for(int i=0;i<7;i++)drawKey(35+i*24,244,22,String(r2[i]));
  drawKey(205,244,33,"<",PINK);
  drawKey(1,281,49,symbols?"ABC":"123",PINK);
  drawKey(55,281,128,"space");drawKey(188,281,50,"OK",YELLOW);
}
void drawCalibration() {
  background();center("Touch setup",120,17,2,CREAM);
  center("Tap the center of each +",120,155,1,CREAM);
  center(String(calStep+1)+" / 4",120,176,2,CREAM);
  int x=targetX[calStep],y=targetY[calStep];
  ui.fillCircle(x,y,14,CREAM);ui.drawCircle(x,y,14,INK);
  ui.drawFastHLine(x-10,y,21,INK);ui.drawFastVLine(x,y-10,21,INK);
}
void draw() {
  if(view==LIST)drawList();else if(view==EDIT)drawEditor();else drawCalibration();
  ui.pushSprite(0,0);dirty=false;
}
void openEditor(int i) { editing=i;draft=i<0?"":String(tasks[i].text);upper=false;symbols=false;view=EDIT;dirty=true; }
void commitDraft() {
  String clean=draft;clean.trim();
  if(!clean.length()) {tell("Write a task first");glow.trigger(LedFlow::ERROR,millis());return;}
  if(editing<0 && count>=MAX_TASKS){view=LIST;tell("List full: 30 tasks");glow.trigger(LedFlow::ERROR,millis());return;}
  int i=editing<0?count++:editing;
  if(editing<0)tasks[i].done=false;
  clean.toCharArray(tasks[i].text,MAX_TEXT+1);page=i/3;
  if(saveTasks())glow.trigger(editing<0?LedFlow::ADD:LedFlow::EDIT_TASK,millis());
  view=LIST;dirty=true;
}
void removeTask(int i) {
  removed=tasks[i];removedAt=i;
  for(int j=i;j<count-1;j++)tasks[j]=tasks[j+1];
  count--;page=min(page,max(0,(count-1)/3));undoUntil=millis()+8000;
  if(saveTasks())glow.trigger(LedFlow::DELETE_TASK,millis());
  view=LIST;dirty=true;
}
void tap(int x,int y) {
  dirty=true;
  if(view==LIST) {
    if(hit(x,y,178,15,50,49)){if(count<MAX_TASKS)openEditor(-1);else tell("List full: 30 tasks");return;}
    if(hit(x,y,10,91,218,149)) {
      int row=(y-91)/52,i=page*3+row;
      if((y-91)%52<45 && row<3 && i<count) {
        if(x<47){tasks[i].done=!tasks[i].done;if(saveTasks())glow.trigger(tasks[i].done?LedFlow::DONE:LedFlow::REOPEN,millis());}else openEditor(i);
      }
      return;
    }
    if(hit(x,y,10,251,40,32) && page>0){page--;glow.trigger(LedFlow::PAGE,millis());}
    else if(hit(x,y,188,251,40,32) && (page+1)*3<count){page++;glow.trigger(LedFlow::PAGE,millis());}
    else if(hit(x,y,10,289,218,31) && undoUntil && (int32_t)(undoUntil-millis())>0 && count<MAX_TASKS) {
      for(int j=count;j>removedAt;j--)tasks[j]=tasks[j-1];tasks[removedAt]=removed;count++;page=removedAt/3;
      undoUntil=0;if(saveTasks())glow.trigger(LedFlow::UNDO,millis());
    } else if(hit(x,y,173,289,55,31)){view=CALIBRATE;calStep=0;}
  } else if(view==EDIT) {
    if(hit(x,y,10,127,65,32)){view=LIST;return;}
    if(hit(x,y,83,127,65,32)){if(editing>=0)removeTask(editing);else draft="";return;}
    if(hit(x,y,156,127,72,32)||hit(x,y,188,281,50,32)){commitDraft();return;}
    char c=0;
    if(hit(x,y,1,170,238,32) && (x-1)%24<22)c=(symbols?"1234567890":(upper?"QWERTYUIOP":"qwertyuiop"))[(x-1)/24];
    else if(hit(x,y,13,207,214,32) && (x-13)%24<22)c=(symbols?"@#$%&*()-":(upper?"ASDFGHJKL":"asdfghjkl"))[(x-13)/24];
    else if(hit(x,y,35,244,166,32) && (x-35)%24<22)c=(symbols?".,?!/_:":(upper?"ZXCVBNM":"zxcvbnm"))[(x-35)/24];
    else if(hit(x,y,1,244,31,32)){if(symbols)symbols=false;else upper=!upper;}
    else if(hit(x,y,205,244,33,32)){if(draft.length())draft.remove(draft.length()-1);}
    else if(hit(x,y,1,281,49,32))symbols=!symbols;
    else if(hit(x,y,55,281,128,32))c=' ';
    if(c && draft.length()<MAX_TEXT)draft+=c;
  }
}
void calibrationPoint(int x,int y) {
  calX[calStep]=x;calY[calStep]=y;calStep++;
  if(calStep==4) {
    int left=(calX[0]+calX[3])/2,right=(calX[1]+calX[2])/2;
    int top=(calY[0]+calY[1])/2,bottom=(calY[2]+calY[3])/2;
    if(right-left>500 && bottom-top>500) {
      xmin=left-(right-left)*22/195;xmax=left+(right-left)*217/195;
      ymin=top-(bottom-top)*44/241;ymax=top+(bottom-top)*275/241;
      store.putInt("xmin",xmin);store.putInt("xmax",xmax);store.putInt("ymin",ymin);store.putInt("ymax",ymax);
      store.putInt("calrot",SCREEN_ROTATION);
      tell("Touch ready");
    } else tell("Setup failed - try again");
    view=LIST;
  }
  dirty=true;
}
void diagnostics() {
  while(Serial.available()) {
    char c=Serial.read();if(c=='\r')continue;
    if(c!='\n'){if(serialLine.length()<90)serialLine+=c;continue;}
    if(serialLine.startsWith("TAP ")){int x,y;if(sscanf(serialLine.c_str(),"TAP %d %d",&x,&y)==2 && x>=0 && x<W && y>=0 && y<H)tap(x,y);}
    else if(serialLine=="STATUS") {
      Serial.printf("TASKPOP view=%d count=%d page=%d stored=%d draft=%s\n",view,count,page,storageOK,draft.c_str());
      for(int i=0;i<count;i++)Serial.printf("TASK %d %d %s\n",i,tasks[i].done,tasks[i].text);
    } else if(serialLine=="CONFIG") {
      Serial.printf("CONFIG version=1.2 lcd_rotation=%d touch_rotation=%d calibration=%d,%d,%d,%d\n",lcd.getRotation(),SCREEN_ROTATION,xmin,xmax,ymin,ymax);
    } else if(serialLine=="LEDSTATUS") {
      Serial.printf("LED event=%d seq=%lu elapsed=%lu duty=%u,%u,%u hw=%lu,%lu,%lu\n",(int)glow.event,(unsigned long)glow.eventCount,(unsigned long)(millis()-glow.started),ledDuty[0],ledDuty[1],ledDuty[2],(unsigned long)ledcRead(4),(unsigned long)ledcRead(5),(unsigned long)ledcRead(6));
    } else if(serialLine.startsWith("LEDTEST ")) {
      int event=serialLine.substring(8).toInt();
      if(event>LedFlow::IDLE && event<=LedFlow::PAGE)glow.trigger((LedFlow::Event)event,millis());
    } else if(serialLine=="SCREEN") {
      draw();Serial.println("RGB565 240 320");
      uint8_t row[W*2];
      for(int y=0;y<H;y++){for(int x=0;x<W;x++){uint16_t p=ui.readPixel(x,y);row[x*2]=p>>8;row[x*2+1]=p&255;}Serial.write(row,sizeof(row));}
      Serial.println("\nENDSCREEN");
    }
    serialLine="";
  }
}
void setup() {
  Serial.begin(115200);
  Serial.println("TASKPOP BOOT");
  pinMode(4,OUTPUT);digitalWrite(4,HIGH); // Mute unused audio amplifier.
  for(int pin: {5,27}){pinMode(pin,OUTPUT);digitalWrite(pin,HIGH);}
  store.begin("taskpop",false);loadTasks();
  Serial.println("TASKPOP STORAGE OK");
  lcd.init();lcd.setRotation(SCREEN_ROTATION);lcd.invertDisplay(false);
  Serial.println("TASKPOP LCD OK");
  ui.setColorDepth(4);
  if(!ui.createSprite(W,H)){Serial.println("TASKPOP SCREEN MEMORY ERROR");lcd.fillScreen(TFT_BLACK);lcd.setTextColor(TFT_WHITE);lcd.drawString("Screen memory error",8,40,2);while(true)delay(1000);}
  Serial.println("TASKPOP FRAMEBUFFER OK");
  ui.createPalette(palette,16);
  touchBus.begin(25,39,32);touch.begin(touchBus);touch.setRotation(SCREEN_ROTATION);
  Serial.println("TASKPOP TOUCH OK");
  beginGlow();draw();Serial.println("TASKPOP READY 1.2 USB BOTTOM");
}
void loop() {
  static bool held=false;static uint32_t releasedAt=0, started=0;static int rawX=0,rawY=0,samples=0;
  bool down=touch.touched();
  if(down) {
    TS_Point p=touch.getPoint();down=p.z>=100 && p.z<=4000;
    if(down){
      if(!held && millis()-releasedAt>65){held=true;started=millis();samples=0;rawX=rawY=0;}
      if(held && samples<12){rawX+=p.x;rawY+=p.y;samples++;}
    }
  }
  if(!down && held){
    held=false;releasedAt=millis();
    if(samples>=2 && releasedAt-started>=20){int rx=rawX/samples,ry=rawY/samples;
      if(view==CALIBRATE)calibrationPoint(rx,ry);
      else tap(constrain(map(rx,xmin,xmax,0,W-1),0,W-1),constrain(map(ry,ymin,ymax,0,H-1),0,H-1));
    }
  }
  if(undoUntil && (int32_t)(millis()-undoUntil)>=0){undoUntil=0;dirty=true;}
  if(noticeUntil && (int32_t)(millis()-noticeUntil)>=0){noticeUntil=0;dirty=true;}
  diagnostics();if(dirty)draw();updateGlow();delay(8);
}
