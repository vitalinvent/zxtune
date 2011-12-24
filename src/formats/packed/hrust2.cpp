/*
Abstract:
  Hrust2 support

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
  (C) Based on XLook sources by HalfElf
*/

//local includes
#include "container.h"
#include "hrust1_bitstream.h"
#include "pack_utils.h"
//common includes
#include <byteorder.h>
#include <tools.h>
//library includes
#include <binary/typed_container.h>
#include <formats/packed.h>
//std includes
#include <numeric>
#include <cstring>
//boost includes
#include <boost/bind.hpp>
#include <boost/make_shared.hpp>
//text includes
#include <formats/text/packed.h>

namespace Hrust2
{
  const std::size_t MAX_DECODED_SIZE = 0x10000;

#ifdef USE_PRAGMA_PACK
#pragma pack(push,1)
#endif
  PACK_PRE struct RawHeader
  {
    uint8_t LastBytes[6];
    uint8_t FirstByte;
    uint8_t BitStream[1];
  } PACK_POST;

  namespace Version1
  {
    const std::string HEADER_FORMAT(
      "'h'r'2"    //ID
      "%x0110001" //Flag
    );

    PACK_PRE struct FormatHeader
    {
      uint8_t ID[3];//'hr2'
      uint8_t Flag;//'1' | 128
      uint16_t DataSize;
      uint16_t PackedSize;
      RawHeader Stream;

      //flag bits
      enum
      {
        NO_COMPRESSION = 128
      };
    } PACK_POST;
  }

  namespace Version3
  {
    const std::string HEADER_FORMAT(
      "'H'r's't'2" //ID
      "%00x00xxx"  //Flag
    );

    PACK_PRE struct FormatHeader
    {
      uint8_t ID[5];//'Hrst2'
      uint8_t Flag;
      uint16_t DataSize;
      uint16_t PackedSize;//without header
      uint8_t AdditionalSize;
      //additional
      uint16_t PackedCRC;
      uint16_t DataCRC;
      char Name[8];
      char Type[3];
      uint16_t Filesize;
      uint8_t Filesectors;
      uint8_t Subdir;
      char Comment[1];

      //flag bits
      enum
      {
        STORED_BLOCK = 1,
        LAST_BLOCK = 2,
        LAST_FILE = 4,
        SOLID_BLOCK = 8,
        SOLID_FILE = 16,
        DELETED_FILE = 32,
        CRYPTED_FILE = 64,
        SUBDIR = 128
      };

      std::size_t GetSize() const
      {
        return offsetof(FormatHeader, PackedCRC) + AdditionalSize;
      }

      std::size_t GetTotalSize() const
      {
        return GetSize() + fromLE(PackedSize);
      }
    } PACK_POST;
  }
#ifdef USE_PRAGMA_PACK
#pragma pack(pop)
#endif

  //hrust2x bitstream decoder
  class Bitstream : public ByteStream
  {
  public:
    Bitstream(const uint8_t* data, std::size_t size)
      : ByteStream(data, size)
      , Bits(), Mask(0)
    {
    }

    uint_t GetBit()
    {
      if (!(Mask >>= 1))
      {
        Bits = GetByte();
        Mask = 0x80;
      }
      return Bits & Mask ? 1 : 0;
    }

    uint_t GetBits(uint_t count)
    {
      uint_t result = 0;
      while (count--)
      {
        result = 2 * result | GetBit();
      }
      return result;
    }

    uint_t GetLen()
    {
      uint_t len = 1;
      for (uint_t bits = 3; bits == 0x3 && len != 0x10;)
      {
        bits = GetBits(2);
        len += bits;
      }
      return len;
    }

    int_t GetDist()
    {
      //%1,disp8
      if (GetBit())
      {
        return static_cast<int16_t>(0xff00 + GetByte());
      }
      else
      {
        //%011x,%010xx,%001xxx,%000xxxx,%0000000
        uint_t res = 0xffff;
        for (uint_t bits = 4 - GetBits(2); bits; --bits)
        {
          res = (res << 1) + GetBit() - 1;
        }
        res &= 0xffff;
        if (0xffe1 == res)
        {
          res = GetByte();
        }
        return static_cast<int16_t>((res << 8) + GetByte());
      }
    }
  private:
    uint_t Bits;
    uint_t Mask;
  };

  class RawDataDecoder
  {
  public:
    RawDataDecoder(const RawHeader& header, std::size_t rawSize)
      : Header(header)
      , Stream(Header.BitStream, rawSize)
      , IsValid(!Stream.Eof())
      , Result(new Dump())
      , Decoded(*Result)
    {
      if (IsValid)
      {
        Decoded.reserve(rawSize * 2);
        IsValid = DecodeData();
      }
    }

