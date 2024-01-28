#include "png.hpp"

#include <cstring>
#include <zlib.h>
#include <cmath>

#ifdef __linux__
  #include <arpa/inet.h>
#elif _WIN32
  #include <winsock2.h>
#endif

#define WINDOW_BITS 47

const std::vector<uint8_t> pngSignature = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, '\0'};

/* Function:    parsemI
   Description: Parses IHDR from png file
   Parameters:  ifstream - file descriptor for file we are reading
                uint32_t - The amount of bytes to read
                struct IHDR - IHDR struct to store values from parsed IHDR
   Returns:     None
 */
void Png::parseIHDR(const uint32_t cChunkLength)
{
  mPngFile.read((char *) &mIhdr, cChunkLength);
  /* There is no endian issues for 1 byte values which is why only width and height needs to be rearranged */
  mIhdr.width = htonl(mIhdr.width);
  mIhdr.height = htonl(mIhdr.height);

  if ((mIhdr.width == 0) || (mIhdr.height == 0))
  {
    std::cout << "Error! Invalid image dimensions." << std::endl;
    exit(-1);
  }
}

/* Function:    subPixelRGB
   Description: subtracts the RGB vals from two Pixels
   Parameters:  struct RGB - Pixel 1 RGB values
                struct RGB - Pixel 2 RGB values
   Returns:     struct RGB - New pixel RGB values
 */
struct Png::RGB Png::subPixelRGB(const struct RGB cPixel1, const struct RGB cPixel2)
{
  return {(uint8_t)((cPixel1.Red - cPixel2.Red)), (uint8_t)((cPixel1.Green - cPixel2.Green)),
          (uint8_t)((cPixel1.Blue - cPixel2.Blue)), 0};
}

/* Function:    addPixelRGB
   Description: Adds the RGB vals from two Pixels
   Parameters:  struct RGB - Pixel 1 RGB values
                struct RGB - Pixel 2 RGB values
   Returns:     struct RGB - New pixel RGB values
 */
struct Png::RGB Png::addPixelRGB(const struct RGB cPixel1, const struct RGB cPixel2)
{
  return {(uint8_t)((cPixel1.Red + cPixel2.Red)), (uint8_t)((cPixel1.Green + cPixel2.Green)),
          (uint8_t)((cPixel1.Blue + cPixel2.Blue)), 0};
}

/* Function:    calcPaethByte
   Description: Paeth Algorightm requires it to be run on each RGB byte
   Parameters:  uint8_t - A pixel RGB byte
                uint8_t - B pixel RGB byte
                uint8_t - C pixel RGB byte
   Returns:     uint8_t - Correct predictor pixel
 */
uint8_t Png::calcPaethByte(const uint8_t cA, const uint8_t cB, const uint8_t cC)
{
  int32_t p, pa, pb, pc;
  // int32_t type cast needed because values can be negative
  p = static_cast<int32_t>(cA) + static_cast<int32_t>(cB) - static_cast<int32_t>(cC);
  pa = abs(p - static_cast<int32_t>(cA));
  pb = abs(p - static_cast<int32_t>(cB));
  pc = abs(p - static_cast<int32_t>(cC)); 

  if ((pa <= pb) && (pa <= pc))
  {
    return cA;
  }
  else if (pb <= pc)
  {
    return cB;
  }
  else
  {
    return cC;
  }
}

/* Function:    paethRGBBitDepth8
   Description: Paeth Algorightm requires it to be run on each RGB byte
   Parameters:  std::vector<uint8_t> - Structure containing unfiltered img data
                uint32_t - Size of scanline row
                uint8_t  - Number of bytes per pixel
                uint32_t - Current index in mI vector
   Returns:     struct RGB - Unfiltered Pixel Color
 */
struct Png::RGB Png::paethRGBBitDepth8(const uint32_t cScanlineSize, const uint8_t cRgbSize, const uint32_t cCurrByte)
{
  /*
  Calculates the PAETH algorithm.
  The PAETH algorithm works on bytes, which means that one byte values will not work.
  For example you need to add two rgb values and compare that with another value.
  Assume 1 byte values 255 and 1. 255 + 1 overflows a uint8_t and results in 0.
  The above would mean that 255 + 1 <= 255 which is wrong in the paeth algorithm.
  
  To Overcome this I converted the rgbs into an integer as 0x0000RRRRGGGGBBBB
  This allows us to add integers without worrying about overflows.
*/
  struct RGB a, b, c, pr;
  const uint32_t ULPix = cCurrByte - (cScanlineSize + cRgbSize);
  const uint32_t leftBound = cCurrByte % cScanlineSize;
  const int32_t upperBound = cCurrByte - cScanlineSize;

