#pragma once
#include "Config.h"
#include <string.h>
namespace costume {
struct Session { char token[33]={}; bool owner=false; uint32_t seen=0; };
class Access {
public:
  Session sessions[8]; bool open=false;
  int controller=-1; uint32_t renewed=0;
  void tick(uint32_t now){
    for(int i=0;i<8;i++)if(sessions[i].token[0] && uint32_t(now-sessions[i].seen)>=SESSION_MS){
      sessions[i].token[0]=0;if(controller==i)controller=-1;
    }
    if(controller>=0 && uint32_t(now-renewed)>=LEASE_MS)controller=-1;
  }
  int find(const char* token,uint32_t now){
    tick(now);if(!token || strlen(token)!=32)return -1;
    for(int i=0;i<8;i++)if(!strcmp(sessions[i].token,token)){sessions[i].seen=now;return i;}
    return -1;
  }
  int create(const char* token,bool owner,uint32_t now){
    tick(now);
    for(int i=0;i<8;i++)if(!sessions[i].token[0]){
      strncpy(sessions[i].token,token,32);sessions[i].token[32]=0;
      sessions[i].owner=owner;sessions[i].seen=now;return i;
    }
    return -1;
  }
  bool view(int id) const {return id>=0 && id<8 && sessions[id].token[0] && (open || sessions[id].owner);}
  bool owner(int id) const {return view(id) && sessions[id].owner;}
  bool claim(int id,uint32_t now){
    tick(now);if(!view(id))return false;
    if(controller<0 || controller==id || owner(id)){controller=id;renewed=now;return true;}return false;
  }
  bool command(int id,uint32_t now){tick(now);if(!view(id) || controller!=id)return false;renewed=now;return true;}
  bool setOpen(int id,bool value){
    if(!owner(id))return false;open=value;
    if(!open)for(int i=0;i<8;i++)if(!sessions[i].owner){sessions[i].token[0]=0;if(controller==i)controller=-1;}
    return true;
  }
  void logout(int id){if(id>=0 && id<8){sessions[id].token[0]=0;if(controller==id)controller=-1;}}
};
}
