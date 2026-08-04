/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libcdr project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "CDRStylesCollector.h"

#include <cstring>
#include <zlib.h>

#include "CDRInternalStream.h"
#include "libcdr_utils.h"

#ifndef DUMP_IMAGE
#define DUMP_IMAGE 0
#endif

namespace
{

void writeU32BE(librevenge::RVNGBinaryData &out, unsigned value)
{
  out.append((unsigned char)((value >> 24) & 0xff));
  out.append((unsigned char)((value >> 16) & 0xff));
  out.append((unsigned char)((value >> 8) & 0xff));
  out.append((unsigned char)(value & 0xff));
}

void writePNGChunk(librevenge::RVNGBinaryData &out, const char type[4],
                   const unsigned char *data, unsigned length)
{
  writeU32BE(out, length);
  unsigned long crc = crc32(0L, Z_NULL, 0);
  crc = crc32(crc, (const Bytef *)type, 4);
  out.append((const unsigned char *)type, 4);
  if (length && data)
  {
    crc = crc32(crc, data, length);
    out.append(data, length);
  }
  writeU32BE(out, (unsigned)crc);
}

// Minimal RGBA PNG writer (zlib already linked by libcdr).
bool writePNG(librevenge::RVNGBinaryData &out, unsigned width, unsigned height,
              const std::vector<unsigned char> &rgba)
{
  if (!width || !height)
    return false;
  if (rgba.size() < (unsigned long)width * height * 4)
    return false;

  // Signature
  static const unsigned char sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
  out.append(sig, 8);

  // IHDR
  unsigned char ihdr[13];
  ihdr[0] = (unsigned char)((width >> 24) & 0xff);
  ihdr[1] = (unsigned char)((width >> 16) & 0xff);
  ihdr[2] = (unsigned char)((width >> 8) & 0xff);
  ihdr[3] = (unsigned char)(width & 0xff);
  ihdr[4] = (unsigned char)((height >> 24) & 0xff);
  ihdr[5] = (unsigned char)((height >> 16) & 0xff);
  ihdr[6] = (unsigned char)((height >> 8) & 0xff);
  ihdr[7] = (unsigned char)(height & 0xff);
  ihdr[8] = 8;  // bit depth
  ihdr[9] = 6;  // RGBA
  ihdr[10] = 0; // compression
  ihdr[11] = 0; // filter
  ihdr[12] = 0; // interlace
  writePNGChunk(out, "IHDR", ihdr, 13);

  // Raw image with filter byte 0 per row
  std::vector<unsigned char> raw;
  raw.resize((size_t)height * (1 + (size_t)width * 4));
  for (unsigned y = 0; y < height; ++y)
  {
    // PNG is top-down; CDR/BMP pixel buffers are bottom-up
    unsigned srcY = height - 1 - y;
    raw[(size_t)y * (1 + (size_t)width * 4)] = 0; // filter None
    memcpy(&raw[(size_t)y * (1 + (size_t)width * 4) + 1],
           &rgba[(size_t)srcY * width * 4],
           (size_t)width * 4);
  }

  uLongf bound = compressBound((uLong)raw.size());
  std::vector<unsigned char> compressed(bound);
  if (compress(compressed.data(), &bound, raw.data(), (uLong)raw.size()) != Z_OK)
    return false;
  compressed.resize(bound);
  writePNGChunk(out, "IDAT", compressed.data(), (unsigned)compressed.size());
  writePNGChunk(out, "IEND", nullptr, 0);
  return true;
}

} // anonymous namespace


libcdr::CDRStylesCollector::CDRStylesCollector(libcdr::CDRParserState &ps) :
  m_ps(ps), m_page(8.5, 11.0, -4.25, -5.5)
{
}

libcdr::CDRStylesCollector::~CDRStylesCollector()
{
}

