/*
Abstract:
  AYM parameters helper

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

#ifndef __AYM_PARAMETERS_HELPER_H_DEFINED__
#define __AYM_PARAMETERS_HELPER_H_DEFINED__

//common includes
#include <parameters.h>
//library includes
#include <core/freq_tables.h>
#include <devices/aym.h>
//std includes
#include <memory>

namespace ZXTune
{
  namespace AYM
  {
    // Performs aym-related parameters applying
    class ParametersHelper
    {
    public:
      typedef std::auto_ptr<ParametersHelper> Ptr;

      virtual ~ParametersHelper() {}

      //called with new parameters set
      virtual void SetParameters(const Parameters::Accessor& params) = 0;

      //frequency table according to parameters
      virtual const Module::FrequencyTable& GetFreqTable() const = 0;
      //initial data chunk according to parameters
      virtual void GetDataChunk(DataChunk& dst) const = 0;

      static Ptr Create(const String& defaultFreqTable);
    };
  }

  //Temporary adapter
  class AYMTrackSynthesizer
  {
  public:
    explicit AYMTrackSynthesizer(const AYM::ParametersHelper& helper)
      : Helper(helper)
    {
    }

    //infrastructure
    void InitData(uint64_t tickToPlay);
    AYM::DataChunk& GetData();

    void SetTone(uint_t chanNum, int_t halfTones, int_t offset);
    void SetNoise(uint_t chanNum, uint_t level);
    void SetLevel(uint_t chanNum, uint_t level);
    void EnableEnvelope(uint_t chanNum);

    void SetEnvelope(uint_t type, uint_t tone);
  private:
    const AYM::ParametersHelper& Helper;
    AYM::DataChunk Chunk;
  };
}

#endif //__AYM_PARAMETERS_HELPER_H_DEFINED__
