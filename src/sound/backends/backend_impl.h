/*
Abstract:
  Backend helper interface definition

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

#ifndef __SOUND_BACKEND_IMPL_H_DEFINED__
#define __SOUND_BACKEND_IMPL_H_DEFINED__

#include <error.h>

#include <core/player.h>
#include <sound/mixer.h>
#include <sound/sound_params.h>

#include "../backend.h"

#include <boost/thread.hpp>

namespace ZXTune
{
  namespace Sound
  {
    class BackendImpl : public Backend, private Receiver
    {
    public:
      BackendImpl();
      virtual ~BackendImpl();

      virtual Error SetPlayer(Module::Player::Ptr player);
      virtual boost::weak_ptr<const Module::Player> GetPlayer() const;
      
      // playback control functions
      virtual Error Play();
      virtual Error Pause();
      virtual Error Stop();
      virtual Error SetPosition(unsigned frame);
      
      virtual Error GetCurrentState(State& state) const;

      virtual Error SetMixer(const std::vector<MultiGain>& data);
      virtual Error SetFilter(Converter::Ptr converter);
      
      virtual Error SetDriverParameters(const ParametersMap& params);
      virtual Error GetDriverParameters(ParametersMap& params) const;

      virtual Error SetRenderParameters(const RenderParameters& params);
      virtual Error GetRenderParameters(RenderParameters& params) const;
    protected:
      //internal usage functions. Should not call external interface funcs due to sync
      virtual void OnBufferReady(const void* data, std::size_t sizeInBytes) = 0;
      virtual void OnStartup() = 0;
      virtual void OnShutdown() = 0;
      virtual void OnPause() = 0;
      virtual void OnResume() = 0;
      virtual void OnParametersChanged(const ParametersMap& updates) = 0;
    private:
      void CheckState() const;
      void StopPlayback();
      bool SafeRenderFrame();
      void RenderFunc();
    protected:
      ParametersMap DriverParameters;
      RenderParameters RenderingParameters;
    private:
      //sync
      mutable boost::mutex PlayerMutex;
      boost::thread RenderThread;
      boost::barrier SyncBarrier;
      //state
      volatile State CurrentState;
      volatile bool InProcess;//STOP => STOPPING, STARTED => STARTING
      Error PlaybackError;
      //context
      unsigned Channels;
      boost::shared_ptr<Module::Player> Player;
      Converter::Ptr FilterObject;
      std::vector<Mixer::Ptr> MixersSet;
    };
  }
}


#endif //__SOUND_BACKEND_IMPL_H_DEFINED__
