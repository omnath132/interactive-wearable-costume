#pragma once
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
namespace costume {
struct LogEntry { uint32_t sequence=0, ms=0; char message[144]={}; };
// Fixed RAM use. Old entries roll off; no flash writes in the animation loop.
class Diagnostics {
  LogEntry entries[64]; unsigned head=0,count=0;uint32_t sequence=1;
public:
  uint32_t overwritten=0;
  void record(uint32_t now,const char* format,...) {
    LogEntry& e=entries[head];e.sequence=sequence++;e.ms=now;
    va_list args;va_start(args,format);vsnprintf(e.message,sizeof(e.message),format,args);va_end(args);
    head=(head+1)%64;if(count<64)count++;else overwritten++;
  }
  unsigned size()const{return count;}
  uint32_t next()const{return sequence;}
  const LogEntry& at(unsigned index)const{return entries[(head+64-count+index)%64];}
  const LogEntry* find(uint32_t seq)const {
    for(unsigned i=0;i<count;i++)if(at(i).sequence==seq)return &at(i);return nullptr;
  }
};
}
