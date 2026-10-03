#include <PNGdec.h>
#include <fstream>
#include <vector>
#include <cstdio>
static PNG decoder;
static int rows;
static bool pixelsOk;
static int draw(PNGDRAW *row) {
 if (row->iWidth > 1024) return 0;
 uint16_t pixels[1024];
 decoder.getLineAsRGB565(row,pixels,PNG_RGB565_LITTLE_ENDIAN,0);
 for(int x=0;x<row->iWidth;++x) {
  int y=row->y;
  uint16_t expected=(((x*13+y*7)%256)>>3)<<11 | (((x*3+y*17)%256)>>2)<<5 | (((x*11+y*5)%256)>>3);
  if(pixels[x]!=expected)pixelsOk=false;
 }
 ++rows;
 return 1;
}
int main(int argc,char**argv) {
 if (argc < 2) return 2;
 bool ok=true;
 for(int i=1;i<argc;++i) {
  std::ifstream in(argv[i],std::ios::binary);
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),{});
  if (bytes.empty()) return 2;
  rows=0;pixelsOk=true;
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
