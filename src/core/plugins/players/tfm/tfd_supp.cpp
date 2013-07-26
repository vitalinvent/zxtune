/*
Abstract:
  TFD modules playback support

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "tfm_base.h"
#include "core/plugins/registrator.h"
#include "core/plugins/players/plugin.h"
#include "core/plugins/players/streaming.h"
//common includes
#include <tools.h>
//library includes
#include <core/plugin_attrs.h>
#include <formats/chiptune/decoders.h>
#include <formats/chiptune/fm/tfd.h>
//boost includes
#include <boost/make_shared.hpp>

namespace Module
{
namespace TFD
{
  typedef std::vector<Devices::TFM::DataChunk> ChunksArray;

  class ModuleData
  {
  public:
    typedef boost::shared_ptr<const ModuleData> Ptr;

    ModuleData(std::auto_ptr<ChunksArray> data, uint_t loop)
      : Data(data)
      , LoopPos(loop)
    {
    }

    uint_t Loop() const
    {
      return LoopPos;
    }

    uint_t Count() const
    {
      return static_cast<uint_t>(Data->size());
    }

    const Devices::TFM::DataChunk& Get(std::size_t frameNum) const
    {
      return Data->at(frameNum);
    }
  private:
    const std::auto_ptr<ChunksArray> Data;  
    uint_t LoopPos;
  };

  class DataBuilder : public Formats::Chiptune::TFD::Builder
  {
  public:
   explicit DataBuilder(PropertiesBuilder& props)
    : Properties(props)
    , Loop(0)
    , Chip(0)
   {
   }

   virtual void SetTitle(const String& title)
   {
     Properties.SetTitle(title);
   }

   virtual void SetAuthor(const String& author)
   {
     Properties.SetAuthor(author);
   }

   virtual void SetComment(const String& comment)
   {
     Properties.SetComment(comment);
   }

   virtual void BeginFrames(uint_t count)
   {
     Chip = 0;
     if (!Allocate(count))
     {
       Append(count);
     }
   }

   virtual void SelectChip(uint_t idx)
   {
     Chip = idx;
   }

   virtual void SetLoop()
   {
     if (Data.get())
     {
       Loop = static_cast<uint_t>(Data->size() - 1);
     }
   }

   virtual void SetRegister(uint_t idx, uint_t val)
   {
     if (Data.get() && !Data->empty())
     {
       Devices::FM::DataChunk::Registers& regs = Data->back().Data[Chip];
       regs.push_back(Devices::FM::DataChunk::Register(idx, val));
     }
   }

   ModuleData::Ptr GetResult() const
   {
     Allocate(0);
     return ModuleData::Ptr(new ModuleData(Data, Loop));
   }
  private:
    bool Allocate(std::size_t count) const
    {
      if (!Data.get())
      {
        Data.reset(new ChunksArray(count));
        return true;
      }
      return false;
    }

    void Append(std::size_t count)
    {
      std::fill_n(std::back_inserter(*Data), count, Devices::TFM::DataChunk());
    }
  private:
    PropertiesBuilder& Properties;
    uint_t Loop;
    mutable std::auto_ptr<ChunksArray> Data;
    uint_t Chip;
  };

  class TFDDataIterator : public TFM::DataIterator
  {
  public:
    TFDDataIterator(StateIterator::Ptr delegate, ModuleData::Ptr data)
      : Delegate(delegate)
      , State(Delegate->GetStateObserver())
      , Data(data)
    {
      UpdateCurrentState();
    }

    virtual void Reset()
    {
      CurrentChunk = Devices::TFM::DataChunk();
      Delegate->Reset();
      UpdateCurrentState();
    }

    virtual bool IsValid() const
    {
      return Delegate->IsValid();
    }

    virtual void NextFrame(bool looped)
    {
      Delegate->NextFrame(looped);
      UpdateCurrentState();
    }

    virtual TrackState::Ptr GetStateObserver() const
    {
      return State;
    }

    virtual Devices::TFM::DataChunk GetData() const
    {
      Devices::TFM::DataChunk res;
      std::swap(res, CurrentChunk);
      return res;
    }
  private:
    void UpdateCurrentState()
    {
      if (Delegate->IsValid())
      {
        const uint_t frameNum = State->Frame();
        const Devices::TFM::DataChunk& inChunk = Data->Get(frameNum);
        for (uint_t idx = 0; idx != Devices::TFM::CHIPS; ++idx)
        {
          UpdateChunk(CurrentChunk.Data[idx], inChunk.Data[idx]);
        }
      }
    }

    static void UpdateChunk(Devices::FM::DataChunk::Registers& dst, const Devices::FM::DataChunk::Registers& src)
    {
      std::copy(src.begin(), src.end(), std::back_inserter(dst));
    }
  private:
    const StateIterator::Ptr Delegate;
    const TrackState::Ptr State;
    const ModuleData::Ptr Data;
    mutable Devices::TFM::DataChunk CurrentChunk;
  };

  class TFDChiptune : public TFM::Chiptune
  {
  public:
    TFDChiptune(ModuleData::Ptr data, Parameters::Accessor::Ptr properties)
      : Data(data)
      , Properties(properties)
      , Info(CreateStreamInfo(Data->Count(), data->Loop()))
    {
    }

    virtual Information::Ptr GetInformation() const
    {
      return Info;
    }

    virtual Parameters::Accessor::Ptr GetProperties() const
    {
      return Properties;
    }

    virtual TFM::DataIterator::Ptr CreateDataIterator() const
    {
      const StateIterator::Ptr iter = CreateStreamStateIterator(Info);
      return boost::make_shared<TFDDataIterator>(iter, Data);
    }
  private:
    const ModuleData::Ptr Data;
    const Parameters::Accessor::Ptr Properties;
    const Information::Ptr Info;
  };

  class Factory : public Module::Factory
  {
  public:
    virtual Holder::Ptr CreateModule(PropertiesBuilder& propBuilder, const Binary::Container& rawData) const
    {
      DataBuilder dataBuilder(propBuilder);
      if (const Formats::Chiptune::Container::Ptr container = Formats::Chiptune::TFD::Parse(rawData, dataBuilder))
      {
        const ModuleData::Ptr data = dataBuilder.GetResult();
        if (data->Count())
        {
          propBuilder.SetSource(*container);
          const TFM::Chiptune::Ptr chiptune = boost::make_shared<TFDChiptune>(data, propBuilder.GetResult());
          return TFM::CreateHolder(chiptune);
        }
      }
      return Holder::Ptr();
    }
  };
}
}

namespace ZXTune
{
  void RegisterTFDSupport(PlayerPluginsRegistrator& registrator)
  {
    //plugin attributes
    const Char ID[] = {'T', 'F', 'D', 0};
    const uint_t CAPS = CAP_STOR_MODULE | CAP_DEV_FM | CAP_CONV_RAW;

    const Formats::Chiptune::Decoder::Ptr decoder = Formats::Chiptune::CreateTFDDecoder();
    const Module::Factory::Ptr factory = boost::make_shared<Module::TFD::Factory>();
    const PlayerPlugin::Ptr plugin = CreatePlayerPlugin(ID, CAPS, decoder, factory);
    registrator.RegisterPlugin(plugin);
  }
}