  /* The below blocks gathers the pixel RGB to the left of the current index, the Pixel RGB to the upper left
     of the current index, and the Pixel RGB above the current index
  */
  a.Red   = (leftBound != 0) ? mImgData[cCurrByte - cRgbSize] : 0;
  a.Green = (leftBound != 0) ? mImgData[cCurrByte - (cRgbSize - 1)] : 0;
  a.Blue  = (leftBound != 0) ? mImgData[cCurrByte - (cRgbSize - 2)] : 0;

  if (mIhdr.colorType == RGBTRIPA)
    a.Alpha  = (leftBound != 0) ? mImgData[cCurrByte - (cRgbSize - 3)] : 0;

  b.Red   = (upperBound >= 0) ? mImgData[upperBound] : 0;
  b.Green = (upperBound >= 0) ? mImgData[upperBound + 1] : 0;
  b.Blue  = (upperBound >= 0) ? mImgData[upperBound + 2] : 0;

  if (mIhdr.colorType == RGBTRIPA)
    b.Alpha = (upperBound >= 0) ? mImgData[upperBound + 3] : 0;

  c.Red   = (leftBound != 0) && (upperBound >= 0) ? mImgData[ULPix] : 0;
  c.Green = (leftBound != 0) && (upperBound >= 0) ? mImgData[ULPix + 1] : 0;
  c.Blue  = (leftBound != 0) && (upperBound >= 0) ? mImgData[ULPix + 2] : 0;

  if (mIhdr.colorType == RGBTRIPA)
    c.Alpha = (leftBound != 0) && (upperBound >= 0) ? mImgData[ULPix + 3] : 0;

  pr.Red   = calcPaethByte(a.Red, b.Red, c.Red);
  pr.Green = calcPaethByte(a.Green, b.Green, c.Green);
  pr.Blue  = calcPaethByte(a.Blue, b.Blue, c.Blue);

  if (mIhdr.colorType == RGBTRIPA)
    pr.Alpha  = calcPaethByte(a.Alpha, b.Alpha, c.Alpha);
    
  return pr;
}

/* Function:    rgbBitDepth8
   Description: Iterates through buffer data applying the correct filter on decompressed IDAT chunk data
   Parameters:  std::vector<uint8_t> - Concatenated decompressed IDAT data
   Returns:     None
 */
