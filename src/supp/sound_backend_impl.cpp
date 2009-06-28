#include "sound_backend_impl.h"

#include <tools.h>

#include <player_attrs.h>

#include "../lib/sound/mixer.h"
#include "../lib/sound/filter.h"
#include "../lib/sound/renderer.h"

#include <cassert>

#include <text/errors.h>

#define FILE_TAG B3D60DB5

namespace
{
  const boost::posix_time::milliseconds PLAYTHREAD_SLEEP_PERIOD(1000);

  //playing thread and starting/stopping thread
  const std::size_t TOTAL_WORKING_THREADS = 2;

  using namespace ZXTune::Sound;
  void GetInitialParameters(Backend::Parameters& params)
  {
    //driver
    params.DriverParameters.clear();
    params.DriverFlags = 0;
    //sound
    params.SoundParameters.ClockFreq = 1750000;
    params.SoundParameters.SoundFreq = 44100;
    params.SoundParameters.FrameDurationMicrosec = 20000;
    params.SoundParameters.Flags = 0;
    //FIR (no)
    params.FIROrder = 0;
    params.LowCutoff = params.HighCutoff = 0;
    //render (one frame)
    params.BufferInMs = 500;
  }

  class Unlocker
  {
  public:
    Unlocker(boost::mutex& mtx) : Obj(mtx)
    {
      Obj.unlock();
    }
    ~Unlocker()
    {
      Obj.lock();
    }
  private:
    boost::mutex& Obj;
  };

  typedef boost::lock_guard<boost::mutex> Locker;
}

namespace ZXTune
{
  namespace Sound
  {
    BackendImpl::BackendImpl()
      : Params(), Player()
      , PlayerThread(), PlayerMutex(), SyncBarrier(TOTAL_WORKING_THREADS)
      , CurrentState(NOTOPENED), InProcess(false), CurrentError()
      , Mixer(), Filter(), FilterCoeffs(), Renderer()
    {
      GetInitialParameters(Params);
    }

    BackendImpl::~BackendImpl()
    {
      assert(NOTOPENED == CurrentState || (STOPPED == CurrentState && !InProcess)
        || !"Backend should be stopped!");
    }

    Backend::State BackendImpl::SetPlayer(ModulePlayer::Ptr player)
    {
      Locker lock(PlayerMutex);
      CurrentError = Error();
      SafeStop();
      Player = player;
      if (Player.get())
      {
        ModulePlayer::Info playInfo;
        Player->GetInfo(playInfo);
        if (playInfo.Capabilities & CAP_STOR_MULTITRACK)
        {
          throw Error(ERROR_DETAIL, 1, TEXT_ERROR_BACKEND_INVALID_PLAYER);//TODO: code
        }
        return CurrentState = STOPPED;
      }
      return CurrentState = NOTOPENED;
    }

    Backend::State BackendImpl::GetState() const
    {
      return CurrentState;
    }

    Backend::State BackendImpl::Play()
    {
      Locker lock(PlayerMutex);
      CheckState();
      const State prevState(CurrentState);
      if (STOPPED == prevState)
      {
        assert(Player.get());
        Player->Reset();
        PlayerThread = boost::thread(std::mem_fun(&BackendImpl::PlayFunc), this);
        SyncBarrier.wait();//wait until real start
        if (STARTED != CurrentState && CurrentError)
        {
          throw CurrentError;
        }
      }
      else if (PAUSED == prevState)
      {
        OnResume();
        CurrentState = STARTED;
      }
      return CurrentState;
    }

    Backend::State BackendImpl::Pause()
    {
      Locker lock(PlayerMutex);
      CheckState();
      OnPause();
      return CurrentState = PAUSED;
    }

    Backend::State BackendImpl::Stop()
    {
      Locker lock(PlayerMutex);
      CheckState();
      SafeStop();
      return CurrentState;
    }

    void BackendImpl::GetPlayerInfo(ModulePlayer::Info& info) const
    {
      Locker lock(PlayerMutex);
      CheckState();
      assert(Player.get());
      return Player->GetInfo(info);
    }

    void BackendImpl::GetModuleInfo(Module::Information& info) const
    {
      Locker lock(PlayerMutex);
      CheckState();
      assert(Player.get());
      return Player->GetModuleInfo(info);
    }

    Backend::State BackendImpl::GetModuleState(std::size_t& timeState, Module::Tracking& trackState) const
    {
      Locker lock(PlayerMutex);
      CheckState();
      assert(Player.get());
      Player->GetModuleState(timeState, trackState);
      return CurrentState;
    }

    Backend::State BackendImpl::GetSoundState(Sound::Analyze::ChannelsState& state) const
    {
      Locker lock(PlayerMutex);
      CheckState();
      assert(Player.get());
      Player->GetSoundState(state);
      return CurrentState;
    }

    /// Seeking
    Backend::State BackendImpl::SetPosition(const uint32_t& frame)
    {
      Locker lock(PlayerMutex);
      CheckState();
      if (STOPPED == CurrentState)
      {
        return CurrentState;
      }
      assert(Player.get());
      return UpdateCurrentState(Player->SetPosition(frame));
    }

    void BackendImpl::GetSoundParameters(Backend::Parameters& params) const
    {
      Locker lock(PlayerMutex);
      params = Params;
    }

