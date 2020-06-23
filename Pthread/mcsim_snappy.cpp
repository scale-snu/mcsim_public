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

#include <string.h>
#include <stdio.h>
#include <algorithm>
#include <vector>

#include "mcsim_snappy.h"

using namespace std;

namespace snappy {

  static const uint32 wordmask[] = {
    0u, 0xffu, 0xffffu, 0xffffffu, 0xffffffffu
  };

  static const uint16 char_table[256] = {
    0x0001, 0x0804, 0x1001, 0x2001, 0x0002, 0x0805, 0x1002, 0x2002,
    0x0003, 0x0806, 0x1003, 0x2003, 0x0004, 0x0807, 0x1004, 0x2004,
    0x0005, 0x0808, 0x1005, 0x2005, 0x0006, 0x0809, 0x1006, 0x2006,
    0x0007, 0x080a, 0x1007, 0x2007, 0x0008, 0x080b, 0x1008, 0x2008,
    0x0009, 0x0904, 0x1009, 0x2009, 0x000a, 0x0905, 0x100a, 0x200a,
    0x000b, 0x0906, 0x100b, 0x200b, 0x000c, 0x0907, 0x100c, 0x200c,
    0x000d, 0x0908, 0x100d, 0x200d, 0x000e, 0x0909, 0x100e, 0x200e,
    0x000f, 0x090a, 0x100f, 0x200f, 0x0010, 0x090b, 0x1010, 0x2010,
    0x0011, 0x0a04, 0x1011, 0x2011, 0x0012, 0x0a05, 0x1012, 0x2012,
    0x0013, 0x0a06, 0x1013, 0x2013, 0x0014, 0x0a07, 0x1014, 0x2014,
    0x0015, 0x0a08, 0x1015, 0x2015, 0x0016, 0x0a09, 0x1016, 0x2016,
    0x0017, 0x0a0a, 0x1017, 0x2017, 0x0018, 0x0a0b, 0x1018, 0x2018,
    0x0019, 0x0b04, 0x1019, 0x2019, 0x001a, 0x0b05, 0x101a, 0x201a,
    0x001b, 0x0b06, 0x101b, 0x201b, 0x001c, 0x0b07, 0x101c, 0x201c,
    0x001d, 0x0b08, 0x101d, 0x201d, 0x001e, 0x0b09, 0x101e, 0x201e,
    0x001f, 0x0b0a, 0x101f, 0x201f, 0x0020, 0x0b0b, 0x1020, 0x2020,
    0x0021, 0x0c04, 0x1021, 0x2021, 0x0022, 0x0c05, 0x1022, 0x2022,
    0x0023, 0x0c06, 0x1023, 0x2023, 0x0024, 0x0c07, 0x1024, 0x2024,
    0x0025, 0x0c08, 0x1025, 0x2025, 0x0026, 0x0c09, 0x1026, 0x2026,
    0x0027, 0x0c0a, 0x1027, 0x2027, 0x0028, 0x0c0b, 0x1028, 0x2028,
    0x0029, 0x0d04, 0x1029, 0x2029, 0x002a, 0x0d05, 0x102a, 0x202a,
    0x002b, 0x0d06, 0x102b, 0x202b, 0x002c, 0x0d07, 0x102c, 0x202c,
    0x002d, 0x0d08, 0x102d, 0x202d, 0x002e, 0x0d09, 0x102e, 0x202e,
    0x002f, 0x0d0a, 0x102f, 0x202f, 0x0030, 0x0d0b, 0x1030, 0x2030,
    0x0031, 0x0e04, 0x1031, 0x2031, 0x0032, 0x0e05, 0x1032, 0x2032,
    0x0033, 0x0e06, 0x1033, 0x2033, 0x0034, 0x0e07, 0x1034, 0x2034,
    0x0035, 0x0e08, 0x1035, 0x2035, 0x0036, 0x0e09, 0x1036, 0x2036,
    0x0037, 0x0e0a, 0x1037, 0x2037, 0x0038, 0x0e0b, 0x1038, 0x2038,
    0x0039, 0x0f04, 0x1039, 0x2039, 0x003a, 0x0f05, 0x103a, 0x203a,
    0x003b, 0x0f06, 0x103b, 0x203b, 0x003c, 0x0f07, 0x103c, 0x203c,
    0x0801, 0x0f08, 0x103d, 0x203d, 0x1001, 0x0f09, 0x103e, 0x203e,
    0x1801, 0x0f0a, 0x103f, 0x203f, 0x2001, 0x0f0b, 0x1040, 0x2040
  };

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

  static inline void IncrementalCopy(const char* src, char* op, ssize_t len) {
    assert(len > 0);
    do {
      *op++ = *src++;
    } while (--len > 0);
  }

