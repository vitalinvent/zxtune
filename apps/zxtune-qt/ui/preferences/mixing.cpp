/*
Abstract:
  Mixing settings widget implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001

  This file is a part of zxtune-qt application based on zxtune library
*/

//local includes
#include "mixing.h"
#include "mixing.ui.h"
#include "mixer.h"
#include "supp/mixer.h"
#include "supp/options.h"
#include "ui/utils.h"
//common includes
#include <format.h>
//qt includes
#include <QtGui/QLabel>
//text includes
#include "text/text.h"

namespace
{
  const char* OUTPUT_CHANNEL_NAMES[ZXTune::Sound::OUTPUT_CHANNELS] =
  {
    "Left",
    "Right"
  };

  class MixingOptionsWidget : public UI::MixingSettingsWidget
                            , public Ui::MixingOptions
  {
  public:
    MixingOptionsWidget(QWidget& parent, unsigned channels)
      : UI::MixingSettingsWidget(parent)
      , Options(GlobalOptions::Instance().Get())
    {
      //setup self
      setupUi(this);
      setWindowTitle(ToQString(Strings::Format(Text::MIXER_WINDOW_TITLE_TEMPLATE, channels)));

      for (uint_t inChan = 0; inChan != channels; ++inChan)
      {
        channelNames->addWidget(new QLabel(QString(Text::INPUT_CHANNEL_NAMES[inChan]), this));
        for (uint_t outChan = 0; outChan != ZXTune::Sound::OUTPUT_CHANNELS; ++outChan)
        {
          UI::MixerWidget* const mixer = UI::MixerWidget::Create(*this, OUTPUT_CHANNEL_NAMES[outChan]);
          channelValues->addWidget(mixer);
          const int defLevel = GetMixerChannelDefaultValue(channels, inChan, outChan);
          Parameters::MixerValue::Bind(*mixer, *Options, GetMixerChannelParameterName(channels, inChan, outChan), defLevel);
        }
      }
    }
  private:
    const Parameters::Container::Ptr Options;
  };
}

namespace UI
{
  MixingSettingsWidget::MixingSettingsWidget(QWidget& parent) : QWidget(&parent)
  {
  }

  MixingSettingsWidget* MixingSettingsWidget::Create(QWidget& parent, unsigned channels)
  {
    return new MixingOptionsWidget(parent, channels);
  }
}
