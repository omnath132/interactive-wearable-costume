#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
class String {
 char* data;
 void assign(const char* s){data=(char*)malloc(strlen(s)+1);strcpy(data,s);}
public:
 String(){assign("");} String(const char* s){assign(s);} String(const String& s){assign(s.data);}
 String(int n){char b[40];snprintf(b,40,"%d",n);assign(b);}
 String(unsigned n){char b[40];snprintf(b,40,"%u",n);assign(b);}
 String(unsigned long n){char b[40];snprintf(b,40,"%lu",n);assign(b);}
 String(float n,unsigned precision){char b[80];snprintf(b,80,"%.*f",int(precision),double(n));assign(b);}
 ~String(){free(data);}
 String& operator=(const String& s){if(this!=&s){free(data);assign(s.data);}return *this;}
 const char* c_str()const{return data;}
 String& operator+=(const String& s){size_t a=strlen(data),b=strlen(s.data);data=(char*)realloc(data,a+b+1);memcpy(data+a,s.data,b+1);return *this;}
 String& operator+=(char c){char b[]={c,0};return *this+=String(b);}
 friend String operator+(const String& a,const String& b){String c(a);c+=b;return c;}
};
