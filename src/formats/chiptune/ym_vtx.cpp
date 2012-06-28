/*
Abstract:
  YM/VTX modules format implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "container.h"
#include "ym.h"
//common includes
#include <byteorder.h>
#include <contract.h>
#include <crc.h>
#include <logging.h>
//library includes
#include <binary/input_stream.h>
#include <binary/typed_container.h>
#include <formats/packed/lha_supp.h>
//std includes
#include <cstring>
//boost includes
#include <boost/array.hpp>
#include <boost/make_shared.hpp>
//text includes
#include <formats/text/chiptune.h>

namespace
{
  const std::string THIS_MODULE("Formats::Chiptune::YM");
}

namespace Formats
{
namespace Chiptune
{
  namespace YM
  {
#ifdef USE_PRAGMA_PACK
#pragma pack(push,1)
#endif
    typedef boost::array<uint8_t, 14> RegistersDump;
    typedef boost::array<uint8_t, 4> IdentifierType;

    const uint_t CLOCKRATE_MIN = 100000;//100kHz

    const uint_t INTFREQ_MIN = 25;
    const uint_t INTFREQ_MAX = 100;

    const std::size_t MAX_STRING_SIZE = 254;

    namespace Ver2
    {
      const uint8_t ID[] = {'Y', 'M', '2', '!'};

      PACK_PRE struct RawHeader
      {
        IdentifierType Signature;
        RegistersDump Row;
      } PACK_POST;

      bool CheckSize(std::size_t size)
      {
        return size >= sizeof(RawHeader) + sizeof(RegistersDump) * INTFREQ_MIN;
      }

      bool FastCheck(const void* data, std::size_t size)
      {
        return CheckSize(size) && 0 == std::memcmp(data, ID, sizeof(ID));
      }
    }

    namespace Ver3
    {
      const uint8_t ID[] = {'Y', 'M', '3', '!'};

      PACK_PRE struct RawHeader
      {
        IdentifierType Signature;
        RegistersDump Row;
      } PACK_POST;

      bool CheckSize(std::size_t size)
      {
        return size >= sizeof(RawHeader) + sizeof(RegistersDump) * INTFREQ_MIN;
      }

      bool FastCheck(const void* data, std::size_t size)
      {
        return CheckSize(size) && 0 == std::memcmp(data, ID, sizeof(ID));
      }
    }

    namespace Ver3b
    {
      const uint8_t ID[] = {'Y', 'M', '3', 'b'};

      PACK_PRE struct RawHeader
      {
        IdentifierType Signature;
        RegistersDump Row;
      } PACK_POST;

      bool CheckSize(std::size_t size)
      {
        return size >= sizeof(RawHeader) + sizeof(RegistersDump) * INTFREQ_MIN + sizeof(uint32_t);
      }

      bool FastCheck(const void* data, std::size_t size)
      {
        return CheckSize(size) && 0 == std::memcmp(data, ID, sizeof(ID));
      }
    }

    namespace Ver5
    {
      const uint8_t ID[] = {'Y', 'M', '5', '!', 'L', 'e','O', 'n', 'A', 'r', 'D', '!'};

      typedef boost::array<uint8_t, 16> RegistersDump;
      typedef boost::array<uint8_t, 4> Footer;

      PACK_PRE struct RawHeader
      {
        uint8_t Signature[12];
        uint32_t Frames;
        uint32_t Attrs;
        uint16_t SamplesCount;
        uint32_t Clockrate;
        uint16_t IntFreq;
        uint32_t Loop;
        uint16_t ExtraSize;
      } PACK_POST;

      bool CheckSize(std::size_t size)
      {
        return size >= sizeof(RawHeader) + sizeof(RegistersDump) * INTFREQ_MIN;
      }

      bool FastCheck(const void* data, std::size_t size)
      {
        return CheckSize(size) && 0 == std::memcmp(data, ID, sizeof(ID));
      }
    }

    namespace Compressed
    {
      PACK_PRE struct RawHeader
      {
        uint8_t HeaderSize;
        uint8_t HeaderSum;
        char Method[5];
        uint32_t PackedSize;
        uint32_t OriginalSize;
        uint32_t Time;
        uint8_t Attribute;
        uint8_t Level;
        uint8_t NameLen;

        std::size_t GetDataOffset() const
        {
          return offsetof(RawHeader, Method) + HeaderSize;
        }
      } PACK_POST;

      bool FastCheck(const void* data, std::size_t size)
      {
        if (size < sizeof(RawHeader))
        {
          return 0;
        }
        const RawHeader& hdr = *static_cast<const RawHeader*>(data);
        const std::size_t hdrLen = hdr.GetDataOffset();
        const std::size_t ftrLen = 1;//final byte
        if (hdrLen + fromLE(hdr.PackedSize) + ftrLen > size)
        {
          return false;
        }
        const std::size_t origSize = fromLE(hdr.OriginalSize);
        return Ver2::CheckSize(origSize)
            || Ver3::CheckSize(origSize)
            || Ver3b::CheckSize(origSize)
            || Ver5::CheckSize(origSize);
      }
    }
#ifdef USE_PRAGMA_PACK
#pragma pack(pop)
#endif

    class StubBuilder : public Builder
    {
    public:
      virtual void SetVersion(const String& /*version*/) {}
      virtual void SetChipType(bool /*ym*/) {}
      virtual void SetStereoMode(uint_t /*mode*/) {}
      virtual void SetLoop(uint_t /*loop*/) {}
      virtual void SetDigitalSample(uint_t /*idx*/, const Dump& /*data*/) {}
      virtual void SetClockrate(uint64_t /*freq*/) {}
      virtual void SetIntFreq(uint_t /*freq*/) {}
      virtual void SetTitle(const String& /*title*/) {}
      virtual void SetAuthor(const String& /*author*/) {}
      virtual void SetComment(const String& /*comment*/) {}
      virtual void SetYear(uint_t /*year*/) {}
      virtual void SetProgram(const String& /*program*/) {}
      virtual void SetEditor(const String& /*editor*/) {}

      virtual void AddData(const Dump& /*registers*/) {}
    };

    bool FastCheck(const Binary::Container& rawData)
    {
      const void* const data = rawData.Data();
      const std::size_t size = rawData.Size();
      return Ver2::FastCheck(data, size)
          || Ver3::FastCheck(data, size)
          || Ver3b::FastCheck(data, size)
          || Ver5::FastCheck(data, size)
      ;
    }

    void ParseTransponedMatrix(const uint8_t* data, std::size_t size, std::size_t columns, Builder& target)
    {
      const std::size_t lines = size / columns;
      Require(lines != 0);
      for (std::size_t row = 0; row != lines; ++row)
      {
        Dump registers(columns);
        for (std::size_t col = 0, cursor = row; col != columns; ++col, cursor += lines)
        {
          registers[col] = data[cursor];
        }
        target.AddData(registers);
      }
    }

    void ParseMatrix(const uint8_t* data, std::size_t size, std::size_t columns, Builder& target)
    {
      const std::size_t lines = size / columns;
      Require(lines != 0);
      const uint8_t* cursor = data;
      for (std::size_t row = 0; row != lines; ++row, data += columns)
      {
        const Dump registers(cursor, cursor + columns);
        target.AddData(registers);
      }
    }

    Formats::Chiptune::Container::Ptr ParseUnpacked(const Binary::Container& rawData, Builder& target)
    {
      const void* const data = rawData.Data();
      const std::size_t size = rawData.Size();
      try
      {
        if (Ver2::FastCheck(data, size)
         || Ver3::FastCheck(data, size)
         || Ver3b::FastCheck(data, size))
        {
          Binary::InputStream stream(rawData);
          const IdentifierType& type = stream.ReadField<IdentifierType>();
          target.SetVersion(String(type.begin(), type.end()));

          const std::size_t dumpOffset = sizeof(IdentifierType);
          const std::size_t dumpSize = size - dumpOffset;
          const std::size_t columns = sizeof(RegistersDump);
          const std::size_t lines = dumpSize / columns;
          const std::size_t matrixSize = lines * columns;
          const uint8_t* const src = stream.ReadData(matrixSize);
          ParseTransponedMatrix(src, matrixSize, columns, target);
          if (Ver3b::FastCheck(data, size))
          {
            const uint_t loop = fromBE(stream.ReadField<uint32_t>());
            target.SetLoop(loop);
          }
          const Binary::Container::Ptr subData = stream.GetReadData();
          return CreateCalculatingCrcContainer(subData, dumpOffset, matrixSize);
        }
        else if (Ver5::FastCheck(data, size))
        {
          Binary::InputStream stream(rawData);
          const Ver5::RawHeader& header = stream.ReadField<Ver5::RawHeader>();
          if (0 != header.SamplesCount)
          {
            Log::Debug(THIS_MODULE, "Digital samples are not supported");
            return Formats::Chiptune::Container::Ptr();
          }
          target.SetVersion(String(header.Signature, header.Signature + sizeof(IdentifierType)));
          target.SetClockrate(fromBE(header.Clockrate));
          target.SetIntFreq(fromBE(header.IntFreq));
          target.SetLoop(fromBE(header.Loop));
          target.SetTitle(FromStdString(stream.ReadCString(MAX_STRING_SIZE)));
          target.SetAuthor(FromStdString(stream.ReadCString(MAX_STRING_SIZE)));
          target.SetComment(FromStdString(stream.ReadCString(MAX_STRING_SIZE)));
          const uint_t attrs = fromBE(header.Attrs);

          const std::size_t dumpOffset = stream.GetPosition();
          const std::size_t dumpSize = size - sizeof(Ver5::Footer) - dumpOffset;
          const std::size_t columns = sizeof(Ver5::RegistersDump);
          const std::size_t lines = dumpSize / columns;
          const std::size_t matrixSize = lines * columns;
          const uint8_t* const src = stream.ReadData(matrixSize);
          if (0 != (attrs & 1))
          {
            ParseTransponedMatrix(src, matrixSize, columns, target);
          }
          else
          {
            ParseMatrix(src, matrixSize, columns, target);
          }
          /*const Ver5::Footer& footer = */stream.ReadField<Ver5::Footer>();
          const Binary::Container::Ptr subData = stream.GetReadData();
          return CreateCalculatingCrcContainer(subData, dumpOffset, matrixSize);
        }
      }
      catch (const std::exception&)
      {
        Log::Debug(THIS_MODULE, "Failed to parse");
      }
      return Formats::Chiptune::Container::Ptr();
    }

    Formats::Chiptune::Container::Ptr ParseYM(const Binary::Container& rawData, Builder& target)
    {
      const void* const data = rawData.Data();
      const std::size_t size = rawData.Size();
      if (!Compressed::FastCheck(data, size))
      {
        return Formats::Chiptune::Container::Ptr();
      }
      const Compressed::RawHeader& hdr = *static_cast<const Compressed::RawHeader*>(data);
      const std::size_t packedOffset = hdr.GetDataOffset();
      const std::size_t packedSize = fromLE(hdr.PackedSize);
      const Binary::Container::Ptr packed = rawData.GetSubcontainer(packedOffset, packedSize);
      const std::size_t unpackedSize = fromLE(hdr.OriginalSize);
      if (Formats::Packed::Container::Ptr unpacked = Formats::Packed::Lha::DecodeRawData(*packed, FromCharArray(hdr.Method), unpackedSize))
      {
        if (ParseUnpacked(*unpacked, target))
        {
          const Binary::Container::Ptr subData = rawData.GetSubcontainer(0, packedOffset + packedSize);
          return CreateCalculatingCrcContainer(subData, packedOffset, packedSize);
        }
      }
      return Formats::Chiptune::Container::Ptr();
    }

    const std::string FORMAT(
      "16-ff"      //header size
      "?"          //header sum
      "'-'l'h'5'-" //method
      "????"       //packed size
      "????"       //original size
      "????"       //time+date
      "%00x00xxx"  //attribute
      "00"         //level
    );

    class Decoder : public Formats::Chiptune::Decoder
    {
    public:
      Decoder()
        : Format(Binary::Format::Create(FORMAT))
      {
      }

      virtual String GetDescription() const
      {
        return Text::YM_DECODER_DESCRIPTION;
      }

      virtual Binary::Format::Ptr GetFormat() const
      {
        return Format;
      }

      virtual bool Check(const Binary::Container& rawData) const
      {
        return Format->Match(rawData.Data(), rawData.Size());
      }

      virtual Formats::Chiptune::Container::Ptr Decode(const Binary::Container& rawData) const
      {
        Builder& stub = GetStubBuilder();
        return ParseYM(rawData, stub);
      }
    private:
      const Binary::Format::Ptr Format;
    };
  }

  namespace VTX
  {
    using namespace YM;

    const uint_t CHIP_AY = 0x7961;
    const uint_t CHIP_YM = 0x6d79;
    const uint_t CHIP_AY_OLD = 0x5941;
    const uint_t CHIP_YM_OLD = 0x4d59;

    const uint_t LAYOUT_MASK = 7;
    const uint_t LAYOUT_MIN = 0;
    const uint_t LAYOUT_MAX = 6;

    const uint_t UNPACKED_MIN = sizeof(RegistersDump) * INTFREQ_MIN * 1;//1 sec
    const uint_t UNPACKED_MAX = sizeof(RegistersDump) * INTFREQ_MAX * 30 * 60;//30 min

#ifdef USE_PRAGMA_PACK
#pragma pack(push,1)
#endif
    PACK_PRE struct RawBasicHeader
    {
      uint16_t ChipType;
      uint8_t LayoutMode;
      uint16_t Loop;
      uint32_t Clockrate;
      uint8_t IntFreq;
    } PACK_POST;

    PACK_PRE struct RawNewHeader : RawBasicHeader
    {
      uint16_t Year;
      uint32_t UnpackedSize;
    } PACK_POST;

    PACK_PRE struct RawOldHeader : RawBasicHeader
    {
      uint32_t UnpackedSize;
    } PACK_POST;
#ifdef USE_PRAGMA_PACK
#pragma pack(pop)
#endif

    BOOST_STATIC_ASSERT(sizeof(RawBasicHeader) == 10);
    BOOST_STATIC_ASSERT(sizeof(RawNewHeader) == 16);
    BOOST_STATIC_ASSERT(sizeof(RawOldHeader) == 14);

    template<class HeaderType>
    bool FastCheck(const HeaderType& hdr)
    {
      if (!in_range<uint_t>(hdr.LayoutMode & LAYOUT_MASK, LAYOUT_MIN, LAYOUT_MAX))
      {
        return false;
      }
      if (!in_range<uint_t>(hdr.IntFreq, INTFREQ_MIN, INTFREQ_MAX))
      {
        return false;
      }
      if (fromLE(hdr.Clockrate) < CLOCKRATE_MIN)
      {
        return false;
      }
      if (!in_range<uint_t>(fromLE(hdr.UnpackedSize), UNPACKED_MIN, UNPACKED_MAX))
      {
        return false;
      }
      return true;
    }

    bool FastCheck(const Binary::Container& rawData)
    {
      const Binary::TypedContainer typedData(rawData);
      if (const RawBasicHeader* basic = typedData.GetField<RawBasicHeader>(0))
      {
        const uint16_t type = fromLE(basic->ChipType);
        if (type == CHIP_AY || type == CHIP_YM)
        {
          if (const RawNewHeader* hdr = typedData.GetField<RawNewHeader>(0))
          {
            return FastCheck(*hdr);
          }
        }
        else if (type == CHIP_AY_OLD || type == CHIP_YM_OLD)
        {
          if (const RawOldHeader* hdr = typedData.GetField<RawOldHeader>(0))
          {
            return FastCheck(*hdr);
          }
        }
      }
      return false;
    }

    const std::string FORMAT(
      "('a|'A|'y|'Y)('y|'Y|'m|'M)" //type
      "00-06"          //layout
      "??"             //loop
      "????"           //clockrate
      "19-64"          //intfreq, 25..100Hz
    );

    class Decoder : public Formats::Chiptune::Decoder
    {
    public:
      Decoder()
        : Format(Binary::Format::Create(FORMAT))
      {
      }

      virtual String GetDescription() const
      {
        return Text::VTX_DECODER_DESCRIPTION;
      }

      virtual Binary::Format::Ptr GetFormat() const
      {
        return Format;
      }

      virtual bool Check(const Binary::Container& rawData) const
      {
        return FastCheck(rawData);
      }

      virtual Formats::Chiptune::Container::Ptr Decode(const Binary::Container& rawData) const
      {
        Builder& stub = GetStubBuilder();
        return ParseVTX(rawData, stub);
      }
    private:
      const Binary::Format::Ptr Format;
    };
  }//namespace VTX

  namespace YM
  {
    Formats::Chiptune::Container::Ptr ParseVTX(const Binary::Container& rawData, Builder& target)
    {
      if (!VTX::FastCheck(rawData))
      {
        return Formats::Chiptune::Container::Ptr();
      }
      try
      {
        Binary::InputStream stream(rawData);
        const VTX::RawBasicHeader& hdr = stream.ReadField<VTX::RawBasicHeader>();
        const uint_t chipType = fromLE(hdr.ChipType);
        const bool ym = chipType == VTX::CHIP_YM || chipType == VTX::CHIP_YM_OLD;
        const bool newVersion = chipType == VTX::CHIP_YM || chipType == VTX::CHIP_AY;
        target.SetChipType(ym);
        target.SetStereoMode(hdr.LayoutMode);
        target.SetLoop(fromLE(hdr.Loop));
        target.SetClockrate(fromLE(hdr.Clockrate));
        target.SetIntFreq(hdr.IntFreq);
        if (newVersion)
        {
          target.SetYear(fromLE(stream.ReadField<uint16_t>()));
        }
        const uint_t unpackedSize = fromLE(stream.ReadField<uint32_t>());
        target.SetTitle(FromStdString(stream.ReadCString(MAX_STRING_SIZE)));
        target.SetAuthor(FromStdString(stream.ReadCString(MAX_STRING_SIZE)));
        if (newVersion)
        {
          target.SetProgram(FromStdString(stream.ReadCString(MAX_STRING_SIZE)));
          target.SetEditor(FromStdString(stream.ReadCString(MAX_STRING_SIZE)));
          target.SetComment(FromStdString(stream.ReadCString(MAX_STRING_SIZE)));
        }

        const std::size_t packedOffset = stream.GetPosition();
        Log::Debug(THIS_MODULE, "Packed data at %1%", packedOffset);
        const Binary::Container::Ptr packed = stream.ReadRestData();
        if (Packed::Container::Ptr unpacked = Packed::Lha::DecodeRawData(*packed, "-lh5-", unpackedSize))
        {
          const std::size_t unpackedSize = unpacked->Size();
          Require(0 == (unpackedSize % sizeof(RegistersDump)));
          ParseTransponedMatrix(static_cast<const uint8_t*>(unpacked->Data()), unpackedSize, sizeof(RegistersDump), target);
          const std::size_t packedSize = unpacked->PackedSize();
          const Binary::Container::Ptr subData = rawData.GetSubcontainer(0, packedOffset + packedSize);
          return CreateCalculatingCrcContainer(subData, packedOffset, packedSize);
        }
        Log::Debug(THIS_MODULE, "Failed to decode ym data");
      }
      catch (const std::exception&)
      {
        Log::Debug(THIS_MODULE, "Failed to parse");
      }
      return Formats::Chiptune::Container::Ptr();
    }

    Builder& GetStubBuilder()
    {
      static StubBuilder stub;
      return stub;
    }
  }

  Formats::Chiptune::Decoder::Ptr CreateVTXDecoder()
  {
    return boost::make_shared<VTX::Decoder>();
  }

  Formats::Chiptune::Decoder::Ptr CreateYMDecoder()
  {
    return boost::make_shared<YM::Decoder>();
  }
}//namespace Chiptune
}//namespace Formats