void Png::rgbBitDepth8(const std::vector<uint8_t> &crRgbData)
{
  /*
    index i will always be either the filter method or a pixel starting on the Red color value_comp
    index i will never be Blue nor Green, to extract the blue and green for the current pixel do i + 1 for Green and
    i + 2 for red
  
    Here is an example below:
    We have a uint8_t array with 1 byte values represented as F (filter method), R (Red color value), G
    (Green Color Value), B (Blue Color Value) If our index i lands on F, we will add 1 to our index and that will put
    our index on R. Each time our index i is on R we will add 3 to our index i. We add 3 because each pixel contains an
    RGB value, so adding 3 to our index i essentially puts us on the next pixel
  
    F RGB RGB RGB RGB
    F RGB RGB RGB RGB
    F RGB RGB RGB RGB
    Filter UP adds the pixel RGB to the current pixel RGB 
  */
  RGB filterPix;
  filterPix.Red = 0;
  filterPix.Blue = 0;
  filterPix.Green = 0;
  filterPix.Alpha = 0;
  uint8_t filterValue = 0;
  uint8_t rgbSize = (mIhdr.colorType == RGBTRIP) ? RGBSIZE : RGBASIZE;
  uint32_t scanlineSize = (mIhdr.width * rgbSize);
  uint16_t mod = 256;
  uint32_t currByte = 0;
  uint32_t leftBound = 0;
  int32_t upperBound = 0;
  mImgData.reserve(mIhdr.width * mIhdr.height * rgbSize);

 for (int i = 0; i < (int) crRgbData.size();)
 {
    leftBound   = currByte % scanlineSize;
    upperBound  = currByte - scanlineSize;

    if (i % (scanlineSize + 1) == 0)
    {
      filterValue = crRgbData[i];
      i ++;
    }
    else
    {
      switch(filterValue)
      {
        case NONE:
          // No filter do nothing but set filter pixels as to avoid any junk memory
          filterPix.Red   = 0;
          filterPix.Green = 0;
          filterPix.Blue  = 0;
          filterPix.Alpha  = 0;
          break;
        case SUB:
          filterPix.Red   = (leftBound != 0) ? mImgData[currByte - rgbSize] : 0;
          filterPix.Green = (leftBound != 0) ? mImgData[currByte - (rgbSize - 1)] : 0;
          filterPix.Blue  = (leftBound != 0) ? mImgData[currByte - (rgbSize - 2)] : 0;

          if (mIhdr.colorType == RGBTRIPA)
            filterPix.Alpha = (leftBound != 0) ? mImgData[currByte - (rgbSize - 3)] : 0;
          break;
        case UP:
          filterPix.Red   = (upperBound >= 0) ? mImgData[upperBound] : 0;
          filterPix.Green = (upperBound >= 0) ? mImgData[upperBound + 1] : 0;
          filterPix.Blue  = (upperBound >= 0) ? mImgData[upperBound + 2] : 0;

          if (mIhdr.colorType == RGBTRIPA)
            filterPix.Alpha = (upperBound >= 0) ? mImgData[upperBound + 3] : 0;
          break;
        case AVERAGE:
          filterPix.Red   = (uint32_t) floor((((upperBound >= 0) ? mImgData[upperBound] : 0)     + ((leftBound != 0) ?
                            mImgData[currByte - rgbSize] : 0)) / 2) % mod;
          filterPix.Green = (uint32_t) floor((((upperBound >= 0) ? mImgData[upperBound + 1] : 0) + ((leftBound != 0) ?
                            mImgData[currByte - (rgbSize - 1)] : 0)) / 2) % mod;
          filterPix.Blue  = (uint32_t) floor((((upperBound >= 0) ? mImgData[upperBound + 2] : 0) + ((leftBound != 0) ?
                            mImgData[currByte - (rgbSize - 2)] : 0)) / 2) % mod;

          if (mIhdr.colorType == RGBTRIPA)
            filterPix.Alpha = (uint32_t) floor((((upperBound >= 0) ? mImgData[upperBound + 3] : 0) + ((leftBound != 0) ?
                              mImgData[currByte - (rgbSize - 3)] : 0)) / 2) % mod;
          break;
        case PAETH:
            filterPix = paethRGBBitDepth8(scanlineSize, rgbSize, currByte);
          break;
        default:
          break;
      }

      mImgData.push_back((crRgbData[i] + filterPix.Red) % mod);
      mImgData.push_back((crRgbData[i + 1] + filterPix.Green) % mod);
      mImgData.push_back((crRgbData[i + 2] + filterPix.Blue) % mod);

      if (mIhdr.colorType == RGBTRIPA)
        mImgData.push_back(crRgbData[i + 3] + filterPix.Alpha);
 
      i += rgbSize;
      currByte += rgbSize;
    }
  }
}

/* Function:    handlePngColorType
   Description: Handles different colortype PNG configurations
   Parameters:  std::vector<uint8_t> - Concatenated decompressed IDAT data
   Returns:     None
 */
void Png::handlePngColorType(const std::vector<uint8_t> &crRgbData)
{
  switch (mIhdr.colorType)
  {
    case 2:
    case 6:
    // fall through due to rgbBitDepth8 function being generic enough to capture
      if (mIhdr.bitDepth == 8)
        rgbBitDepth8(crRgbData);
      else
      {
          std::cout << "BitDepth not implemented" << std::endl;
          exit(-1);
      }
      break;
    default:
      std::cout << "ColorType not implemented" << std::endl;
      exit(-1);
  }
}

/* Function:    uncompressIDAT
   Description: Decompresses IDAT chunk data and fills a buffer with the data
   Parameters:  std::vector<uint8_t> - Concatenated uncompressed IDAT data
                std::vector<uint8_t> - Concatenated decompressed IDAT data
   Returns:     None
 */