void libcdr::CDRStylesCollector::collectBmp(unsigned imageId, unsigned colorModel, unsigned width, unsigned height, unsigned bpp, const std::vector<unsigned> &palette, const std::vector<unsigned char> &bitmap, const std::vector<unsigned char> &alpha)
{
  libcdr::CDRInternalStream stream(bitmap);
  librevenge::RVNGBinaryData image;

  if (height == 0)
    height = 1;

  auto tmpPixelSize = (unsigned)(height * width);
  if (tmpPixelSize < (unsigned)height) // overflow
    return;

  const bool hasAlpha = !alpha.empty() && alpha.size() >= tmpPixelSize;

  // Build BGRA pixels (BMP bottom-up order). Alpha defaults to opaque.
  std::vector<unsigned char> bgra;
  bgra.resize((size_t)tmpPixelSize * 4);

  // Cater for eventual padding
  unsigned long lineWidth = bitmap.size() / height;

  bool storeImage = true;

  for (unsigned j = 0; j < height; ++j)
  {
    unsigned i = 0;
    unsigned k = 0;
    if (colorModel == 6)
    {
      while (i <lineWidth && k < width)
      {
        unsigned l = 0;
        unsigned char c = bitmap[j*lineWidth+i];
        i++;
        while (k < width && l < 8)
        {
          unsigned rgb = (c & 0x80) ? 0xffffffu : 0u;
          unsigned idx = (j * width + k) * 4;
          bgra[idx] = (unsigned char)(rgb & 0xff);
          bgra[idx+1] = (unsigned char)((rgb >> 8) & 0xff);
          bgra[idx+2] = (unsigned char)((rgb >> 16) & 0xff);
          bgra[idx+3] = 0xff;
          c <<= 1;
          l++;
          k++;
        }
      }
    }
    else if (colorModel == 5)
    {
      while (i < lineWidth && k < width)
      {
        unsigned char c = bitmap[j*lineWidth+i];
        i++;
        unsigned rgb = m_ps.getBMPColor(libcdr::CDRColor((unsigned short)colorModel, c));
        unsigned idx = (j * width + k) * 4;
        bgra[idx] = (unsigned char)(rgb & 0xff);
        bgra[idx+1] = (unsigned char)((rgb >> 8) & 0xff);
        bgra[idx+2] = (unsigned char)((rgb >> 16) & 0xff);
        bgra[idx+3] = 0xff;
        k++;
      }
    }
    else if (!palette.empty())
    {
      while (i < lineWidth && k < width)
      {
        unsigned long c = bitmap[j*lineWidth+i];
        if (c >= palette.size())
          c = palette.size() - 1;
        i++;
        unsigned rgb = m_ps.getBMPColor(libcdr::CDRColor((unsigned short)colorModel, palette[c]));
        unsigned idx = (j * width + k) * 4;
        bgra[idx] = (unsigned char)(rgb & 0xff);
        bgra[idx+1] = (unsigned char)((rgb >> 8) & 0xff);
        bgra[idx+2] = (unsigned char)((rgb >> 16) & 0xff);
        bgra[idx+3] = 0xff;
        k++;
      }
    }
    else if (bpp == 24 && lineWidth >= 3)
    {
      while (i < lineWidth -2 && k < width)
      {
        unsigned c = ((unsigned)bitmap[j*lineWidth+i+2] << 16) | ((unsigned)bitmap[j*lineWidth+i+1] << 8) | ((unsigned)bitmap[j*lineWidth+i]);
        i += 3;
        unsigned rgb = m_ps.getBMPColor(libcdr::CDRColor((unsigned short)colorModel, c));
        unsigned idx = (j * width + k) * 4;
        bgra[idx] = (unsigned char)(rgb & 0xff);
        bgra[idx+1] = (unsigned char)((rgb >> 8) & 0xff);
        bgra[idx+2] = (unsigned char)((rgb >> 16) & 0xff);
        bgra[idx+3] = 0xff;
        k++;
      }
    }
    else if (bpp == 32 && lineWidth >= 4)
    {
      while (i < lineWidth - 3 && k < width)
      {
        unsigned char srcA = bitmap[j*lineWidth+i+3];
        unsigned c = ((unsigned)bitmap[j*lineWidth+i+2] << 16) | ((unsigned)bitmap[j*lineWidth+i+1] << 8) | ((unsigned)bitmap[j*lineWidth+i]);
        i += 4;
        unsigned rgb = m_ps.getBMPColor(libcdr::CDRColor((unsigned short)colorModel, c));
        unsigned idx = (j * width + k) * 4;
        bgra[idx] = (unsigned char)(rgb & 0xff);
        bgra[idx+1] = (unsigned char)((rgb >> 8) & 0xff);
        bgra[idx+2] = (unsigned char)((rgb >> 16) & 0xff);
        // Prefer separate alpha plane; else keep source alpha if present
        bgra[idx+3] = hasAlpha ? alpha[j * width + k] : srcA;
        k++;
      }
    }
    else
      storeImage = false;

    // Apply separate alpha plane for non-32bpp paths
    if (storeImage && hasAlpha && bpp != 32)
    {
      for (unsigned x = 0; x < width; ++x)
        bgra[(j * width + x) * 4 + 3] = alpha[j * width + x];
    }
  }

  if (!storeImage)
    return;

  if (hasAlpha)
  {
    // Inkscape (and most SVG hosts) ignore alpha in BI_RGB BMPs; emit PNG.
    std::vector<unsigned char> rgba(bgra.size());
    for (unsigned p = 0; p < tmpPixelSize; ++p)
    {
      rgba[p*4+0] = bgra[p*4+2]; // R
      rgba[p*4+1] = bgra[p*4+1]; // G
      rgba[p*4+2] = bgra[p*4+0]; // B
      rgba[p*4+3] = bgra[p*4+3]; // A
    }
    if (!writePNG(image, width, height, rgba))
      return;
  }
  else
  {
    unsigned tmpDIBImageSize = tmpPixelSize * 4;
    if (tmpPixelSize > tmpDIBImageSize) // overflow !!!
      return;

    unsigned tmpDIBOffsetBits = 14 + 40;
    unsigned tmpDIBFileSize = tmpDIBOffsetBits + tmpDIBImageSize;
    if (tmpDIBImageSize > tmpDIBFileSize) // overflow !!!
      return;

    // Create DIB file header
    writeU16(image, 0x4D42);  // Type
    writeU32(image, tmpDIBFileSize); // Size
    writeU16(image, 0); // Reserved1
    writeU16(image, 0); // Reserved2
    writeU32(image, tmpDIBOffsetBits); // OffsetBits

    // Create DIB Info header
    writeU32(image, 40); // Size

    writeU32(image, width);  // Width
    writeU32(image, height); // Height

    writeU16(image, 1); // Planes
    writeU16(image, 32); // BitCount
    writeU32(image, 0); // Compression
    writeU32(image, tmpDIBImageSize); // SizeImage
    writeU32(image, 0); // XPelsPerMeter
    writeU32(image, 0); // YPelsPerMeter
    writeU32(image, 0); // ColorsUsed
    writeU32(image, 0); // ColorsImportant

    // BGRA with opaque alpha (0xFF) so hosts that treat the 4th byte as alpha
    // do not make the whole image transparent/black.
    image.append(bgra.data(), bgra.size());
  }

#if DUMP_IMAGE
  librevenge::RVNGString filename;
  filename.sprintf("bitmap%.8x.%s", imageId, hasAlpha ? "png" : "bmp");
  FILE *f = fopen(filename.cstr(), "wb");
  if (f)
  {
    const unsigned char *tmpBuffer = image.getDataBuffer();
    for (unsigned long k = 0; k < image.size(); k++)
      fprintf(f, "%c",tmpBuffer[k]);
    fclose(f);
  }
#endif

  m_ps.m_bmps[imageId] = image;
}

