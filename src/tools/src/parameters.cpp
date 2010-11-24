/*
Abstract:
  Parameters-related functions implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//common includes
#include <parameters.h>
//std includes
#include <algorithm>
#include <cctype>
#include <functional>
#include <set>
//boost includes
#include <boost/bind.hpp>
#include <boost/make_shared.hpp>

namespace
{
  using namespace Parameters;

  const IntType RADIX = 10;

  BOOST_STATIC_ASSERT(1 == sizeof(DataType::value_type));

  inline bool DoTest(const String::const_iterator it, const String::const_iterator lim, int(*Fun)(int))
  {
    return lim == std::find_if(it, lim, std::not1(std::ptr_fun(Fun)));
  }

  inline bool IsData(const String& str)
  {
    return str.size() >= 3 && DATA_PREFIX == *str.begin() && 0 == (str.size() - 1) % 2 &&
      DoTest(str.begin() + 1, str.end(), &std::isxdigit);
  }

  inline uint8_t FromHex(Char val)
  {
    assert(std::isxdigit(val));
    return val >= 'A' ? val - 'A' + 10 : val - '0';
  }

  inline void DataFromString(const String& val, DataType& res)
  {
    res.resize((val.size() - 1) / 2);
    String::const_iterator src = val.begin();
    for (DataType::iterator it = res.begin(), lim = res.end(); it != lim; ++it)
    {
      const DataType::value_type highNibble = FromHex(*++src);
      const DataType::value_type lowNibble = FromHex(*++src);
      *it = highNibble * 16 | lowNibble;
    }
  }

  inline Char ToHex(uint_t val)
  {
    assert(val < 16);
    return static_cast<Char>(val >= 10 ? val + 'A' - 10 : val + '0');
  }

  inline String DataToString(const DataType& dmp)
  {
    String res(dmp.size() * 2 + 1, DATA_PREFIX);
    String::iterator dstit = res.begin();
    for (DataType::const_iterator srcit = dmp.begin(), srclim = dmp.end(); srcit != srclim; ++srcit)
    {
      const DataType::value_type val = *srcit;
      *++dstit = ToHex(val >> 4);
      *++dstit = ToHex(val & 15);
    }
    return res;
  }

  inline bool IsInteger(const String& str)
  {
    return !str.empty() &&
      DoTest(str.begin() + (*str.begin() == '-' || *str.begin() == '+' ? 1 : 0), str.end(), &std::isdigit);
  }

  inline IntType IntegerFromString(const String& val)
  {
    IntType res = 0;
    String::const_iterator it = val.begin();
    const bool negate = *it == '-';
    if (negate || *it == '+')
    {
      ++it;
    }
    for (String::const_iterator lim = val.end(); it != lim; ++it)
    {
      res *= RADIX;
      res += *it - '0';
    }
    return negate ? -res : res;
  }

  inline String IntegerToString(IntType var)
  {
    //integer may be so long, so it's better to convert here
    String res;
    const bool negate = var < 0;

    if (negate)
    {
      var =- var;
    }
    do
    {
      res += ToHex(static_cast<uint_t>(var % RADIX));
    }
    while (var /= RADIX);

    if (negate)
    {
      res += '-';
    }
    return String(res.rbegin(), res.rend());
  }

  inline bool IsQuoted(const String& str)
  {
    return !str.empty() && STRING_QUOTE == *str.begin()  && STRING_QUOTE == *str.rbegin();
  }
                    
  inline StringType StringFromString(const String& val)
  {
    if (IsQuoted(val))
    {
      return StringType(val.begin() + 1, val.end() - 1);
    }
    return val;
  }

  inline String StringToString(const StringType& str)
  {
    if (IsData(str) || IsInteger(str) || IsQuoted(str))
    {
      String res = String(1, STRING_QUOTE);
      res += str;
      res += STRING_QUOTE;
      return res;
    }
    return str;
  }

  template<class T>
  bool FindByName(const std::map<NameType, T>& map, const NameType& name, T& res)
  {
    const typename std::map<NameType, T>::const_iterator it = map.find(name);
    return it != map.end()
      ? (res = it->second, true)
      : false;
  }

  class ContainerImpl : public Container
  {
  public:
    ContainerImpl()
    {
    }

    //accessor virtuals
    virtual bool FindIntValue(const NameType& name, IntType& val) const
    {
      return FindByName(Integers, name, val);
    }

    virtual bool FindStringValue(const NameType& name, StringType& val) const
    {
      return FindByName(Strings, name, val);
    }

    virtual bool FindDataValue(const NameType& name, DataType& val) const
    {
      return FindByName(Datas, name, val);
    }

    virtual void Process(Visitor& visitor) const
    {
      std::for_each(Integers.begin(), Integers.end(), boost::bind(&Visitor::SetIntValue, &visitor,
        boost::bind(&IntegerMap::value_type::first, _1), boost::bind(&IntegerMap::value_type::second, _1)));
      std::for_each(Strings.begin(), Strings.end(), boost::bind(&Visitor::SetStringValue, &visitor,
        boost::bind(&StringMap::value_type::first, _1), boost::bind(&StringMap::value_type::second, _1)));
      std::for_each(Datas.begin(), Datas.end(), boost::bind(&Visitor::SetDataValue, &visitor,
        boost::bind(&DataMap::value_type::first, _1), boost::bind(&DataMap::value_type::second, _1)));
    }

    //modifier virtuals
    virtual void SetIntValue(const NameType& name, IntType val)
    {
      Integers[name] = val;
    }

    virtual void SetStringValue(const NameType& name, const StringType& val)
    {
      Strings[name] = val;
    }

    virtual void SetDataValue(const NameType& name, const DataType& val)
    {
      Datas[name] = val;
    }

  private:
    typedef std::map<NameType, IntType> IntegerMap;
    typedef std::map<NameType, StringType> StringMap;
    typedef std::map<NameType, DataType> DataMap;
    IntegerMap Integers;
    StringMap Strings;
    DataMap Datas;
  };

  class MergedAccessor : public Accessor
  {
    class MergedVisitor : public Visitor
    {
    public:
      explicit MergedVisitor(Visitor& delegate)
        : Delegate(delegate)
      {
      }

      virtual void SetIntValue(const NameType& name, IntType val)
      {
        if (DoneIntegers.insert(name).second)
        {
          return Delegate.SetIntValue(name, val);
        }
      }

      virtual void SetStringValue(const NameType& name, const StringType& val)
      {
        if (DoneStrings.insert(name).second)
        {
          return Delegate.SetStringValue(name, val);
        }
      }

      virtual void SetDataValue(const NameType& name, const DataType& val)
      {
        if (DoneDatas.insert(name).second)
        {
          return Delegate.SetDataValue(name, val);
        }
      }
    private:
      Visitor& Delegate;
      std::set<NameType> DoneIntegers;
      std::set<NameType> DoneStrings;
      std::set<NameType> DoneDatas;
    };
  public:
    MergedAccessor(Accessor::Ptr first, Accessor::Ptr second)
      : First(first)
      , Second(second)
    {
    }

    virtual bool FindIntValue(const NameType& name, IntType& val) const
    {
      return First->FindIntValue(name, val) || Second->FindIntValue(name, val);
    }

    virtual bool FindStringValue(const NameType& name, StringType& val) const
    {
      return First->FindStringValue(name, val) || Second->FindStringValue(name, val);
    }

    virtual bool FindDataValue(const NameType& name, DataType& val) const
    {
      return First->FindDataValue(name, val) || Second->FindDataValue(name, val);
    }

    virtual void Process(Visitor& visitor) const
    {
      MergedVisitor mergedVisitor(visitor);
      First->Process(mergedVisitor);
      Second->Process(mergedVisitor);
    }
  private:
    const Accessor::Ptr First;
    const Accessor::Ptr Second;
  };

  class StringConvertor : public StringMap
                        , public Visitor
  {
  public:
    virtual void SetIntValue(const NameType& name, IntType val)
    {
      insert(value_type(name, IntegerToString(val)));
    }

    virtual void SetStringValue(const NameType& name, const StringType& val)
    {
      insert(value_type(name, StringToString(val)));
    }

    virtual void SetDataValue(const NameType& name, const DataType& val)
    {
      insert(value_type(name, DataToString(val)));
    }
  };

  void SetValue(Modifier& modifier, const StringMap::value_type& pair)
  {
    const NameType& name = pair.first;
    const String& val = pair.second;
    if (IsData(val))
    {
      DataType res;
      DataFromString(val, res);
      modifier.SetDataValue(name, res);
    }
    else if (IsInteger(val))
    {
      const IntType res = IntegerFromString(val);
      modifier.SetIntValue(name, res);
    }
    else
    {
      modifier.SetStringValue(name, StringFromString(val));
    }
  }
}

namespace Parameters
{
  String ConvertToString(IntType val)
  {
    return IntegerToString(val);
  }

  String ConvertToString(StringType val)
  {
    return StringToString(val);
  }

  String ConvertToString(DataType val)
  {
    return DataToString(val);
  }

  void ParseStringMap(const StringMap& map, Modifier& modifier)
  {
    std::for_each(map.begin(), map.end(), 
      boost::bind(&SetValue, boost::ref(modifier), _1));
  }

  Accessor::Ptr CreateMergedAccessor(Accessor::Ptr first, Accessor::Ptr second)
  {
    return boost::make_shared<MergedAccessor>(first, second);
  }

  Container::Ptr Container::Create()
  {
    return boost::make_shared<ContainerImpl>();
  }

  void Convert(const Accessor& ac, StringMap& strings)
  {
    StringConvertor cnv;
    ac.Process(cnv);
    cnv.swap(strings);
  }
}
