/*
Abstract:
  PT3 modules playback support

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "ay_base.h"
#include "ay_conversion.h"
#include "ts_base.h"
#include "core/plugins/utils.h"
#include "core/plugins/registrator.h"
#include "core/plugins/archives/archive_supp_common.h"
#include "core/plugins/players/creation_result.h"
#include "core/plugins/players/module_properties.h"
//common includes
#include <byteorder.h>
#include <error_tools.h>
#include <range_checker.h>
#include <tools.h>
//library includes
#include <core/convert_parameters.h>
#include <core/core_parameters.h>
#include <core/module_attrs.h>
#include <core/plugin_attrs.h>
#include <formats/chiptune_decoders.h>
#include <formats/packed_decoders.h>
#include <formats/chiptune/protracker3.h>
//text includes
#include <core/text/plugins.h>

#define FILE_TAG 3CBC0BBC

namespace ProTracker3
{
  typedef ZXTune::Module::Vortex::Track Track;

  std::auto_ptr<Formats::Chiptune::ProTracker3::Builder> CreateDataBuilder(Track::ModuleData::RWPtr data, ZXTune::Module::ModuleProperties::RWPtr props, uint_t& patOffset);
}

namespace ProTracker3
{
  using namespace ZXTune;
  using namespace ZXTune::Module;

  void ConvertSample(const Formats::Chiptune::ProTracker3::Sample& src, Track::Sample& dst)
  {
    dst = Track::Sample(src.Loop, src.Lines.begin(), src.Lines.end());
  }

  void ConvertOrnament(const Formats::Chiptune::ProTracker3::Ornament& src, Track::Ornament& dst)
  {
    dst = Track::Ornament(src.Loop, src.Lines.begin(), src.Lines.end());
  }

  class DataBuilder : public Formats::Chiptune::ProTracker3::Builder
  {
  public:
    DataBuilder(Track::ModuleData::RWPtr data, ZXTune::Module::ModuleProperties::RWPtr props, uint_t& patOffset)
      : Data(data)
      , Properties(props)
      , PatOffset(patOffset)
      , Context(*Data)
    {
    }

    virtual void SetProgram(const String& program)
    {
      Properties->SetProgram(OptimizeString(program));
    }

    virtual void SetTitle(const String& title)
    {
      Properties->SetTitle(OptimizeString(title));
    }

    virtual void SetAuthor(const String& author)
    {
      Properties->SetAuthor(OptimizeString(author));
    }

    virtual void SetVersion(uint_t version)
    {
      Properties->SetVersion(3, version);
    }

    virtual void SetNoteTable(Formats::Chiptune::ProTracker3::NoteTable table)
    {
      const String freqTable = Vortex::GetFreqTable(static_cast<Vortex::NoteTable>(table), Vortex::ExtractVersion(*Properties));
      Properties->SetFreqtable(freqTable);
    }

    virtual void SetMode(uint_t mode)
    {
      PatOffset = mode;
    }

    virtual void SetInitialTempo(uint_t tempo)
    {
      Data->InitialTempo = tempo;
    }

    virtual void SetSample(uint_t index, const Formats::Chiptune::ProTracker3::Sample& sample)
    {
      //TODO: use common types
      Data->Samples.resize(index + 1);
      ConvertSample(sample, Data->Samples[index]);
    }

    virtual void SetOrnament(uint_t index, const Formats::Chiptune::ProTracker3::Ornament& ornament)
    {
      Data->Ornaments.resize(index + 1);
      ConvertOrnament(ornament, Data->Ornaments[index]);
    }

    virtual void SetPositions(const std::vector<uint_t>& positions, uint_t loop)
    {
      Data->Positions.assign(positions.begin(), positions.end());
      Data->LoopPosition = loop;
    }

    virtual void StartPattern(uint_t index)
    {
      Context.SetPattern(index);
    }

    virtual void FinishPattern(uint_t size)
    {
      Context.FinishPattern(size);
    }

    virtual void StartLine(uint_t index)
    {
      Context.SetLine(index);
    }

    virtual void SetTempo(uint_t tempo)
    {
      Context.CurLine->SetTempo(tempo);
    }

    virtual void StartChannel(uint_t index)
    {
      Context.SetChannel(index);
    }

    virtual void SetRest()
    {
      Context.CurChannel->SetEnabled(false);
    }

    virtual void SetNote(uint_t note)
    {
      Track::Line::Chan* const channel = Context.CurChannel;
      channel->SetEnabled(true);
      const Track::CommandsArray::iterator cmd = std::find(channel->Commands.begin(), channel->Commands.end(), Vortex::GLISS_NOTE);
      if (cmd != channel->Commands.end())
      {
        cmd->Param3 = int_t(note);
      }
      else
      {
        channel->SetNote(note);
      }
    }

    virtual void SetSample(uint_t sample)
    {
      Context.CurChannel->SetSample(sample);
    }

    virtual void SetOrnament(uint_t ornament)
    {
      Context.CurChannel->SetOrnament(ornament);
    }

    virtual void SetVolume(uint_t vol)
    {
      Context.CurChannel->SetVolume(vol);
    }

    virtual void SetGlissade(uint_t period, int_t val)
    {
      Context.CurChannel->Commands.push_back(Track::Command(Vortex::GLISS, period, val));
    }

    virtual void SetNoteGliss(uint_t period, int_t val, uint_t /*limit*/)
    {
      //ignore limit
      Context.CurChannel->Commands.push_back(Track::Command(Vortex::GLISS_NOTE, period, val));
    }

    virtual void SetSampleOffset(uint_t offset)
    {
      Context.CurChannel->Commands.push_back(Track::Command(Vortex::SAMPLEOFFSET, offset));
    }

    virtual void SetOrnamentOffset(uint_t offset)
    {
      Context.CurChannel->Commands.push_back(Track::Command(Vortex::ORNAMENTOFFSET, offset));
    }

    virtual void SetVibrate(uint_t ontime, uint_t offtime)
    {
      Context.CurChannel->Commands.push_back(Track::Command(Vortex::VIBRATE, ontime, offtime));
    }

    virtual void SetEnvelopeSlide(uint_t period, int_t val)
    {
      Context.CurChannel->Commands.push_back(Track::Command(Vortex::SLIDEENV, period, val));
    }

    virtual void SetEnvelope(uint_t type, uint_t value)
    {
      Context.CurChannel->Commands.push_back(Track::Command(Vortex::ENVELOPE, type, value));
    }

    virtual void SetNoEnvelope()
    {
      Context.CurChannel->Commands.push_back(Track::Command(Vortex::NOENVELOPE));
    }

    virtual void SetNoiseBase(uint_t val)
    {
      Context.CurChannel->Commands.push_back(Track::Command(Vortex::NOISEBASE, val));
    }
  private:
    struct BuildContext
    {
      Track::ModuleData& Data;
      Track::Pattern* CurPattern;
      Track::Line* CurLine;
      Track::Line::Chan* CurChannel;

      explicit BuildContext(Track::ModuleData& data)
        : Data(data)
        , CurPattern()
        , CurLine()
        , CurChannel()
      {
      }

      void SetPattern(uint_t idx)
      {
        Data.Patterns.resize(std::max<std::size_t>(idx + 1, Data.Patterns.size()));
        CurPattern = &Data.Patterns[idx];
        CurLine = 0;
        CurChannel = 0;
      }

      void SetLine(uint_t idx)
      {
        if (const std::size_t skipped = idx - CurPattern->GetSize())
        {
          CurPattern->AddLines(skipped);
        }
        CurLine = &CurPattern->AddLine();
        CurChannel = 0;
      }

      void SetChannel(uint_t idx)
      {
        CurChannel = &CurLine->Channels[idx];
      }

      void FinishPattern(uint_t size)
      {
        if (const std::size_t skipped = size - CurPattern->GetSize())
        {
          CurPattern->AddLines(skipped);
        }
        CurLine = 0;
        CurPattern = 0;
      }
    };
  private:
    const Track::ModuleData::RWPtr Data;
    const ModuleProperties::RWPtr Properties;
    uint_t& PatOffset;

    BuildContext Context;
  };
}