  inline void IncrementalCopyFastPath(const char* src, char* op, ssize_t len) {
    while (PREDICT_FALSE(op - src < 8)) {
      UnalignedCopy64(src, op);
      len -= op - src;
      op += op - src;
    }
    while (len > 0) {
      UnalignedCopy64(src, op);
      src += 8;
      op += 8;
      len -= 8;
    }
  }


  /* RawUncompress Class */
  /* 1. Class ByteArraySource */
  ByteArraySource::~ByteArraySource() { }

  size_t ByteArraySource::Available() const { return left_; }

  const char* ByteArraySource::Peek(size_t* len) {
    *len = left_;
    return ptr_;
  }

  void ByteArraySource::Skip(size_t n) {
    left_ -= n;
    ptr_ += n;
  }

  /* 2. Class Source */
  Source::~Source() { }
 
  /* 3. Class SnappyArrayWriter */
  class SnappyArrayWriter {
    private:
      char* base_;
      char* op_;
      char* op_limit_;

    public:
      inline explicit SnappyArrayWriter(char* dst)
        : base_(dst),
        op_(dst),
        op_limit_(dst) {
        }

      inline void SetExpectedLength(size_t len) {
        op_limit_ = op_ + len;
      }

      inline bool CheckLength() const {
        return op_ == op_limit_;
      }

      inline bool Append(const char* ip, size_t len) {
        char* op = op_;
        const size_t space_left = op_limit_ - op;
        if (space_left < len) {
          return false;
        }
        memcpy(op, ip, len);
        op_ = op + len;
        return true;
      }

      inline bool TryFastAppend(const char* ip, size_t available, size_t len) {
        char* op = op_;
        const size_t space_left = op_limit_ - op;
        if (len <= 16 && available >= 16 + kMaximumTagLength && space_left >= 16) {
          // Fast path, used for the majority (about 95%) of invocations.
          UnalignedCopy64(ip, op);
          UnalignedCopy64(ip + 8, op + 8);
          op_ = op + len;
          return true;
        } else {
          return false;
        }
      }

      inline bool AppendFromSelf(size_t offset, size_t len) {
        char* op = op_;
        const size_t space_left = op_limit_ - op;

        // Check if we try to append from before the start of the buffer.
        // Normally this would just be a check for "produced < offset",
        // but "produced <= offset - 1u" is equivalent for every case
        // except the one where offset==0, where the right side will wrap around
        // to a very big number. This is convenient, as offset==0 is another
        // invalid case that we also want to catch, so that we do not go
        // into an infinite loop.
        assert(op >= base_);
        size_t produced = op - base_;
        if (produced <= offset - 1u) {
          return false;
        }
        if (len <= 16 && offset >= 8 && space_left >= 16) {
          // Fast path, used for the majority (70-80%) of dynamic invocations.
          UnalignedCopy64(op - offset, op);
          UnalignedCopy64(op - offset + 8, op + 8);
        } else {
          if (space_left >= len + kMaxIncrementCopyOverflow) {
            IncrementalCopyFastPath(op - offset, op, len);
          } else {
            if (space_left < len) {
              return false;
            }
            IncrementalCopy(op - offset, op, len);
          }
        }

        op_ = op + len;
        return true;
      }
      inline size_t Produced() const {
        return op_ - base_;
      }
      inline void Flush() {}
  };


  /* 4. Class Snappydecompressor */
  class SnappyDecompressor {
    private:
      Source*       reader_;
      const char*   ip_;
      const char*   ip_limit_;
      uint32        peeked_;
      bool          eof_;
      char          scratch_[kMaximumTagLength];

      bool RefillTag();

    public:
      explicit SnappyDecompressor(Source* reader)
        : reader_(reader),
        ip_(NULL),
        ip_limit_(NULL),
        peeked_(0),
        eof_(false) {
        }

      ~SnappyDecompressor() {
        reader_->Skip(peeked_);
      }

      bool eof() const {
        return eof_;
      }

      bool ReadUncompressedLength(uint32* result) {
        assert(ip_ == NULL);       // Must not have read anything yet
        *result = 0;
        uint32 shift = 0;
        while (true) {
          if (shift >= 32) return false;
          size_t n;
          const char* ip = reader_->Peek(&n);
          if (n == 0) return false;
          const unsigned char c = *(reinterpret_cast<const unsigned char*>(ip));
          reader_->Skip(1);
          *result |= static_cast<uint32>(c & 0x7f) << shift;
          if (c < 128) {
            break;
          }
          shift += 7;
        }
        return true;
      }

