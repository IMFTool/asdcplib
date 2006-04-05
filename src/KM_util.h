/*
Copyright (c) 2005-2006, John Hurst
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
  /*! \file    KM_util.h
    \version $Id$
    \brief   Utility functions
  */

#ifndef _KM_UTIL_H_
#define _KM_UTIL_H_

#include <KM_memio.h>
#include <KM_error.h>
#include <string.h>
#include <string>

namespace Kumu
{

  template <class T, int SIZE = 16>
    class IntPrinter : public std::string
  {
    protected:
    IntPrinter();
    char m_strbuf[SIZE];
    
    public:
    inline const char* c_str() { return m_strbuf; }
    
    IntPrinter(const char* format, T value) {
      snprintf(m_strbuf, SIZE, format, value);
    }
  };

  struct i8Printer : public IntPrinter<i8_t> {
    i8Printer(i8_t value) : IntPrinter<i8_t>("%hd", value) {}
  };

  struct ui8Printer : public IntPrinter<ui8_t> {
    ui8Printer(ui8_t value) : IntPrinter<ui8_t>("%hu", value) {}
  };

  struct i16Printer : public IntPrinter<i16_t> {
    i16Printer(i16_t value) : IntPrinter<i16_t>("%hd", value) {}
  };

  struct ui16Printer : public IntPrinter<ui16_t> {
    ui16Printer(ui16_t value) : IntPrinter<ui16_t>("%hu", value) {}
  };

  struct i32Printer : public IntPrinter<i32_t> {
    i32Printer(i32_t value) : IntPrinter<i32_t>("%d", value) {}
  };

  struct ui32Printer : public IntPrinter<ui32_t> {
    ui32Printer(ui32_t value) : IntPrinter<ui32_t>("%u", value) {}
  };

#ifdef KM_WIN32
  struct i64Printer : public IntPrinter<i64_t, 32> {
    i64Printer(i64_t value) : IntPrinter<i64_t, 32>("%I64d", value) {}
  };

  struct ui64Printer : public IntPrinter<ui64_t, 32> {
    ui64Printer(ui64_t value) : IntPrinter<ui64_t, 32>("%I64u", value) {}
  };
#else
  struct i64Printer : public IntPrinter<i64_t, 32> {
    i64Printer(i64_t value) : IntPrinter<i64_t, 32>("%qd", value) {}
  };

  struct ui64Printer : public IntPrinter<ui64_t, 32> {
    ui64Printer(ui64_t value) : IntPrinter<ui64_t, 32>("%qu", value) {}
  };
#endif

  // Convert NULL-terminated UTF-8 hexadecimal string to binary, returns 0 if
  // the binary buffer was large enough to hold the result. The output parameter
  // 'char_count' will contain the length of the converted string. If the output
  // buffer is too small or any of the pointer arguments are NULL, the subroutine
  // will return -1 and set 'char_count' to the required buffer size. No data will
  // be written to 'buf' if the subroutine fails.
  i32_t       hex2bin(const char* str, byte_t* buf, ui32_t buf_len, ui32_t* char_count);

  // Convert a binary string to NULL-terminated UTF-8 hexadecimal, returns the buffer
  // if the binary buffer was large enough to hold the result. If the output buffer
  // is too small or any of the pointer arguments are NULL, the subroutine will
  // return 0.
  //
  const char* bin2hex(const byte_t* bin_buf, ui32_t bin_len, char* str_buf, ui32_t str_len);

  const char* bin2UUIDhex(const byte_t* bin_buf, ui32_t bin_len, char* str_buf, ui32_t str_len);

  // same as above for base64 text
  i32_t       base64decode(const char* str, byte_t* buf, ui32_t buf_len, ui32_t* char_count);
  const char* base64encode(const byte_t* bin_buf, ui32_t bin_len, char* str_buf, ui32_t str_len);

  // returns the length of a Base64 encoding of a buffer of the given length
  inline ui32_t base64_encode_length(ui32_t length) {
    while ( ( length % 3 ) != 0 )
      length++;

    return ( length / 3 ) * 4;
  }

  // print buffer contents to a stream as hexadecimal values in numbered
  // rows of 16-bytes each.
  //
  void hexdump(const byte_t* buf, ui32_t dump_len, FILE* stream = 0);