namespace ProTracker3
{
  std::auto_ptr<Formats::Chiptune::ProTracker3::Builder> CreateDataBuilder(Track::ModuleData::RWPtr data, ZXTune::Module::ModuleProperties::RWPtr props, uint_t& patOffset)
  {
    return std::auto_ptr<Formats::Chiptune::ProTracker3::Builder>(new DataBuilder(data, props, patOffset));
  }
}

namespace PT3
{
  using namespace ZXTune;
  using namespace ZXTune::Module;

  class TSModuleData : public Vortex::Track::ModuleData
  {
  public:
    TSModuleData(uint_t base, Vortex::Track::ModuleData::Ptr delegate)
      : Vortex::Track::ModuleData(*delegate)
      , Base(base)
    {
    }

    virtual uint_t GetPatternSize(uint_t position) const
    {
      const uint_t size1 = Vortex::Track::ModuleData::GetPatternSize(position);
      const uint_t size2 = GetSecondPatternByPosition(position).GetSize();
      return std::min(size1, size2);
    }

    virtual uint_t GetNewTempo(uint_t position, uint_t line) const
    {
      if (uint_t originalTempo = Vortex::Track::ModuleData::GetNewTempo(position, line))
      {
        return originalTempo;
      }
      if (const Vortex::Track::Line* lineObj = GetSecondPatternByPosition(position).GetLine(line))
      {
        if (const boost::optional<uint_t>& tempo = lineObj->Tempo)
        {
          return *tempo;
        }
      }
      return 0;
    }
  private:
    const Vortex::Track::Pattern& GetSecondPatternByPosition(uint_t position) const
    {
      const uint_t originalPattern = Vortex::Track::ModuleData::GetPatternIndex(position);
      return Patterns[Base - 1 - originalPattern];
    }
  private:
    const uint_t Base;
  };

