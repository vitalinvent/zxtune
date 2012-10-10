/*
Abstract:
  Common SharedLibrary logic

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "shared_library_common.h"
//common includes
#include <error_tools.h>
#include <debug_log.h>
//library includes
#include <l10n/api.h>
//text includes
#include <tools/text/tools.h>

#define FILE_TAG 098808A4

namespace
{
  const Debug::Stream Dbg("Platform");

  const L10n::TranslateFunctor translate = L10n::TranslateFunctor("tools");
}

SharedLibrary::Ptr SharedLibrary::Load(const std::string& name)
{
  const std::string& fileName = GetSharedLibraryFilename(name);
  SharedLibrary::Ptr res;
  ThrowIfError(LoadSharedLibrary(fileName, res));
  Dbg("Loaded '%1%' (as '%2%')", name, fileName);
  return res;
}

SharedLibrary::Ptr SharedLibrary::Load(const SharedLibrary::Name& name)
{
  const std::vector<std::string> filenames = GetSharedLibraryFilenames(name);
  Error resError = MakeFormattedError(THIS_LINE,
    translate("Failed to load dynamic library '%1%' by any of the alternative names."), FromStdString(name.Base()));
  for (std::vector<std::string>::const_iterator it = filenames.begin(), lim = filenames.end(); it != lim; ++it)
  {
    SharedLibrary::Ptr res;
    if (const Error& err = LoadSharedLibrary(*it, res))
    {
      resError.AddSuberror(err);
    }
    else
    {
      Dbg("Loaded '%1%' (as '%2%')", name.Base(), *it);
      return res;
    }
  }
  throw resError;
  //workaround for MSVS7.1
  return SharedLibrary::Ptr();
}