void Png::uncompressIDAT(const std::vector<uint8_t> &crBuffer, std::vector<uint8_t> &rDecompressedData)
{
  // Decompression works, note that if png size IDAT chunk has more than 16384 bytes uncompressed, then while loop will keep running til empty
  // as well as subsequent IDAT chunks can occur, these two things are still not handled properly
  // When multiple IDAT chunks, you need to concatenate data all together then inflate
  std::vector<uint8_t> decompressedChunk(CHUNK_SIZE);
  int8_t error = 0;
  uint32_t numInflates = 1;

  //zlib struct needed for decompression (inflate is decompression, deflate is compression)

  z_stream infstream;
  infstream.zalloc = Z_NULL;
  infstream.zfree = Z_NULL;
  infstream.opaque = Z_NULL;
  infstream.avail_in = (uInt)crBuffer.size(); // size of input
  infstream.next_in = (Bytef *)crBuffer.data(); // input char array

  /* WINDOW_BITS = 47 since this tells zlib to automatically check if 
     gzip or zlib header exists in decompressed data
  */ 
  inflateInit2(&infstream, WINDOW_BITS);

  do
  {
    infstream.avail_out = (uInt)decompressedChunk.size(); // size of output
    infstream.next_out = (Bytef *)decompressedChunk.data(); // output char array
    error = inflate(&infstream, Z_NO_FLUSH);

    if(error == Z_DATA_ERROR || error == Z_MEM_ERROR)
    {
      std::cout << "ZLIB decompression returned error: " << error << std::endl;
      exit(-1);
    }

    // We only sub from decompressedChunk.end() when an inflate does not use up all 16384 bytes. Otherwise our decompressedData size will be wrong
    rDecompressedData.insert(rDecompressedData.end(), decompressedChunk.begin(), decompressedChunk.end() - abs((int)(infstream.total_out - (CHUNK_SIZE * numInflates))));
    numInflates ++;
  } while(infstream.avail_in != 0);

  inflateEnd(&infstream);
}

/* Function:    parseICCP
   Description: Parses ICCP chunk, but currently doesn't do that, just used to parse items
                that are not IHDR and IDAT from png
   Parameters:  uint32_t - Amount of bytes to read
   Returns:     None
 */
void Png::parseICCP(const uint32_t cChunkLength)
{
  std::vector<uint8_t> buffer(cChunkLength);

  mPngFile.read((char *) buffer.data(), cChunkLength);
}

/* Function:    readPng
   Description: Parses an entire png file and stores rgb values to recreate img
   Parameters:  None
   Returns:     None
 */
void Png::readPng() {
  std::vector<uint8_t> decompressedData;
  uint8_t validPngMask = 0;
  uint32_t chunkLength;
  uint32_t crc;
  std::vector<uint8_t> chunkType(5);
  std::vector<uint8_t> signature(pngSignature.size());
  std::vector<uint8_t> idatBuffer;

// TODO: Move check
  if(!mPngFile)
  {
    std::cout << "Png file descriptor is not good." << std::endl;
    exit(-1);
  }

  // Header is 8 bytes long, every png has this, use to confirm file format is png
  mPngFile.read((char *) signature.data(), sizeof(uint64_t));
  signature[8] = '\0';

  if(strcmp((char *)signature.data(), (char *)pngSignature.data()) != 0)
  {
    std::cout << "Not a PNG format" << std::endl;
    exit(-1);
  }

  while (strcmp((char *)chunkType.data(), "IEND\0") != 0 && mPngFile.good())
  {
    // Format for data is 4 bytes to give length of chunk buffer
    // 4 bytes for chunk type
    // X bytes of chunk data
    // Topped off with 4 bytes of crc
    // This below reads the IHDR which should follow right after header
    mPngFile.read((char*) &chunkLength, sizeof(uint32_t));
    chunkLength = htonl(chunkLength);
    mPngFile.read((char *) chunkType.data(), sizeof(uint32_t));
    chunkType[4] = '\0';

    // IDAT chain is broken so now uncompress all data collected
    if((((validPngMask & IDAT_CHAIN) >> 3) == 1) && (strcmp((char *) chunkType.data(), "IDAT\0") != 0))
    {
      validPngMask &= 0xF7;
      //temp
      uncompressIDAT(idatBuffer, decompressedData);
      handlePngColorType(decompressedData);
      idatBuffer.clear();
      decompressedData.clear();
    }

    if((validPngMask & IHDR_MASK) == 0 && strcmp((char *) chunkType.data(), "IHDR\0") != 0)
    {
      std::cout << "Error! Invalid PNG file, IHDR chunk did not follow PNG signature." << std::endl;
      exit(-1);
    } 
    else if ((validPngMask & IHDR_MASK) == 0 && strcmp((char *) chunkType.data(), "IHDR\0") == 0)
    {
      validPngMask |= IHDR_MASK;
      parseIHDR(chunkLength);
    }
    else if (strcmp((char *) chunkType.data(), "IDAT\0") == 0)
    {
      std::vector<uint8_t> tempBuffer(chunkLength);
      mPngFile.read((char *) tempBuffer.data(), chunkLength);
      idatBuffer.insert(idatBuffer.end(), tempBuffer.begin(), tempBuffer.end());

      if ((validPngMask & IDAT_MASK) == 0)
      {
        validPngMask |= IDAT_MASK;
      }

      // Start of the IDAT chain, start appending all data to buffer
      if ((validPngMask & IDAT_CHAIN) == 0)
      {
        validPngMask |= IDAT_CHAIN;
      }
    }
    else if (strcmp((char *) chunkType.data(), "iCCP\0") == 0)
    {
      if ((validPngMask & IDAT_MASK) != 0)
      {
        std::cout << "Error! iCCP chunk must appear before first IDAT chunk" << std::endl;
        exit(-1);
      }
       //Temp
       parseICCP(chunkLength);
    }
    else 
    {
      //Temp
      parseICCP(chunkLength);
    }

    mPngFile.read((char *) &crc, sizeof(uint32_t));
  }

  if (strcmp((char *) chunkType.data(), "IEND\0") == 0)
  {
    validPngMask |= IEND_MASK;
  }

}