  class MirroredModuleData : public Vortex::Track::ModuleData
  {
  public:
    MirroredModuleData(uint_t base, const Vortex::Track::ModuleData& data)
      : Vortex::Track::ModuleData(data)
      , Base(base)
    {
    }

    virtual uint_t GetPatternIndex(uint_t position) const
    {
      return Base - 1 - Vortex::Track::ModuleData::GetPatternIndex(position);
    }
  private:
    const uint_t Base;
  };

  class TSHolder : public Holder
  {
  public:
    TSHolder(Vortex::Track::ModuleData::Ptr data, Holder::Ptr delegate, uint_t patOffset)
      : Data(data)
      , Delegate(delegate)
      , PatOffset(patOffset)
    {
    }

    virtual Plugin::Ptr GetPlugin() const
    {
      return Delegate->GetPlugin();
    } 

    virtual Information::Ptr GetModuleInformation() const
    {
      return Delegate->GetModuleInformation();
    }

    virtual Parameters::Accessor::Ptr GetModuleProperties() const
    {
      return Delegate->GetModuleProperties();
    }

    virtual Renderer::Ptr CreateRenderer(Parameters::Accessor::Ptr params, Sound::MultichannelReceiver::Ptr target) const
    {
      const Devices::AYM::Receiver::Ptr receiver = AYM::CreateReceiver(target);
      const AYMTSMixer::Ptr mixer = CreateTSMixer(receiver);
      const Devices::AYM::ChipParameters::Ptr chipParams = AYM::CreateChipParameters(params);
      const Devices::AYM::Chip::Ptr chip1 = Devices::AYM::CreateChip(chipParams, mixer);
      const Devices::AYM::Chip::Ptr chip2 = Devices::AYM::CreateChip(chipParams, mixer);

      const Information::Ptr info = GetModuleInformation();
      const uint_t version = Vortex::ExtractVersion(*Delegate->GetModuleProperties());
      const Renderer::Ptr renderer1 = Vortex::CreateRenderer(params, info, Data, version, chip1);
      const Renderer::Ptr renderer2 = Vortex::CreateRenderer(params, info, boost::make_shared<MirroredModuleData>(PatOffset, *Data), version, chip2);
      return CreateTSRenderer(renderer1, renderer2, mixer);
    }

