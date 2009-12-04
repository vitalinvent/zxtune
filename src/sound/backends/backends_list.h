/*
Abstract:
  Sound backends list

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

#ifndef __SOUND_BACKENDS_LIST_H_DEFINED__
#define __SOUND_BACKENDS_LIST_H_DEFINED__

namespace ZXTune
{
  namespace Sound
  {
    class BackendsEnumerator;
    
    //forward declaration of supported backends
    
    inline void RegisterBackends(BackendsEnumerator& enumerator)
    {
    }
  }
}

#endif //__SOUND_BACKENDS_LIST_H_DEFINED__
