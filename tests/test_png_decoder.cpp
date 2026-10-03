#include <PNGdec.h>
#include "../LEAP/src/MediaColor.h"
#include <cassert>
#include <fstream>
#include <vector>
#include <cstdio>
static PNG decoder;
static int rows;
static bool pixelsOk;
static bool alphaFixture;
static uint16_t background = 0x18e7;
static int draw(PNGDRAW *row) {
 if (row->iWidth > 1024) return 0;
 uint16_t pixels[1024];
 decoder.getLineAsRGB565(row,pixels,PNG_RGB565_LITTLE_ENDIAN,leap::pngBackground(background));
 for(int x=0;x<row->iWidth;++x) {
  int y=row->y;
  int r=(x*13+y*7)%256,g=(x*3+y*17)%256,b=(x*11+y*5)%256;
  if (alphaFixture) {
   if(x==2 && y==0)r=g=b=0; // Opaque black must remain black.
   int a=(x%3)*127 + (x%3==2); // 0, 127, 255: transparent, edge, opaque.
   uint32_t bg=leap::pngBackground(background);
   if(a==0) {r=bg&255;g=(bg>>8)&255;b=(bg>>16)&255;}
   else if(a!=255) {r=(r*a+int(bg&255)*(255-a))>>8;g=(g*a+int((bg>>8)&255)*(255-a))>>8;b=(b*a+int((bg>>16)&255)*(255-a))>>8;}
  }
  uint16_t expected=(r>>3)<<11 | (g>>2)<<5 | (b>>3);
  if(pixels[x]!=expected)pixelsOk=false;
 }
 ++rows;
 return 1;
}
int main(int argc,char**argv) {
 if (argc < 2) return 2;
 bool ok=true;
 assert(leap::pngBackground(0xf800)==0x0000ff);
 assert(leap::pngBackground(0x001f)==0xff0000);
 assert(leap::pngBackground(0x07e0)==0x00ff00);
 assert(leap::pngBackground(0xffff)==0xffffff);
 for(int i=1;i<argc;++i) {
  std::ifstream in(argv[i],std::ios::binary);
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),{});
  if (bytes.empty()) return 2;
  rows=0;pixelsOk=true;
  alphaFixture=std::string(argv[i]).find("-alpha.png") != std::string::npos;
  int opened=decoder.openRAM(bytes.data(),bytes.size(),draw);
  int decoded=opened==PNG_SUCCESS?decoder.decode(nullptr,0):-1;
  bool reject=std::string(argv[i]).find("-reject.png") != std::string::npos;
  bool good=reject ? opened!=PNG_SUCCESS || decoded!=PNG_SUCCESS
                   : opened==PNG_SUCCESS && decoded==PNG_SUCCESS && pixelsOk && rows==decoder.getHeight();
  printf("%s: open=%d decode=%d rows=%d pixels=%d PASS=%d\n",argv[i],opened,decoded,rows,pixelsOk,good);
  ok=ok&&good;
 }
 return ok?0:1;
}
