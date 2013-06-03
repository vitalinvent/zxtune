/*
Abstract:
  TurboSound functionality helpers

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

#pragma once
#ifndef __CORE_PLUGINS_PLAYERS_TS_BASE_H_DEFINED__
#define __CORE_PLUGINS_PLAYERS_TS_BASE_H_DEFINED__

//library includes
#include <core/module_holder.h>
#include <core/module_types.h>
#include <devices/aym/chip.h>
#include <devices/fm.h>

namespace ZXTune
{
  namespace Module
  {
    Renderer::Ptr CreateTSRenderer(Renderer::Ptr first, Renderer::Ptr second);
    Renderer::Ptr CreateTSRenderer(Renderer::Ptr first, Renderer::Ptr second, TrackState::Ptr state);
  }
}
#endif //__CORE_PLUGINS_PLAYERS_TS_BASE_H_DEFINED__
