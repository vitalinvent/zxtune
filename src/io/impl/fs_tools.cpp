/*
Abstract:
  Different io-related tools implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//common includes
#include <error_tools.h>
#include <tools.h>
//library includes
#include <io/fs_tools.h>
#include <l10n/api.h>
//std includes
#include <cctype>

#define FILE_TAG 9AC2A0AC

namespace
{
  const L10n::TranslateFunctor translate = L10n::TranslateFunctor("io");

  const Char FS_DELIMITERS[] =
  {
  //windows-based systems supports two types of delimiters
    '/',
  #ifdef _WIN32
    '\\',
  #endif
     '\0'
  };

  inline bool IsNotFSSymbol(Char sym)
  {
    return std::iscntrl(sym) || 
      sym == '*' || sym == '\?' || sym == '%' || 
      sym == ':' || sym == '|' || sym == '\"' || sym == '<' || sym == '>' || 
      sym == '\\' || sym == '/';
  }
  
  inline bool IsFSSymbol(Char sym)
  {
    return !IsNotFSSymbol(sym);
  }

#ifdef _WIN32
  String ApplyOSFilenamesRestrictions(const String& in)
  {
    static const std::string DEPRECATED_NAMES[] =
    {
      "CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
      "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
    };
    const String::size_type dotPos = in.find('.');
    const String filename = in.substr(0, dotPos);
    if (ArrayEnd(DEPRECATED_NAMES) != std::find(DEPRECATED_NAMES, ArrayEnd(DEPRECATED_NAMES), ZXTune::IO::ConvertToFilename(filename)))
    {
      const String restPart = dotPos != String::npos ? in.substr(dotPos) : String();
      return filename + '~' + restPart;
    }
    return in;
  }
#else
  String ApplyOSFilenamesRestrictions(const String& in)
  {
    return in;
  }
#endif
}

namespace ZXTune
{
  namespace IO
  {
    std::string ConvertToFilename(const String& str)
    {
#ifdef UNICODE
      //TODO: use utf8 convertor if required
      return std::string(str.begin(), str.end());
#else
      return str;
#endif
    }
    
    String ExtractLastPathComponent(const String& path, String& restPart)
    {
      const String::size_type delimPos = path.find_last_of(FS_DELIMITERS);
      if (String::npos == delimPos)
      {
        restPart.clear();
        return path;
      }
      else
      {
        restPart = path.substr(0, delimPos);
        return path.substr(delimPos + 1);
      }
    }
    
    String AppendPath(const String& path1, const String& path2)
    {
      static const String DELIMS(FS_DELIMITERS);
      String result = path1;
      if (!path1.empty() && String::npos == DELIMS.find(*path1.rbegin()) &&
          !path2.empty() && String::npos == DELIMS.find(*path2.begin()))
      {
        result += FS_DELIMITERS[0];
      }
      result += path2;
      return result;
    }

    String MakePathFromString(const String& input, Char replacing)
    {
      String result;
      for (String::const_iterator it = input.begin(), lim = input.end(); it != lim; ++it)
      {
        if (IsFSSymbol(*it))
        {
          result += *it;
        }
        else if (replacing != '\0' && !result.empty())
        {
          result += replacing;
        }
      }
      const String res = replacing != '\0' ? result.substr(0, 1 + result.find_last_not_of(replacing)) : result;
      return ApplyOSFilenamesRestrictions(res);
    }
  }
}
