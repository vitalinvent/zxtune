/**
*
* @file     detector.h
* @brief    Format detector interface
* @version  $Id$
* @author   (C) Vitamin/CAIG/2001
*
**/

#pragma once
#ifndef __DETECTOR_H_DEFINED__
#define __DETECTOR_H_DEFINED__

//common includes
#include <types.h>
//std includes
#include <string>

//Pattern format
//xx - match byte (hex)
//? - any byte
//+xx+ - skip xx bytes (dec)
bool DetectFormat(const uint8_t* data, std::size_t size, const std::string& pattern);

typedef std::vector<uint16_t> BinaryPattern;
bool DetectFormat(const uint8_t* data, std::size_t size, const BinaryPattern& pattern);

void CompileDetectPattern(const std::string& textPattern, BinaryPattern& binPattern);

class DetectFormatHelper
{
public:
  explicit DetectFormatHelper(const std::string& txtPattern)
  {
    CompileDetectPattern(txtPattern, Pattern);
  }

  bool operator ()(const void* data, std::size_t size) const
  {
    return DetectFormat(static_cast<const uint8_t*>(data), size, Pattern);
  }
private:
  BinaryPattern Pattern;
};

#endif //__DETECTOR_H_DEFINED__
