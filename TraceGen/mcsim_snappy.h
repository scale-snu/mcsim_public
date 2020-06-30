/*
 * Copyright 2011 Martin Gieseking <martin.gieseking@uos.de>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Plain C interface (a wrapper around the C++ implementation).
 */

#ifndef MCSIM_SNAPPY_H_
#define MCSIM_SNAPPY_H_

#include <stddef.h>
#include <string>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>

#define PREDICT_FALSE(x) x
#define PREDICT_TRUE(x) x

#ifdef ARRAYSIZE
#undef ARRAYSIZE
#endif
#define ARRAYSIZE(a) (sizeof(a) / sizeof(*(a)))

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)

namespace snappy {

  typedef int8_t int8;
  typedef uint8_t uint8;
  typedef int16_t int16;
  typedef uint16_t uint16;
  typedef int32_t int32;
  typedef uint32_t uint32;
  typedef int64_t int64;
  typedef uint64_t uint64;
  typedef const char* EightBytesReference;

  enum {
    LITERAL = 0,
    COPY_1_BYTE_OFFSET = 1,    
    COPY_2_BYTE_OFFSET = 2,
    COPY_4_BYTE_OFFSET = 3
  };

  const int kMaxIncrementCopyOverflow = 10;
  static const int kMaximumTagLength = 5; 
  static const int kMaxHashTableBits = 14;
  static const size_t kMaxHashTableSize = 1 << kMaxHashTableBits;
  static const int kBlockLog = 16;
  static const size_t kBlockSize = 1 << kBlockLog;
  static const uint32 kuint32max = static_cast<uint32>(0xFFFFFFFF);

  inline uint16 UNALIGNED_LOAD16(const void *p) {
    uint16 t;
    memcpy(&t, p, sizeof t);
    return t;
  }

  inline uint32 UNALIGNED_LOAD32(const void *p) {
    uint32 t;
    memcpy(&t, p, sizeof t);
    return t;
  }

  inline uint64 UNALIGNED_LOAD64(const void *p) {
    uint64 t;
    memcpy(&t, p, sizeof t);
    return t;
  }

  inline void UNALIGNED_STORE16(void *p, uint16 v) {
    memcpy(p, &v, sizeof v);
  }

  inline void UNALIGNED_STORE32(void *p, uint32 v) {
    memcpy(p, &v, sizeof v);
  }

  inline void UNALIGNED_STORE64(void *p, uint64 v) {
    memcpy(p, &v, sizeof v);
  }

  static inline uint32 HashBytes(uint32 bytes, int shift) {
    uint32 kMul = 0x1e35a7bd;
    return (bytes * kMul) >> shift;
  }

  static inline uint32 Hash(const char* p, int shift) {
    return HashBytes(UNALIGNED_LOAD32(p), shift);
  }

  inline void UnalignedCopy64(const void *src, void *dst) {
    if (sizeof(void *) == 8) {
      UNALIGNED_STORE64(dst, UNALIGNED_LOAD64(src));
    } else {
      const char *src_char = reinterpret_cast<const char *>(src);
      char *dst_char = reinterpret_cast<char *>(dst);
      UNALIGNED_STORE32(dst_char, UNALIGNED_LOAD32(src_char));
      UNALIGNED_STORE32(dst_char + 4, UNALIGNED_LOAD32(src_char + 4));
    }
  }

  class Source {
    public:
      Source() { }
      virtual ~Source();

      virtual size_t Available() const = 0;
      virtual const char* Peek(size_t* len) = 0;
      virtual void Skip(size_t n) = 0;

    private:
      Source(const Source&);
      void operator=(const Source&);
  };

  class Sink {
    public:
      Sink() { }
      virtual ~Sink();
      virtual void Append(const char* bytes, size_t n) = 0;
      virtual char* GetAppendBuffer(size_t length, char* scratch);

    private:
      Sink(const Sink&);
      void operator=(const Sink&);
  };

