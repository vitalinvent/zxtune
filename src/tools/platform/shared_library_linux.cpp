/*
Abstract:
  SharedLibrary implementation for Linux

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "shared_library_common.h"
//common includes
#include <contract.h>
#include <error_tools.h>
//boost includes
#include <boost/make_shared.hpp>
//platform includes
#include <dlfcn.h>
//text includes
#include <tools/text/tools.h>

#define FILE_TAG 4C0042B0

namespace
{
  const unsigned THIS_MODULE = Error::ModuleCode<'L', 'S', 'O'>::Value;

  class LinuxSharedLibrary : public SharedLibrary
  {
  public:
    explicit LinuxSharedLibrary(void* handle)
      : Handle(handle)
    {
      Require(Handle != 0);
    }

    virtual ~LinuxSharedLibrary()
    {
      if (Handle)
      {
        ::dlclose(Handle);
      }
    }

    virtual void* GetSymbol(const std::string& name) const
    {
      if (void* res = ::dlsym(Handle, name.c_str()))
      {
        return res;
      }
      throw MakeFormattedError(THIS_LINE, THIS_MODULE, Text::FAILED_FIND_DYNAMIC_LIBRARY_SYMBOL, FromStdString(name));
    }
  private:
    void* const Handle;
  };
  
  const std::string SUFFIX(".so");
  
  std::string BuildLibraryFilename(const std::string& name)
  {
    return "lib" + name + SUFFIX;
  }
}

Error LoadSharedLibrary(const std::string& fileName, SharedLibrary::Ptr& res)
{
  if (LibHandle handle = LibHandle(::dlopen(fileName.c_str(), RTLD_LAZY), std::ptr_fun(&CloseLibrary)))
  {
    res = boost::make_shared<LinuxSharedLibrary>(handle);
    return Error();
  }
  return MakeFormattedError(THIS_LINE, THIS_MODULE_CODE,
    Text::FAILED_LOAD_DYNAMIC_LIBRARY, FromStdString(fileName), FromStdString(::dlerror()));
}
  
std::string GetSharedLibraryFilename(const std::string& name)
{
  return name.find(SUFFIX) == name.npos
    ? BuildLibraryFilename(name)
    : name;
}

std::vector<std::string> GetSharedLibraryFilenames(const SharedLibrary::Name& name)
{
  std::vector<std::string> res;
  res.push_back(GetSharedLibraryFilename(name.Base()));
  const std::vector<std::string>& alternatives = name.LinuxAlternatives();
  std::transform(alternatives.begin(), alternatives.end(), std::back_inserter(res), std::ptr_fun(&GetSharedLibraryFilename));
  return res;
}