void libcdr::CDRStylesCollector::collectBmp(unsigned imageId, const std::vector<unsigned char> &bitmap)
{
  librevenge::RVNGBinaryData image(&bitmap[0], bitmap.size());
#if DUMP_IMAGE
  librevenge::RVNGString filename;
  filename.sprintf("bitmap%.8x.bmp", imageId);
  FILE *f = fopen(filename.cstr(), "wb");
  if (f)
  {
    const unsigned char *tmpBuffer = image.getDataBuffer();
    for (unsigned long k = 0; k < image.size(); k++)
      fprintf(f, "%c",tmpBuffer[k]);
    fclose(f);
  }
#endif

  m_ps.m_bmps[imageId] = image;
}

void libcdr::CDRStylesCollector::collectPageSize(double width, double height, double offsetX, double offsetY)
{
  if (m_ps.m_pages.empty())
    m_page = CDRPage(width, height, offsetX, offsetY);
  else
    m_ps.m_pages.back() = CDRPage(width, height, offsetX, offsetY);
}

void libcdr::CDRStylesCollector::collectPage(unsigned /* level */)
{
  m_ps.m_pages.push_back(m_page);
}

void libcdr::CDRStylesCollector::collectBmpf(unsigned patternId, unsigned width, unsigned height, const std::vector<unsigned char> &pattern)
{
  m_ps.m_patterns[patternId] = CDRPattern(width, height, pattern);
}

