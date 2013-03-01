/*
Abstract:
  DAC interface

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

#pragma once
#ifndef __DEVICES_DAC_H_DEFINED__
#define __DEVICES_DAC_H_DEFINED__

//common includes
#include <data_streaming.h>
//library includes
#include <devices/dac_sample.h>
#include <time/stamp.h>

//supporting for multichannel sample-based DAC
namespace Devices
{
  namespace DAC
  {
    struct DataChunk
    {
      struct ChannelData
      {
        ChannelData()
          : Channel()
          , Mask()
          , Enabled()
          , Note()
          , NoteSlide()
          , FreqSlideHz()
          , SampleNum()
          , PosInSample()
          , LevelInPercents()
        {
        }

        enum Flags
        {
          ENABLED = 1,
          NOTE = 2,
          NOTESLIDE = 4,
          FREQSLIDEHZ = 8,
          SAMPLENUM = 16,
          POSINSAMPLE = 32,
          LEVELINPERCENTS = 64,

          ALL_PARAMETERS = 127
        };

        uint_t Channel;
        uint_t Mask;
        bool Enabled;
        uint_t Note;
        int_t NoteSlide;
        int_t FreqSlideHz;
        uint_t SampleNum;
        uint_t PosInSample;
        uint_t LevelInPercents;

        const bool* GetEnabled() const
        {
          return 0 != (Mask & ENABLED) ? &Enabled : 0;
        }

        const uint_t* GetNote() const
        {
          return 0 != (Mask & NOTE) ? &Note : 0;
        }

        const int_t* GetNoteSlide() const
        {
          return 0 != (Mask & NOTESLIDE) ? &NoteSlide : 0;
        }

        const int_t* GetFreqSlideHz() const
        {
          return 0 != (Mask & FREQSLIDEHZ) ? &FreqSlideHz : 0;
        }

        const uint_t* GetSampleNum() const
        {
          return 0 != (Mask & SAMPLENUM) ? &SampleNum : 0;
        }

        const uint_t* GetPosInSample() const
        {
          return 0 != (Mask & POSINSAMPLE) ? &PosInSample : 0;
        }

        const uint_t* GetLevelInPercents() const
        {
          return 0 != (Mask & LEVELINPERCENTS) ? &LevelInPercents : 0;
        }
      };

      Time::Microseconds TimeStamp;
      std::vector<ChannelData> Channels;
    };

    //channels state
    struct ChanState
    {
      ChanState()
        : Enabled(), Band(), LevelInPercents()
      {
      }

      //Is channel enabled to output
      bool Enabled;
      //Currently played tone band (up to 96)
      uint_t Band;
      //Currently played tone level percentage
      uint_t LevelInPercents;
    };
    typedef std::vector<ChanState> ChannelsState;

    class Chip
    {
    public:
      typedef boost::shared_ptr<Chip> Ptr;

      virtual ~Chip() {}

      /// Set sample for work
      virtual void SetSample(uint_t idx, Sample::Ptr sample) = 0;

      /// render single data chunk
      virtual void RenderData(const DataChunk& src) = 0;
      virtual void Flush() = 0;
      virtual void GetChannelState(uint_t chan, DataChunk::ChannelData& dst) const = 0;

      virtual void GetState(ChannelsState& state) const = 0;

      /// reset internal state to initial
      virtual void Reset() = 0;
    };

    // Variable channels per sample
    typedef std::vector<SoundSample> MultiSoundSample;
    // Result sound stream receiver
    typedef DataReceiver<MultiSoundSample> Receiver;

    class ChipParameters
    {
    public:
      typedef boost::shared_ptr<const ChipParameters> Ptr;

      virtual ~ChipParameters() {}

      virtual uint_t SoundFreq() const = 0;
      virtual bool Interpolate() const = 0;
    };

    /// Virtual constructors
    Chip::Ptr CreateChip(uint_t channels, uint_t sampleFreq, ChipParameters::Ptr params, Receiver::Ptr target);
  }
}

#endif //__DEVICES_DAC_H_DEFINED__