    std::auto_ptr<Dump> GetResult()
    {
      return IsValid
        ? Result
        : std::auto_ptr<Dump>();
    }
  private:
    bool DecodeData()
    {
      //put first byte
      Decoded.push_back(Header.FirstByte);

      while (!Stream.Eof() && Decoded.size() < MAX_DECODED_SIZE)
      {
        //%1,byte
        if (Stream.GetBit())
        {
          Decoded.push_back(Stream.GetByte());
          continue;
        }
        uint_t len = Stream.GetLen();
        //%01100..
        if (4 == len)
        {
          //%011001
          if (Stream.GetBit())
          {
            uint_t len = Stream.GetByte();
            if (!len)
            {
              break;//eof
            }
            else if (len < 16)
            {
              len = len * 256 | Stream.GetByte();
            }
            const int_t offset = Stream.GetDist();
            if (!CopyFromBack(-offset, Decoded, len))
            {
              return false;
            }
          }
          else//%011000xxxx
          {
            for (uint_t len = 2 * (Stream.GetBits(4) + 6); len; --len)
            {
              Decoded.push_back(Stream.GetByte());
            }
          }
        }
        else
        {
          if (len > 4)
          {
            --len;
          }
          const int_t offset = 1 == len
            ? static_cast<int16_t>(0xfff8 + Stream.GetBits(3))
            : (2 == len ? static_cast<int16_t>(0xff00 + Stream.GetByte()) : Stream.GetDist());
          if (!CopyFromBack(-offset, Decoded, len))
          {
            return false;
          }
        }
      }
      std::copy(Header.LastBytes, ArrayEnd(Header.LastBytes), std::back_inserter(Decoded));
      return true;
    }
  private:
    const RawHeader& Header;
    Bitstream Stream;
    bool IsValid;
    std::auto_ptr<Dump> Result;
    Dump& Decoded;
  };

  namespace Version1
  {
    class Container
    {
    public:
      Container(const void* data, std::size_t size)
        : Data(static_cast<const uint8_t*>(data))
        , Size(size)
      {
      }

      bool FastCheck() const
      {
        if (Size < sizeof(FormatHeader))
        {
          return false;
        }
        const FormatHeader& header = GetHeader();
        if (0 != (header.Flag & FormatHeader::NO_COMPRESSION) &&
            header.PackedSize != header.DataSize)
        {
          return false;
        }
        const std::size_t usedSize = GetUsedSize();
        return in_range(usedSize, sizeof(header), Size);
      }

      uint_t GetUsedSize() const
      {
        const FormatHeader& header = GetHeader();
        return sizeof(header) + fromLE(header.PackedSize) - sizeof(header.Stream);
      }

      const FormatHeader& GetHeader() const
      {
        assert(Size >= sizeof(FormatHeader));
        return *safe_ptr_cast<const FormatHeader*>(Data);
      }
    private:
      const uint8_t* const Data;
      const std::size_t Size;
    };

    class DataDecoder
    {
    public:
      explicit DataDecoder(const Container& container)
        : IsValid(container.FastCheck())
        , Header(container.GetHeader())
        , Result(new Dump())
      {
        IsValid = IsValid && DecodeData();
      }

      std::auto_ptr<Dump> GetResult()
      {
        return IsValid
          ? Result
          : std::auto_ptr<Dump>();
      }
    private:
      bool DecodeData()
      {
        const uint_t size = fromLE(Header.DataSize);
        if (0 != (Header.Flag & Header.NO_COMPRESSION))
        {
          //just copy
          Result->resize(size);
          std::memcpy(&(*Result)[0], &Header.Stream, size);
          return true;
        }
        RawDataDecoder decoder(Header.Stream, fromLE(Header.PackedSize));
        Result = decoder.GetResult();
        return 0 != Result.get();
      }
    private:
      bool IsValid;
      const FormatHeader& Header;
      std::auto_ptr<Dump> Result;
    };
  }

  namespace Version3
  {
    class Container
    {
    public:
      Container(const void* data, std::size_t size)
        : Data(static_cast<const uint8_t*>(data))
        , Size(size)
      {
      }

      bool FastCheck() const
      {
        if (Size < sizeof(FormatHeader))
        {
          return false;
        }
        //at least one block should be available
        const FormatHeader& header = GetHeader();
        if (0 != (header.Flag & FormatHeader::STORED_BLOCK) &&
            header.PackedSize != header.DataSize)
        {
          return false;
        }
        return in_range(header.GetTotalSize(), sizeof(header), Size);
      }
   private:
      const FormatHeader& GetHeader() const
      {
        assert(Size >= sizeof(FormatHeader));
        return *safe_ptr_cast<const FormatHeader*>(Data);
      }
    private:
      const uint8_t* const Data;
      const std::size_t Size;
    };

    class BlocksAccumulator
    {
    public:
      void AddBlock(Binary::Container::Ptr block)
      {
        Blocks.push_back(block);
      }

