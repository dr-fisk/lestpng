#ifndef PNG_HPP
#define PNG_HPP

#include <iostream>
#include <cstdint>
#include <vector>
#include <string>
#include <fstream>

#include "common.hpp"


#define IHDR_MASK  0x01
#define IDAT_MASK  0x02
#define IEND_MASK  0x04
#define IDAT_CHAIN 0x08
#define CHUNK_SIZE 16384

#define RGBSIZE 3
#define RGBASIZE 4

class Png {
public:
enum ColorType {
  GRAYSCALE  = 0,
  RGBTRIP  = 2,
  PLTE       = 3,
  GRAYSCALEA = 4,
  RGBTRIPA = 6
};

struct IHDR {
  uint32_t width;
  uint32_t height;
  uint8_t  bitDepth;
  uint8_t  colorType;
  uint8_t  compressionMethod;
  uint8_t  filterMethod;
  uint8_t  interfaceMethod;
};

struct RGB {
  uint8_t Red;
  uint8_t Green;
  uint8_t Blue;
  uint8_t Alpha;
};

enum FilterMethods {
  NONE, 
  SUB, 
  UP, 
  AVERAGE, 
  PAETH
};

Png(const std::string &crPngFile);
~Png();
std::vector<uint8_t> getImgData();
struct IHDR getIhdr();
Status compareSize(const uint32_t cWidth, const uint32_t cHeight);
void reverseImg();

private:
  void readPng();
  void parseIHDR(const uint32_t cChunkLength);
  struct RGB subPixelRGB(const struct RGB cPixel1, const struct RGB cPixel2);
  struct RGB addPixelRGB(const struct RGB cPixel1, const struct RGB cPixel2);
  struct RGB paethRGBBitDepth8(const uint32_t cScanlineSize, const uint8_t cRgbSize, const uint32_t cCurrByte);
  uint8_t calcPaethByte(const uint8_t cA, const uint8_t cB, const uint8_t cC);
  void rgbBitDepth8(const std::vector<uint8_t> &crRgbData);
  void handlePngColorType(const std::vector<uint8_t> &crRgbData);
  void uncompressIDAT(const std::vector<uint8_t> &crBuffer, std::vector<uint8_t> &rDecompressedData);
  void parseICCP(const uint32_t cChunkLength);
  
  struct IHDR mIhdr;
  std::vector<uint8_t> mImgData;
  std::ifstream mPngFile;
};

#endif