    virtual Error Convert(const Conversion::Parameter& spec, Parameters::Accessor::Ptr params, Dump& dst) const
    {
      using namespace Conversion;
      if (parameter_cast<RawConvertParam>(&spec))
      {
        return Delegate->Convert(spec, params, dst);
      }
      return CreateUnsupportedConversionError(THIS_LINE, spec);
    }
  private:
    const Vortex::Track::ModuleData::Ptr Data;
    const Holder::Ptr Delegate;
    const uint_t PatOffset;
  };
}

namespace PT3
{
  using namespace ZXTune;

  //plugin attributes
  const Char ID[] = {'P', 'T', '3', 0};
  const uint_t CAPS = CAP_STOR_MODULE | CAP_DEV_AYM | CAP_CONV_RAW | GetSupportedAYMFormatConvertors() | GetSupportedVortexFormatConvertors();

  class Factory : public ModulesFactory
  {
  public:
    explicit Factory(Formats::Chiptune::Decoder::Ptr decoder)
      : Decoder(decoder)
    {
    }

    virtual bool Check(const Binary::Container& inputData) const
    {
      return Decoder->Check(inputData);
    }

    virtual Binary::Format::Ptr GetFormat() const
    {
      return Decoder->GetFormat();
    }

    virtual Holder::Ptr CreateModule(ModuleProperties::RWPtr properties, Binary::Container::Ptr rawData, std::size_t& usedSize) const
    {
      const ::ProTracker3::Track::ModuleData::RWPtr modData = ::ProTracker3::Track::ModuleData::Create();
      uint_t patOffset = 0;
      const std::auto_ptr<Formats::Chiptune::ProTracker3::Builder> dataBuilder = ::ProTracker3::CreateDataBuilder(modData, properties, patOffset);
      if (const Formats::Chiptune::Container::Ptr container = Formats::Chiptune::ProTracker3::ParseCompiled(*rawData, *dataBuilder))
      {
        usedSize = container->Size();
        properties->SetSource(container);
        if (patOffset != Formats::Chiptune::ProTracker3::SINGLE_AY_MODE)
        {
          //TurboSound modules
          properties->SetComment(Text::PT3_TURBOSOUND_MODULE);
          const Vortex::Track::ModuleData::Ptr fixedModData = boost::make_shared<TSModuleData>(patOffset, modData);
          const AYM::Chiptune::Ptr chiptune = Vortex::CreateChiptune(fixedModData, properties,  2 * Devices::AYM::CHANNELS);
          const Holder::Ptr nativeHolder = AYM::CreateHolder(chiptune);
          return boost::make_shared<TSHolder>(fixedModData, nativeHolder, patOffset);
        }
        else
        {
          const AYM::Chiptune::Ptr chiptune = Vortex::CreateChiptune(modData, properties,  Devices::AYM::CHANNELS);
          const Holder::Ptr nativeHolder = AYM::CreateHolder(chiptune);
          return Vortex::CreateHolder(modData, nativeHolder);
        }
      }
      return Holder::Ptr();
    }
  private:
    const Formats::Chiptune::Decoder::Ptr Decoder;
  };
}

namespace PT3
{
  const Char IDC_PTU13[] = {'C', 'O', 'M', 'P', 'I', 'L', 'E', 'D', 'P', 'T', 'U', '1', '3', 0};
  const uint_t CCAPS = CAP_STOR_CONTAINER;
}

namespace ZXTune
{
  void RegisterPT3Support(PluginsRegistrator& registrator)
  {
    //modules with players
    {
      const Formats::Packed::Decoder::Ptr decoder = Formats::Packed::CreateCompiledPTU13Decoder();
      const ArchivePlugin::Ptr plugin = CreateArchivePlugin(PT3::IDC_PTU13, PT3::CCAPS, decoder);
      registrator.RegisterPlugin(plugin);
    }
    //direct modules
    {
      const Formats::Chiptune::Decoder::Ptr decoder = Formats::Chiptune::CreateProTracker3Decoder();
      const ModulesFactory::Ptr factory = boost::make_shared<PT3::Factory>(decoder);
      const PlayerPlugin::Ptr plugin = CreatePlayerPlugin(PT3::ID, decoder->GetDescription(), PT3::CAPS, factory);
      registrator.RegisterPlugin(plugin);
    }
  }
}