      Binary::Container::Ptr GetResult() const
      {
        if (Blocks.empty())
        {
          return Binary::Container::Ptr();
        }
        if (1 == Blocks.size())
        {
          return Blocks.front();
        }
        const std::size_t totalSize = std::accumulate(Blocks.begin(), Blocks.end(), 0, 
          boost::bind(std::plus<std::size_t>(), _1,
            boost::bind(&Binary::Container::Size, _2)));
        std::auto_ptr<Dump> result(new Dump(totalSize));
        std::size_t offset = 0;
        for (std::vector<Binary::Container::Ptr>::const_iterator it = Blocks.begin(), lim = Blocks.end(); it != lim; ++it)
        {
          const Binary::Container::Ptr block = *it;
          std::memcpy(&result->at(offset), block->Data(), block->Size());
          offset += block->Size();
        }
        return Binary::CreateContainer(result);
      }
    private:
      std::vector<Binary::Container::Ptr> Blocks;
    };

    Binary::Container::Ptr DecodeBlock(const Binary::Container& data)
    {
      const RawHeader& block = *safe_ptr_cast<const RawHeader*>(data.Data());
      RawDataDecoder decoder(block, data.Size());
      return Binary::CreateContainer(decoder.GetResult());
    }

    class DataDecoder
    {
    public:
      explicit DataDecoder(const Binary::Container& data)
        : Data(data)
        , UsedSize()
      {
        if (Container(data.Data(), data.Size()).FastCheck())
        {
          DecodeData();
        }
      }

      Binary::Container::Ptr GetResult()
      {
        return Result;
      }

      std::size_t GetUsedSize() const
      {
        return UsedSize;
      }
    private:
      void DecodeData()
      {
        const Binary::TypedContainer source(Data);
        BlocksAccumulator target;
        std::size_t offset = 0;
        for (;;)
        {
          if (const FormatHeader* hdr = source.GetField<FormatHeader>(offset))
          {
            const std::size_t blockEnd = offset + hdr->GetTotalSize();
            if (blockEnd > Data.Size())
            {
              break;
            }
            const std::size_t packedOffset = offset + hdr->GetSize();
            const std::size_t packedSize = blockEnd - packedOffset;
            const Binary::Container::Ptr packedData = Data.GetSubcontainer(packedOffset, packedSize);
            const bool storedBlock = (0 != (hdr->Flag & FormatHeader::STORED_BLOCK));
            if (const Binary::Container::Ptr unpackedData = storedBlock ? packedData : DecodeBlock(*packedData))
            {
              target.AddBlock(unpackedData);
            }
            else
            {
              break;
            }
            offset = blockEnd;
            if (0 != (hdr->Flag & FormatHeader::LAST_BLOCK))
            {
              break;
            }
          }
          else
          {
            break;
          }
        }
        if (Result = target.GetResult())
        {
          UsedSize = offset;
        }
      }
    private:
      const Binary::Container& Data;
      Binary::Container::Ptr Result;
      std::size_t UsedSize;
    };
  }
}

namespace Formats
{
  namespace Packed
  {
    class Hrust21Decoder : public Decoder
    {
    public:
      Hrust21Decoder()
        : Format(Binary::Format::Create(Hrust2::Version1::HEADER_FORMAT))
      {
      }

      virtual String GetDescription() const
      {
        return Text::HRUST21_DECODER_DESCRIPTION;
      }

      virtual Binary::Format::Ptr GetFormat() const
      {
        return Format;
      }

      virtual Container::Ptr Decode(const Binary::Container& rawData) const
      {
        const void* const data = rawData.Data();
        const std::size_t availSize = rawData.Size();
        const Hrust2::Version1::Container container(data, availSize);
        if (!container.FastCheck() || !Format->Match(data, availSize))
        {
          return Container::Ptr();
        }
        Hrust2::Version1::DataDecoder decoder(container);
        return CreatePackedContainer(decoder.GetResult(), container.GetUsedSize());
      }
    private:
      const Binary::Format::Ptr Format;
    };

    class Hrust23Decoder : public Decoder
    {
    public:
      Hrust23Decoder()
        : Format(Binary::Format::Create(Hrust2::Version3::HEADER_FORMAT))
      {
      }

      virtual String GetDescription() const
      {
        return Text::HRUST23_DECODER_DESCRIPTION;
      }

      virtual Binary::Format::Ptr GetFormat() const
      {
        return Format;
      }

      virtual Container::Ptr Decode(const Binary::Container& rawData) const
      {
        const void* const data = rawData.Data();
        const std::size_t availSize = rawData.Size();
        const Hrust2::Version3::Container container(data, availSize);
        if (!container.FastCheck() || !Format->Match(data, availSize))
        {
          return Container::Ptr();
        }
        Hrust2::Version3::DataDecoder decoder(rawData);
        return CreatePackedContainer(decoder.GetResult(), decoder.GetUsedSize());
      }
    private:
      const Binary::Format::Ptr Format;
    };

    Decoder::Ptr CreateHrust21Decoder()
    {
      return Decoder::Ptr(new Hrust21Decoder());
    }

    Decoder::Ptr CreateHrust23Decoder()
    {
      return Decoder::Ptr(new Hrust23Decoder());
    }
  }
}