    void BackendImpl::SetSoundParameters(const Backend::Parameters& params)
    {
      const unsigned UPDATE_RENDERER_MASK(BUFFER | SOUND_FRAME);
      const unsigned UPDATE_FILTER_MASK(FIR_ORDER | FIR_LOW | FIR_HIGH | SOUND_FREQ);
      const unsigned UPDATE_MIXER_MASK(MIXER);

      if (Params.Mixer && params.Mixer->InMatrix.size() != Params.Mixer->InMatrix.size())
      {
        throw Error(ERROR_DETAIL, 1, TEXT_ERROR_BACKEND_INVALID_MIXER);//TODO: code
      }

      Locker lock(PlayerMutex);
      const unsigned changedFields(MatchParameters(params));
      Params = params;
      if (changedFields & UPDATE_RENDERER_MASK)
      {
        Renderer = CreateCallbackRenderer(Params.BufferInMultisamples(), CallbackFunc, this);
      }
      if ((changedFields & UPDATE_FILTER_MASK) && Params.FIROrder)
      {
        if (changedFields & UPDATE_FILTER_MASK)
        {
          FilterCoeffs.resize(Params.FIROrder);
          CalculateFIRCoefficients(Params.SoundParameters.SoundFreq, Params.LowCutoff, Params.HighCutoff, FilterCoeffs);
        }

        Filter = CreateFIRFilter(FilterCoeffs);
      }

      if (Filter.get())
      {
        Filter->SetEndpoint(Renderer);
      }
      if ((changedFields & UPDATE_MIXER_MASK) && Params.Mixer)
      {
        Mixer = CreateMixer(Params.Mixer);
      }
      if (Mixer.get())
      {
        Mixer->SetEndpoint(Filter.get() ? Filter : Renderer);
      }
      return OnParametersChanged(changedFields);
    }

    void BackendImpl::CheckState() const
    {
      if (CurrentError)
      {
        throw CurrentError;
      }
      if (NOTOPENED == CurrentState && !Player.get())
      {
        throw Error(ERROR_DETAIL, 1, TEXT_ERROR_BACKEND_INVALID_STATE);//TODO: code
      }
    }

    void BackendImpl::SafeStop()
    {
      if (STARTED == CurrentState || PAUSED == CurrentState || (STOPPED == CurrentState && InProcess))
      {
        if (PAUSED == CurrentState)
        {
          OnResume();
        }
        CurrentState = STOPPED;
        InProcess = true;//stopping now
        {
          Unlocker unlock(PlayerMutex);//TODO: use condvariable
          SyncBarrier.wait();//wait for thread stop
          PlayerThread.join();//cleanup thread
        }
      }
    }

    Backend::State BackendImpl::UpdateCurrentState(ModulePlayer::State state)
    {
      //do not change state if playing or pause if underlaying player is not stopped
      return CurrentState = ModulePlayer::MODULE_STOPPED == state ? STOPPED : CurrentState;
    }

    unsigned BackendImpl::MatchParameters(const Backend::Parameters& after) const
    {
      unsigned res(0);
      if (Params.DriverParameters != after.DriverParameters)
      {
        res |= DRIVER_PARAMS;
      }
      if (Params.DriverFlags != after.DriverFlags)
      {
        res |= DRIVER_FLAGS;
      }
      if (Params.SoundParameters.SoundFreq != after.SoundParameters.SoundFreq)
      {
        res |= SOUND_FREQ;
      }
      if (Params.SoundParameters.ClockFreq != after.SoundParameters.ClockFreq)
      {
        res |= SOUND_CLOCK;
      }
      if (Params.SoundParameters.FrameDurationMicrosec != after.SoundParameters.FrameDurationMicrosec)
      {
        res |= SOUND_FRAME;
      }
      if (Params.SoundParameters.Flags != after.SoundParameters.Flags)
      {
        res |= SOUND_FLAGS;
      }
      if (Params.Mixer != after.Mixer)
      {
        res |= MIXER;
      }
      if (Params.FIROrder != after.FIROrder)
      {
        res |= FIR_ORDER;
      }
      if (Params.LowCutoff != after.LowCutoff)
      {
        res |= FIR_LOW;
      }
      if (Params.HighCutoff != after.HighCutoff)
      {
        res |= FIR_HIGH;
      }
      if (Params.BufferInMs != after.BufferInMs)
      {
        res |= BUFFER;
      }
      return res;
    }

    ModulePlayer::State BackendImpl::SafeRenderFrame()
    {
      Locker lock(PlayerMutex);
      if (Convertor::Ptr mixer = Mixer)
      {
        return Player->RenderFrame(Params.SoundParameters, *mixer);
      }
      else
      {
        return ModulePlayer::MODULE_PLAYING;//null playback
      }
    }

    void BackendImpl::PlayFunc()
    {
      try
      {
        CurrentState = STARTED;
        InProcess = true;//starting begin
        OnStartup();//throw
        SyncBarrier.wait();
        InProcess = false;//starting finished
        while (STOPPED != CurrentState)
        {
          if (STARTED == CurrentState)
          {
            if (ModulePlayer::MODULE_STOPPED == SafeRenderFrame())//throw
            {
              CurrentState = STOPPED;
              InProcess = true; //stopping begin
              break;
            }
          }
          else if (PAUSED == CurrentState)
          {
            boost::this_thread::sleep(PLAYTHREAD_SLEEP_PERIOD);
          }
        }
        OnShutdown();//throw
        SyncBarrier.wait();
        InProcess = false; //stopping finished
      }
      catch (const Error& e)
      {
        CurrentError = e;
        CurrentState = STOPPED;
        SyncBarrier.wait();
        InProcess = false;
      }
    }

    void BackendImpl::CallbackFunc(const void* data, std::size_t size, void* obj)
    {
      assert(obj);
      BackendImpl* const self(static_cast<BackendImpl*>(obj));
      Unlocker ulock(self->PlayerMutex);
      return self->OnBufferReady(data, size);
    }
  }
}
