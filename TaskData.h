#pragma once
#include <stdint.h>
#include <string.h>
namespace TaskData {
constexpr int CAPACITY=30, TEXT_LIMIT=64;
constexpr uint32_t MAGIC=0x54504F50;
struct LegacyTask {char text[TEXT_LIMIT+1];bool done;};
struct LegacyStore {uint32_t magic;uint16_t version,size;LegacyTask rows[CAPACITY];};
struct Task {char text[TEXT_LIMIT+1];bool done;uint16_t sessions;uint32_t focusSeconds,id;};
struct Store {uint32_t magic;uint16_t version,size;uint32_t nextId;Task rows[CAPACITY];};
inline bool migrate(const LegacyStore& old,Store& data) {
  if(old.magic!=MAGIC || old.version!=1 || old.size>CAPACITY)return false;
  memset(&data,0,sizeof(data));data.magic=MAGIC;data.version=2;data.size=old.size;data.nextId=old.size+1;
  for(int i=0;i<old.size;i++) {
    memcpy(data.rows[i].text,old.rows[i].text,TEXT_LIMIT);
    data.rows[i].done=old.rows[i].done;data.rows[i].id=i+1;
  }
  return true;
}
}