  class UncheckedByteArraySink : public Sink {
    public:
      explicit UncheckedByteArraySink(char* dest) : dest_(dest) { }
      virtual ~UncheckedByteArraySink();
      virtual void Append(const char* data, size_t n);
      virtual char* GetAppendBuffer(size_t len, char* scratch);
      char* CurrentDestination() const { return dest_; }

    private:
      char* dest_;
  };

  class ByteArraySource : public Source {
    public:
      ByteArraySource(const char* p, size_t n) : ptr_(p), left_(n) { }
      virtual ~ByteArraySource();
      virtual size_t Available() const;
      virtual const char* Peek(size_t* len);
      virtual void Skip(size_t n);

    private:
      const char* ptr_;
      size_t left_;
  };

  class LittleEndian {
    public:
      static uint32 FromHost32(uint32 x) { return x; }
      static uint32 ToHost32(uint32 x) { return x; }
      static uint16 FromHost16(uint16 x) { return x; }
      static uint16 ToHost16(uint16 x) { return x; }
      static bool IsLittleEndian() { return true; }

      static uint32 Load32(const void *p) {
        return ToHost32(UNALIGNED_LOAD32(p));
      }

      static void Store16(void *p, uint16 v) {
        UNALIGNED_STORE16(p, FromHost16(v));
      }

      static void Store32(void *p, uint32 v) {
        UNALIGNED_STORE32(p, FromHost32(v));
      }
  };


  size_t Compress(Source* source, Sink* sink);
  void RawCompress(const char* input, size_t input_length, char* compressed, size_t* compressed_length);
  bool RawUncompress(const char* compressed, size_t compressed_length, char* uncompressed);
  bool RawUncompress(Source* compressed, char* uncompressed);
  size_t MaxCompressedLength(size_t source_bytes);


  /* 2. Class Varint */
  class Varint {
    public:
      static const int kMax32 = 5;
      static char* Encode32(char* ptr, uint32 v);
  };

  namespace internal {
    class Bits {
      public:
        static int Log2Floor(uint32 n);
        static int FindLSBSetNonZero(uint32 n);

      private:
        DISALLOW_COPY_AND_ASSIGN(Bits);
    };

    inline int Bits::Log2Floor(uint32 n) {
      if (n == 0)
        return -1;

      int log = 0;
      uint32 value = n;

      for (int i = 4; i >= 0; --i) {
        int shift = (1 << i);
        uint32 x = value >> shift;
        if (x != 0) {
          value = x;
          log += shift;
        }
      }

      assert(value == 1);
      return log;
    }

    inline int Bits::FindLSBSetNonZero(uint32 n) {
      int rc = 31;

      for (int i = 4, shift = 1 << 4; i >= 0; --i) {
        const uint32 x = n << shift;
        if (x != 0) {
          n = x;
          rc -= shift;
        }
        shift >>= 1;
      }

      return rc;
    }

    class WorkingMemory {
      public:
        WorkingMemory() : large_table_(NULL) { }
        ~WorkingMemory() { delete[] large_table_; }
        uint16* GetHashTable(size_t input_size, int* table_size);

      private:
        uint16 small_table_[1<<10];    // 2KB
        uint16* large_table_;          // Allocated only when needed
        DISALLOW_COPY_AND_ASSIGN(WorkingMemory);
    };

    static inline int FindMatchLength(const char* s1,
        const char* s2,
        const char* s2_limit) {
      assert(s2_limit >= s2);
      int matched = 0;

      while (s2 <= s2_limit - 4 &&
          UNALIGNED_LOAD32(s2) == UNALIGNED_LOAD32(s1 + matched)) {
        s2 += 4;
        matched += 4;
      }

      if (LittleEndian::IsLittleEndian() && s2 <= s2_limit - 4) {
        uint32 x = UNALIGNED_LOAD32(s2) ^ UNALIGNED_LOAD32(s1 + matched);
        int matching_bits = Bits::FindLSBSetNonZero(x);
        matched += matching_bits >> 3;
      } else {
        while ((s2 < s2_limit) && (s1[matched] == *s2)) {
          ++s2;
          ++matched;
        }
      }

      return matched;
    }

  }  // end namespace internal
}  // end namespace snappy

#endif // MCSIM_SNAPPY_H_