/* Function:    Png
   Description: Constructs png object
   Parameters:  std::string - Filepath for png file to read from
   Returns:     None
 */
Png::Png(const std::string &crPngPath)
{
  mPngFile.open(crPngPath.c_str(), std::ios::binary);
  readPng();
}

/* Function:    compareSize
   Description: Compares if png size matches given arguments
   Parameters:  uint32_t - Width limit
                uint32_t Height limit
   Returns:     Status failure/success status of function
 */
Status Png::compareSize(const uint32_t cWidth, const uint32_t cHeight)
{
  if(mIhdr.width != cWidth || mIhdr.height != cHeight)
    return FAIL;

  return SUCCESS;
}

/* Function:    ~Png
   Description: Destroys png object
   Parameters:  None
   Returns:     None
 */
Png::~Png()
{
  mImgData.clear();
  mPngFile.close();
}

/* Function:    getImgData
   Description: Getter function for imgData
   Parameters:  None
   Returns:     std::vector<uint8_t> - Unfiltered image data
 */
std::vector<uint8_t> Png::getImgData()
{
  return mImgData;
}

/* Function:    getIhdr
   Description: Getter function for ihdr
   Parameters:  None
   Returns:     struct RGB - Metadata for png file that was read
 */
struct Png::IHDR Png::getIhdr()
{
  return mIhdr;
}

// Only for 8 bit depth for now
void Png::reverseImg()
{
  uint8_t rgbSize = (mIhdr.colorType == RGBTRIP) ? RGBSIZE : RGBASIZE;
  uint32_t scanlineSize = (mIhdr.width * rgbSize);
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  uint8_t a = 0;
  uint32_t earlyIndex = 0;
  uint32_t lateIndex = 0;
  uint32_t midPoint = mIhdr.height / 2;
  for (size_t i = mIhdr.height; i > midPoint ; i --)
  {
    for (size_t j = 0; j < scanlineSize; j+=4)
    {
      earlyIndex = scanlineSize * (mIhdr.height - i);
      lateIndex = scanlineSize * (i - 1);
      r = mImgData[earlyIndex + j];
      g = mImgData[earlyIndex + j + 1];
      b = mImgData[earlyIndex + j + 2];
      a = mImgData[earlyIndex + j + 3];
      mImgData[earlyIndex + j] = mImgData[lateIndex + j];
      mImgData[earlyIndex + j + 1] = mImgData[lateIndex + j + 1];
      mImgData[earlyIndex + j + 2] = mImgData[lateIndex + j + 2];
      mImgData[earlyIndex + j + 3] = mImgData[lateIndex + j + 3];
      mImgData[lateIndex + j] = r;
      mImgData[lateIndex + j + 1] = g;
      mImgData[lateIndex + j + 2] = b;
      mImgData[lateIndex + j + 3] = a;
    }
  }
}