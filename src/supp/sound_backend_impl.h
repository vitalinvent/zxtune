#ifndef __SOUND_BACKEND_IMPL_H_DEFINED__
#define __SOUND_BACKEND_IMPL_H_DEFINED__

#include <error.h>

#include "sound_backend.h"

#include <boost/thread.hpp>

namespace ZXTune
{
  namespace Sound
  {
    class BackendImpl : public Backend
    {
    public:
      BackendImpl();
      virtual ~BackendImpl();

      virtual State SetPlayer(ModulePlayer::Ptr player);

      virtual State GetState() const;
      virtual State Play();
      virtual State Pause();
      virtual State Stop();

      /// Information getters
      virtual void GetPlayerInfo(ModulePlayer::Info& info) const;
      virtual void GetModuleInfo(Module::Information& info) const;
      virtual State GetModuleState(std::size_t& timeState, Module::Tracking& trackState) const;
      virtual State GetSoundState(Sound::Analyze::ChannelsState& state) const;

      /// Seeking
      virtual State SetPosition(const uint32_t& frame);
      virtual void GetSoundParameters(Parameters& params) const;
      virtual void SetSoundParameters(const Parameters& params);

    protected:
      //internal usage functions. Should not call external interface funcs due to sync
      virtual void OnBufferReady(const void* data, std::size_t sizeInBytes) = 0;
      //changed fields
      enum
      {
        DRIVER_PARAMS = 1,
        DRIVER_FLAGS = 2,
        SOUND_FREQ = 4,
        SOUND_CLOCK = 8,
        SOUND_FRAME = 16,
        SOUND_FLAGS = 32,
        MIXER = 64,
        FIR_ORDER = 128,
        FIR_LOW = 256,
        FIR_HIGH = 512,
        BUFFER = 1024
      };
      virtual void OnParametersChanged(unsigned changedFields) = 0;
      virtual void OnStartup() = 0;
      virtual void OnShutdown() = 0;
      virtual void OnPause() = 0;
      virtual void OnResume() = 0;
    protected:
      Parameters Params;
      ModulePlayer::Ptr Player;

    private:
      ModulePlayer::State SafeRenderFrame();
      void PlayFunc();
      void CheckState() const;
      void SafeStop();
      State UpdateCurrentState(ModulePlayer::State);
      unsigned MatchParameters(const Parameters& params) const;
      static void CallbackFunc(const void*, std::size_t, void*);
    private:
      boost::thread PlayerThread;
      mutable boost::mutex PlayerMutex;
      boost::barrier SyncBarrier;
    private:
      volatile State CurrentState;
      volatile bool InProcess;//STOP => STOPPING, STARTED => STARTING
      Error CurrentError;
      Convertor::Ptr Mixer;
      Convertor::Ptr Filter;
      std::vector<signed> FilterCoeffs;
      Receiver::Ptr Renderer;
    };
  }
}


#endif //__SOUND_BACKEND_IMPL_H_DEFINED__
