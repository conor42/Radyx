///////////////////////////////////////////////////////////////////////////////
//
// Class: FastLzma2
//        Management of solid data units and data block overlap
//
// Copyright 1998-2000, 2015-present Conor McCarthy
//
// This file is part of Radyx.
//
// Radyx is free software : you can redistribute it and / or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Radyx is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with Radyx. If not, see <http://www.gnu.org/licenses/>.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef RADYX_UNIT_COMPRESSOR_H
#define RADYX_UNIT_COMPRESSOR_H

#include "common.h"
#include "OutputFile.h"
#include "Thread.h"
#include "RadyxOptions.h"
#include "BcjX86.h"
#include "Progress.h"
#include "ErrorCode.h"
#include "fast-lzma2/fast-lzma2.h"

namespace Radyx {

class FastLzma2
{
public:
	FastLzma2(RadyxOptions& options);
	~FastLzma2();
    void SetOptions(Lzma2Options& lzma2);
    void SetTimeout(unsigned ms);
	void Begin(bool do_bcj);
    uint8_t* GetAvailableBuffer(unsigned long& size);
    void AddByteCount(size_t count, OutputStream& out_stream, Progress* progress);
    CoderInfo GetCoderInfo();
    uint_least64_t Finalize(OutputStream& out_stream, Progress* progress);
    void IncBufferCount(OutputStream& out_stream);
    void Write(OutputStream& out_stream);
    void Cancel();
	size_t GetUnpackSize() const { return unpack_size; }
	size_t GetPackSize() const { return pack_size; }
	bool UsedBcj() const { return bcj.get() != nullptr; }
	CoderInfo GetBcjCoderInfo() const { return bcj->GetCoderInfo(); }
	size_t GetMemoryUsage() const { return FL2_estimateCStreamSize_usingCStream(fcs); }

private:
    void CheckError(size_t res);
    size_t WaitAndReport(size_t csize, Progress* progress);
    void WriteBuffers(OutputStream& out_stream);

    FL2_CStream* fcs;
	std::unique_ptr<BcjTransform> bcj;
    FL2_dictBuffer dict;
    size_t dict_pos;
    size_t bcj_trim;
    uint_least64_t unpack_size;
    uint_least64_t pack_size;
    uint8_t bcj_cache[4];

	FastLzma2(const FastLzma2&) = delete;
	FastLzma2& operator=(const FastLzma2&) = delete;
};

}

#endif // RADYX_UNIT_COMPRESSOR_H