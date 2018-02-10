///////////////////////////////////////////////////////////////////////////////
//
// Class: UnitCompressor
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
#include "CoderBuffer.h"
#include "fast-lzma2/fast-lzma2.h"
#include "Thread.h"
#include "RadyxOptions.h"
#include "ArchiveStreamIn.h"
#include "InPlaceFilter.h"
#include "OutputStream.h"
#include "Progress.h"
#include "ErrorCode.h"

namespace Radyx {

class UnitCompressor
{
public:
	UnitCompressor(RadyxOptions& options, size_t read_extra);
	~UnitCompressor();
	void Begin();
	void Begin(bool async_read_);
	size_t GetAvailableSpace() const;
	uint8_t* GetAvailableBuffer() { return data_buffers[buffer_index].get() + block_end; }
	void AddByteCount(size_t count) { block_end += count; }
	void RemoveByteCount(size_t count) { block_end -= count; }
	void Compress(ArchiveStreamIn* inStream, OutputStream& out_stream, FilterList* filters, std::list<CoderInfo>& coder_info, Progress* progress);
	bool IsFull() const {
		return data_buffers[buffer_index].IsFull();
	}
	size_t Read(ArchiveStreamIn* inStream) {
		return data_buffers[buffer_index].Read(inStream);
	}
	void Put(uint8_t c) {
		data_buffers[buffer_index].Put(c);
	}
	void Write(const void* data,
		size_t count,
		OutputStream& out_stream);
	void Compress(FilterList* filters,
		OutputStream& out_stream,
		Progress* progress);
	void Shift();
	void Write(OutputStream& out_stream);
    CoderInfo GetCoderInfo();
    size_t Finalize(OutputStream& out_stream);
    void CheckError() const;
	inline void WaitCompletion();
	size_t GetMemoryUsage() const { return FL2_estimateCCtxSize_usingCCtx(cctx) + dictionary_size * (1 + async_read); }
	bool Unprocessed() const noexcept {
		return data_buffers[buffer_index].Unprocessed();
	}
	size_t GetUnpackSize() const noexcept {
		return unpack_size;
	}
	size_t GetPackSize() const noexcept {
		return pack_size;
	}

private:
	struct ThreadArgs
	{
        FL2_CCtx* cctx;
		OutputStream* out_stream;
		Progress* progress;
        FL2_blockBuffer data_block;  // Must not be a reference
        size_t prev_done;
		ThreadArgs() {}
		ThreadArgs(FL2_CCtx* cctx_,
			OutputStream* out_stream_,
			Progress* progress_,
			const FL2_blockBuffer& data_block_)
			: cctx(cctx_), out_stream(out_stream_), progress(progress_), data_block(data_block_), prev_done(0) {}
	};

    void CompressTranslateError();
    static int FL2LIB_CALL WriterFn(const void* src, size_t srcSize, void* opaque);
    static int FL2LIB_CALL ProgressFn(size_t done, void* opaque);
    static void ThreadFn(void* pwork, int unused);

    FL2_CCtx* cctx;
	CoderBuffer data_buffers[2];
	size_t dictionary_size;
	size_t block_start;
	size_t block_end;
	size_t unpack_size;
	size_t pack_size;
	size_t unprocessed;
	Thread compress_thread;
	ThreadArgs args;
	size_t buffer_index;
	ErrorCode error_code;
	bool async_read;
	volatile bool working;

	UnitCompressor(const UnitCompressor&) = delete;
	UnitCompressor& operator=(const UnitCompressor&) = delete;
	UnitCompressor(UnitCompressor&&) = delete;
	UnitCompressor& operator=(UnitCompressor&&) = delete;
};

void UnitCompressor::WaitCompletion()
{
	if (async_read && working) {
		compress_thread.Join();
	}
}

}

#endif // RADYX_UNIT_COMPRESSOR_H