#pragma once
#include "Arduino.h"
#include <map>
#include <set>
inline std::map<std::string,std::shared_ptr<std::string>> fakeFiles;
inline std::set<std::string> fakeDirs;
inline size_t fakeOpens=0, fakeMaxWrite=0, fakeUsedCalls=0;
inline bool fakeRenameFail=false, fakeWriteFail=false;
class File {
 std::shared_ptr<std::string> data;
 size_t offset=0;
public:
 File()=default;
 explicit File(std::shared_ptr<std::string> value):data(value) {}
 explicit operator bool() const { return bool(data); }
 size_t size() const { return data ? data->size() : 0; }
 size_t read(uint8_t *p,size_t n) {
  n=std::min(n,data->size()-offset); memcpy(p,data->data()+offset,n); offset+=n; return n;
 }
 size_t write(const uint8_t *p,size_t n) {
  fakeMaxWrite=std::max(fakeMaxWrite,n);
  if(fakeWriteHook) fakeWriteHook();
  if(fakeWriteFail) return 0;
  data->append(reinterpret_cast<const char*>(p),n); return n;
 }
 void flush() {}
 void close() { data.reset(); }
};
struct FakeLittleFS {
 bool begin(bool,const char*,int,const char*) { return true; }
 void end() {}
 bool format() { fakeFiles.clear();return true; }
 bool exists(const String &p) { return fakeDirs.count(p)||fakeFiles.count(p); }
 bool mkdir(const String &p) { fakeDirs.insert(p);return true; }
 size_t totalBytes() const { return 8*1024*1024; }
 size_t usedBytes() const {
  ++fakeUsedCalls;size_t n=0;for(auto &f:fakeFiles)n+=f.second->size();return n;
 }
 File open(const String &p,const char *mode) {
  ++fakeOpens;
  if(mode[0]=='w') { fakeFiles[p]=std::make_shared<std::string>();return File(fakeFiles[p]); }
  auto i=fakeFiles.find(p);return i==fakeFiles.end()?File():File(i->second);
 }
 bool remove(const String &p) { return fakeFiles.erase(p); }
 bool rename(const String &a,const String &b) {
  if(fakeRenameFail||!fakeFiles.count(a))return false;
  fakeFiles[b]=fakeFiles[a];fakeFiles.erase(a);return true;
 }
};
inline FakeLittleFS LittleFS;