void libcdr::CDRStylesCollector::collectColorProfile(const std::vector<unsigned char> &profile)
{
  if (!profile.empty())
    m_ps.setColorTransform(profile);
}

void libcdr::CDRStylesCollector::collectPaletteEntry(unsigned colorId, unsigned /* userId */, const libcdr::CDRColor &color)
{
  m_ps.m_documentPalette[colorId] = color;
}

void libcdr::CDRStylesCollector::collectText(unsigned textId, unsigned styleId, const std::vector<unsigned char> &data,
                                             const std::vector<unsigned char> &charDescriptions, const std::map<unsigned, CDRStyle> &styleOverrides)
{
  if (data.empty() && styleOverrides.empty())
    return;

  // Bit 0 of the per-char description marks a UTF-16 code unit (2 payload bytes).
  // Some X5+ files set that bit even when the payload is still 8-bit (numBytes ==
  // numChars). Treating those pairs as UTF-16LE produces CJK garbage (e.g.
  // "Register" -> "敒楧..."). Only honour the flag when the buffer is large
  // enough for 2 bytes per flagged character.
  unsigned utf16CharCount = 0;
  for (unsigned char d : charDescriptions)
  {
    if (d & 0x01)
      ++utf16CharCount;
  }
  const unsigned expectedUtf16Bytes =
    (unsigned)charDescriptions.size() + utf16CharCount; // 1 byte each + 1 extra per UTF-16
  const bool trustUnicodeFlag = data.size() >= expectedUtf16Bytes;
  if (!trustUnicodeFlag && utf16CharCount)
  {
    CDR_DEBUG_MSG(("CDRStylesCollector::collectText - ignoring UTF-16 flags "
                   "(%u flagged chars, %u bytes, need >= %u)\n",
                   utf16CharCount, (unsigned)data.size(), expectedUtf16Bytes));
  }

  unsigned char tmpCharDescription = 0;
  unsigned i = 0;
  unsigned j = 0;
  std::vector<unsigned char> tmpTextData;
  CDRStyle defaultCharStyle, tmpCharStyle;
  m_ps.getRecursedStyle(defaultCharStyle, styleId);

  CDRTextLine line;
  for (i=0, j=0; i<charDescriptions.size() && j<data.size(); ++i)
  {
    tmpCharStyle = defaultCharStyle;
    auto iter = styleOverrides.find(tmpCharDescription & 0xfe);
    if (iter != styleOverrides.end())
      tmpCharStyle.overrideStyle(iter->second);
    if (charDescriptions[i] != tmpCharDescription)
    {
      librevenge::RVNGString text;
      if (!tmpTextData.empty())
      {
        if (trustUnicodeFlag && (tmpCharDescription & 0x01))
          appendCharacters(text, tmpTextData);
        else
          appendCharacters(text, tmpTextData, tmpCharStyle.m_charSet);
      }
      line.append(CDRText(text, tmpCharStyle));
      tmpTextData.clear();
      tmpCharDescription = charDescriptions[i];

    }
    tmpTextData.push_back(data[j++]);
    if (trustUnicodeFlag && (tmpCharDescription & 0x01) && (j < data.size()))
      tmpTextData.push_back(data[j++]);
  }
  librevenge::RVNGString text;
  if (!tmpTextData.empty())
  {
    if (trustUnicodeFlag && (tmpCharDescription & 0x01))
      appendCharacters(text, tmpTextData);
    else
      appendCharacters(text, tmpTextData, tmpCharStyle.m_charSet);
  }
  line.append(CDRText(text, tmpCharStyle));
  CDR_DEBUG_MSG(("CDRStylesCollector::collectText - Text: %s\n", text.cstr()));

  std::vector<CDRTextLine> &paragraphVector = m_ps.m_texts[textId];
  paragraphVector.push_back(line);
}

void libcdr::CDRStylesCollector::collectStld(unsigned id, const CDRStyle &style)
{
  m_ps.m_styles[id] = style;
}

void libcdr::CDRStylesCollector::collectFillStyle(unsigned id, const CDRFillStyle &fillStyle)
{
  m_ps.m_fillStyles[id] = fillStyle;
}

void libcdr::CDRStylesCollector::collectLineStyle(unsigned id, const CDRLineStyle &lineStyle)
{
  m_ps.m_lineStyles[id] = lineStyle;
}
/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
