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

namespace snappy {

  typedef int8_t int8;
  typedef uint8_t uint8;
  typedef int16_t int16;
  typedef uint16_t uint16;
  typedef int32_t int32;
  typedef uint32_t uint32;
  typedef int64_t int64;
  typedef uint64_t uint64;

  enum {
    LITERAL = 0,
    COPY_1_BYTE_OFFSET = 1,    
    COPY_2_BYTE_OFFSET = 2,
    COPY_4_BYTE_OFFSET = 3
  };

  const int kMaxIncrementCopyOverflow = 10;
  static const int kMaximumTagLength = 5; 

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

  /* RawUncompress Class */
  /* 1. Class Source */
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

  /* 2. Class ByteArraySource */
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

  /* 3. Class LittleEndian */
  class LittleEndian {
    public:
      static uint32 FromHost32(uint32 x) { return x; }
      static uint32 ToHost32(uint32 x) { return x; }
      static bool IsLittleEndian() { return true; }

      static uint32 Load32(const void *p) {
        return ToHost32(UNALIGNED_LOAD32(p));
      }

      static void Store32(void *p, uint32 v) {
        UNALIGNED_STORE32(p, FromHost32(v));
      }
  };

  bool RawUncompress(const char* compressed, size_t compressed_length, char* uncompressed);
  bool RawUncompress(Source* compressed, char* uncompressed);
  size_t MaxCompressedLength(size_t source_bytes);

}  // end namespace snappy

#endif // MCSIM_SNAPPY_H_