  // Return the length in bytes of a BER encoded value
  inline ui32_t BER_length(const byte_t* buf)
    {
      if ( buf == 0 || (*buf & 0xf0) != 0x80 )
	return 0;

      return (*buf & 0x0f) + 1;
    }

  // read a BER value
  bool read_BER(const byte_t* buf, ui64_t* val);

  // decode a ber value and compare it to a test value
  bool read_test_BER(byte_t **buf, ui64_t test_value);

  // create BER encoding of integer value
  bool write_BER(byte_t* buf, ui64_t val, ui32_t ber_len = 0);

  //----------------------------------------------------------------
  //

  class IArchive
    {
    public:
      virtual ~IArchive(){}
      virtual bool HasValue() const = 0;
      virtual bool Archive(MemIOWriter* Writer) const = 0;
      virtual bool Unarchive(MemIOReader* Reader) = 0;
    };


  //
  // the base of all identifier classes
  template <ui32_t SIZE>
    class Identifier : public IArchive
    {
    protected:
      bool   m_HasValue;
      byte_t m_Value[SIZE];

    public:
      Identifier() : m_HasValue(false) { memset(m_Value, 0, SIZE); }
      Identifier(const byte_t* value) : m_HasValue(true)   { memcpy(m_Value, value, SIZE); }
      Identifier(const Identifier& rhs) : m_HasValue(true) { memcpy(m_Value, rhs.m_Value, SIZE); }
      virtual ~Identifier() {}

      const Identifier& operator=(const Identifier& rhs) {
	m_HasValue = true;
	memcpy(m_Value, rhs.m_Value, SIZE);
	return *this;
      }

      inline void Set(const byte_t* value) { m_HasValue = true; memcpy(m_Value, value, SIZE); }
      inline const byte_t* Value() const { return m_Value; }
      inline ui32_t Size() const { return SIZE; }

      inline bool operator<(const Identifier& rhs) const
	{
	  ui32_t test_size = xmin(rhs.Size(), SIZE);
	  for ( ui32_t i = 0; i < test_size; i++ )
	    {
	      if ( m_Value[i] != rhs.m_Value[i] )
		return m_Value[i] < rhs.m_Value[i];
	    }

	  return false;
	}

      inline bool operator==(const Identifier& rhs) const
	{
	  if ( rhs.Size() != SIZE ) return false;
	  return ( memcmp(m_Value, rhs.m_Value, SIZE) == 0 );
	}

      inline bool operator!=(const Identifier& rhs) const
	{
	  if ( rhs.Size() != SIZE ) return true;
	  return ( memcmp(m_Value, rhs.m_Value, SIZE) != 0 );
	}

      inline bool DecodeHex(const char* str)
	{
	  ui32_t char_count;
	  if ( hex2bin(str, m_Value, SIZE, &char_count) != 0 )
	    return false;

	  m_HasValue = true;
	  return true;
	}

      inline const char* EncodeHex(char* buf, ui32_t buf_len) const
	{
	  return bin2hex(m_Value, SIZE, buf, buf_len);
	}

      inline const char* EncodeString(char* str_buf, ui32_t buf_len) const {
	return EncodeHex(str_buf, buf_len);
      }

      inline bool DecodeBase64(const char* str)
 	{
	  ui32_t char_count;
	  if ( base64decode(str, m_Value, SIZE, &char_count) != 0 )
	    return false;

	  m_HasValue = true;
	  return true;
	}

      inline const char* EncodeBase64(char* buf, ui32_t buf_len) const
	{
	  return base64encode(m_Value, SIZE, buf, buf_len);
	}

      inline virtual bool HasValue() const { return m_HasValue; }

      inline virtual bool Unarchive(Kumu::MemIOReader* Reader) {
	if ( ! Reader->ReadRaw(m_Value, SIZE) ) return false;
	m_HasValue = true;
	return true;
      }

      inline virtual bool Archive(Kumu::MemIOWriter* Writer) const {
	return Writer->WriteRaw(m_Value, SIZE);
      }
    };

  
  // UUID
  //
  const ui32_t UUID_Length = 16;
  class UUID : public Identifier<UUID_Length>
    {
    public:
      UUID() {}
      UUID(const byte_t* value) : Identifier<UUID_Length>(value) {}
      UUID(const UUID& rhs) : Identifier<UUID_Length>(rhs) {}
      virtual ~UUID() {}
      
