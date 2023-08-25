#include "jpegdec.h"
#include <cassert>

int main(int argc, char** argv) {
  assert(argc >= 2 && "no input file");
  char* filename = argv[1];
  JpegDecoder decoder(filename);

  decoder.decode();
}