      template <class Writer>
        void DecompressAllTags(Writer* writer) {
          const char* ip = ip_;

#define MAYBE_REFILL() \
          if (ip_limit_ - ip < kMaximumTagLength) { \
            ip_ = ip; \
            if (!RefillTag()) return; \
            ip = ip_; \
          }

          MAYBE_REFILL();
          for ( ;; ) {
            const unsigned char c = *(reinterpret_cast<const unsigned char*>(ip++));

            if ((c & 0x3) == LITERAL) {
              size_t literal_length = (c >> 2) + 1u;
              if (writer->TryFastAppend(ip, ip_limit_ - ip, literal_length)) {
                assert(literal_length < 61);
                ip += literal_length;
                continue;
              }
              if (PREDICT_FALSE(literal_length >= 61)) {
                const size_t literal_length_length = literal_length - 60;
                literal_length =
                  (LittleEndian::Load32(ip) & wordmask[literal_length_length]) + 1;
                ip += literal_length_length;
              }

              size_t avail = ip_limit_ - ip;
              while (avail < literal_length) {
                if (!writer->Append(ip, avail)) return;
                literal_length -= avail;
                reader_->Skip(peeked_);
                size_t n;
                ip = reader_->Peek(&n);
                avail = n;
                peeked_ = avail;
                if (avail == 0) return;  // Premature end of input
                ip_limit_ = ip + avail;
              }
              if (!writer->Append(ip, literal_length)) {
                return;
              }
              ip += literal_length;
              MAYBE_REFILL();
            } else {
              const uint32 entry = char_table[c];
              const uint32 trailer = LittleEndian::Load32(ip) & wordmask[entry >> 11];
              const uint32 length = entry & 0xff;
              ip += entry >> 11;
              const uint32 copy_offset = entry & 0x700;
              if (!writer->AppendFromSelf(copy_offset + trailer, length)) {
                return;
              }
              MAYBE_REFILL();
            }
          }
#undef MAYBE_REFILL
        }
  };

  bool SnappyDecompressor::RefillTag() {
    const char* ip = ip_;
    if (ip == ip_limit_) {
      // Fetch a new fragment from the reader
      reader_->Skip(peeked_);   // All peeked bytes are used up
      size_t n;
      ip = reader_->Peek(&n);
      peeked_ = n;
      if (n == 0) {
        eof_ = true;
        return false;
      }
      ip_limit_ = ip + n;
    }

    // Read the tag character
    assert(ip < ip_limit_);
    const unsigned char c = *(reinterpret_cast<const unsigned char*>(ip));
    const uint32 entry = char_table[c];
    const uint32 needed = (entry >> 11) + 1;  // +1 byte for 'c'
    assert(needed <= sizeof(scratch_));

    // Read more bytes from reader if needed
    uint32 nbuf = ip_limit_ - ip;
    if (nbuf < needed) {
      // Stitch together bytes from ip and reader to form the word
      // contents.  We store the needed bytes in "scratch_".  They
      // will be consumed immediately by the caller since we do not
      // read more than we need.
      memmove(scratch_, ip, nbuf);
      reader_->Skip(peeked_);  // All peeked bytes are used up
      peeked_ = 0;
      while (nbuf < needed) {
        size_t length;
        const char* src = reader_->Peek(&length);
        if (length == 0) return false;
        uint32 to_add = min<uint32>(needed - nbuf, length);
        memcpy(scratch_ + nbuf, src, to_add);
        nbuf += to_add;
        reader_->Skip(to_add);
      }
      assert(nbuf == needed);
      ip_ = scratch_;
      ip_limit_ = scratch_ + needed;
    } else if (nbuf < kMaximumTagLength) {
      // Have enough bytes, but move into scratch_ so that we do not
      // read past end of input
      memmove(scratch_, ip, nbuf);
      reader_->Skip(peeked_);  // All peeked bytes are used up
      peeked_ = 0;
      ip_ = scratch_;
      ip_limit_ = scratch_ + nbuf;
    } else {
      // Pass pointer to buffer returned by reader_.
      ip_ = ip;
    }
    return true;
  }


  /* MaxCompressedLength Function */
  size_t MaxCompressedLength(size_t source_len) {
    return 32 + source_len + source_len/6;
  }

  /* RawUncompress Function */
  template <typename Writer>
    static bool InternalUncompressAllTags(SnappyDecompressor* decompressor,
        Writer* writer,
        uint32 uncompressed_len) {
      writer->SetExpectedLength(uncompressed_len);
      decompressor->DecompressAllTags(writer);
      writer->Flush();
      return (decompressor->eof() && writer->CheckLength());
    }

  template <typename Writer>
    static bool InternalUncompress(Source* r, Writer* writer) {
      SnappyDecompressor decompressor(r);
      uint32 uncompressed_len = 0;
      if (!decompressor.ReadUncompressedLength(&uncompressed_len)) return false;
      return InternalUncompressAllTags(&decompressor, writer, uncompressed_len);
    }

  bool RawUncompress(const char* compressed, size_t n, char* uncompressed) {
    ByteArraySource reader(compressed, n);
    return RawUncompress(&reader, uncompressed);
  }

  bool RawUncompress(Source* compressed, char* uncompressed) {
    SnappyArrayWriter output(uncompressed);
    return InternalUncompress(compressed, &output);
  }
}
