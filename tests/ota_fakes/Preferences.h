#pragma once
#include <Arduino.h>
#define Preferences BasePreferences
#include "../game_fakes/Preferences.h"
#undef Preferences
class Preferences : public BasePreferences {
  std::string space;
public:
  inline static std::map<std::string,String> values;
  bool begin(const char *name,bool readOnly) {space=name;return BasePreferences::begin(name,readOnly);}
  String getString(const char *key,const char *fallback) {auto it=values.find(space+key);return it==values.end()?String(fallback):it->second;}
  size_t putString(const char *key,const String &value) {if(failWrites)return 0;values[space+key]=value;return value.length();}
  bool remove(const char *key) {return values.erase(space+key)>0;}
};
