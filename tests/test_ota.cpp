#include "Ota.h"
#include "FirmwareIdentity.h"
#include <esp_app_format.h>
#include <cassert>
#include <iostream>
using namespace leap;
namespace leap {
RamAllocator jsonRam;
void *RamAllocator::allocate(size_t n){return malloc(n);}
void RamAllocator::deallocate(void *p){free(p);}
void *RamAllocator::reallocate(void *p,size_t n){return realloc(p,n);}
DeviceSettings deviceSettings;
static std::vector<uint8_t> image;
static size_t chunkSize=1;
bool Transport::download(const String &,size_t,const String &,const std::function<bool(const uint8_t*,size_t)> &sink){
  for(size_t i=0;i<image.size();i+=chunkSize)
    if(!sink(image.data()+i,std::min(chunkSize,image.size()-i))) return false;
  return true;
}
}
static std::string tag(const char *version){return std::string("LEAP_APP_VERSION_V1=")+version+";"+char(0);}
static void build(const std::string &identity){
  image.assign(sizeof(esp_image_header_t),0); image[0]=ESP_IMAGE_HEADER_MAGIC; image[12]=9;
  image.insert(image.end(),4096,0x55); image.insert(image.end(),identity.begin(),identity.end());
}
static JsonDocument release(){JsonDocument d;d["version"]="1.4.2";d["size"]=image.size();d["url"]="/api/v1/firmware/1/binary";d["sha256"]=std::string(64,'a');return d;}
int main(){
  auto identity=tag("1.4.2");
  for(size_t chunk=1;chunk<=128;++chunk){
    FirmwareIdentity parser; std::string data="noiseLEAP_APP_VERSION_V1="+std::string(1,char(0))+"noise"+identity+identity;
    for(size_t i=0;i<data.size();i+=chunk)parser.append(reinterpret_cast<const uint8_t*>(data.data()+i),std::min(chunk,data.size()-i));
    assert(parser.matches("1.4.2") && !parser.matches("1.4.1"));
  }
  FirmwareIdentity ambiguous;auto mixed=identity+tag("1.4.0-beta.2");
  ambiguous.append(reinterpret_cast<const uint8_t*>(mixed.data()),mixed.size());assert(!ambiguous.matches("1.4.2"));
  deviceSettings.otaEnabled=true; Transport net; Ota test; assert(test.begin());
  build(tag("1.4.0-beta.2"));auto d=release();
  assert(!test.offered(d.as<JsonObjectConst>(),"new-sync",net));
  assert(otaSelected==0 && otaEnded==0 && otaAborted==1 && test.pendingVersion().empty());
  build("unmarked");d=release();assert(!test.offered(d.as<JsonObjectConst>(),"new-sync",net));assert(otaSelected==0);
  build(identity);d=release();chunkSize=7;
  assert(test.offered(d.as<JsonObjectConst>(),"new-sync",net));
  assert(otaSelected==1 && otaEnded==1 && otaWritten==image && test.pendingVersion()=="1.4.2");
  // A wrong-version image pending validation must not be confirmed as healthy.
  otaState=ESP_OTA_IMG_PENDING_VERIFY; Ota wrong;wrong.begin(); assert(wrong.rolledBack);
  assert(!wrong.confirm(true) && otaReverted==1 && otaConfirmed==0);
  // An already valid fallback image remains usable and can report rollback.
  otaState=ESP_OTA_IMG_VALID; Ota fallback;fallback.begin();assert(fallback.rolledBack && fallback.confirm(true));assert(otaReverted==1);
  Preferences::values["leap-otaversion"]=FirmwareVersion;
  otaState=ESP_OTA_IMG_PENDING_VERIFY; Ota good;good.begin();assert(!good.rolledBack && good.confirm(true));assert(otaConfirmed==1);
  std::cout<<"PASS: streamed image identity, mislabeled/ambiguous binary refusal, unchanged boot slot, journal and pending-version confirmation\n";
}