      inline const char* EncodeHex(char* buf, ui32_t buf_len) const {
	return bin2UUIDhex(m_Value, Size(), buf, buf_len);
      }
    };
  
  void GenRandomUUID(byte_t* buf);
  void GenRandomValue(UUID&);
  
  // a self-wiping key container
  //
  const ui32_t SymmetricKey_Length = 16;
  const byte_t NilKey[SymmetricKey_Length] = {
    0xfa, 0xce, 0xfa, 0xce, 0xfa, 0xce, 0xfa, 0xce,
    0xfa, 0xce, 0xfa, 0xce, 0xfa, 0xce, 0xfa, 0xce
  };

  class SymmetricKey : public Identifier<SymmetricKey_Length>
    {
    public:
      SymmetricKey() {}
      SymmetricKey(const byte_t* value) : Identifier<SymmetricKey_Length>(value) {}
      SymmetricKey(const UUID& rhs) : Identifier<SymmetricKey_Length>(rhs) {}
      virtual ~SymmetricKey() { memcpy(m_Value, NilKey, 16); m_HasValue = false; }
    };

  void GenRandomValue(SymmetricKey&);

  //
  // 2004-05-01T13:20:00-00:00
  const ui32_t DateTimeLen = 25; //  the number of chars in the xs:dateTime format (sans milliseconds)

  // UTC time+date representation
  class Timestamp : public IArchive
    {
    public:
      ui16_t Year;
      ui8_t  Month;
      ui8_t  Day;
      ui8_t  Hour;
      ui8_t  Minute;
      ui8_t  Second;

      Timestamp();
      Timestamp(const Timestamp& rhs);
      Timestamp(const char* datestr);
      virtual ~Timestamp();

      const Timestamp& operator=(const Timestamp& rhs);
      bool operator<(const Timestamp& rhs) const;
      bool operator==(const Timestamp& rhs) const;
      bool operator!=(const Timestamp& rhs) const;

      // Write the timestamp value to the given buffer in the form 2004-05-01T13:20:00-00:00
      // returns 0 if the buffer is smaller than DateTimeLen
      const char* EncodeString(char* str_buf, ui32_t buf_len) const;

      // decode and set value from string formatted by EncodeString
      Result_t    SetFromString(const char* datestr);

      // add the given number of days or hours to the timestamp value. Values less than zero
      // will cause the value to decrease
      void AddDays(i32_t);
      void AddHours(i32_t);

      // Read and write the timestamp value as a byte string
      virtual bool HasValue() const;
      virtual bool Archive(MemIOWriter* Writer) const;
      virtual bool Unarchive(MemIOReader* Reader);
    };

  //
  class ByteString
    {
      KM_NO_COPY_CONSTRUCT(ByteString);
	
    protected:
      byte_t* m_Data;          // pointer to memory area containing frame data
      ui32_t  m_Capacity;      // size of memory area pointed to by m_Data
      ui32_t  m_Length;        // length of byte string in memory area pointed to by m_Data
	
    public:
      ByteString();
      ByteString(ui32_t cap);
      virtual ~ByteString();

      // Sets the size of the internally allocated buffer.
      // Resets content Size to zero.
      Result_t Capacity(ui32_t cap);

      Result_t Append(const ByteString&);
      Result_t Append(const byte_t* buf, ui32_t buf_len);
	
      // returns the size of the buffer
      inline ui32_t  Capacity() const { return m_Capacity; }

      // returns a const pointer to the essence data
      inline const byte_t* RoData() const { return m_Data; }
	
      // returns a non-const pointer to the essence data
      inline byte_t* Data() { return m_Data; }
	
      // set the length of the buffer's contents
      inline ui32_t  Length(ui32_t l) { return m_Length = l; }
	
      // returns the length of the buffer's contents
      inline ui32_t  Length() const { return m_Length; }

      // copy the given data into the ByteString, set Length value.
      // Returns error if the ByteString is too small.
      Result_t Set(const byte_t* buf, ui32_t buf_len);
    };

} // namespace Kumu


#endif // _KM_UTIL_H_

//
// end KM_util.h
//