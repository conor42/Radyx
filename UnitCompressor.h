///////////////////////////////////////////////////////////////////////////////
//
// Class: UnitCompressor
//        Management of solid data units and data block overlap
//
// Copyright 1998-2000, 2015 Conor McCarthy
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
#include <array>
#include "CoderBuffer.h"
#include "InPlaceFilter.h"
#include "OutputStream.h"
#include "ThreadPool.h"
#include "CompressorInterface.h"
#include "Progress.h"
#include "ErrorCode.h"

namespace Radyx {

class UnitCompressor
{
public:
	UnitCompressor(size_t dictionary_size_, size_t max_buffer_overrun, size_t read_extra_, size_t overlap_, bool async_read_);
	void Reset();
	void Reset(bool async_read_);
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
		CompressorInterface& compressor,
		ThreadPool& threads,
		OutputStream& out_stream);
	void Compress(FilterList* filters,
		CompressorInterface& compressor,
		ThreadPool& threads,
		OutputStream& out_stream,
		Progress* progress);
	void Shift();
	void Write(OutputStream& out_stream);
	void CheckError() const;
	inline void WaitCompletion();

	bool Unprocessed() const noexcept {
		return data_buffers[buffer_index].Unprocessed();
	}
	size_t GetUnpackSize() const noexcept {
		return unpack_size;
	}
	size_t GetPackSize() const noexcept {
		return pack_size;
	}
	size_t GetMemoryUsage() const noexcept {
		return data_buffers[0].GetMainBufferSize() * (1 + async_read);
	}

private:
	struct ThreadArgs
	{
		CompressorInterface* compressor;
		ThreadPool* threads;
		OutputStream* out_stream;
		Progress* progress;
		DataBlock data_block;  // Must not be a reference
		ThreadArgs() {}
		ThreadArgs(CompressorInterface* compressor_,
			ThreadPool* threads_,
			OutputStream* out_stream_,
			Progress* progress_,
			const DataBlock& data_block_)
			: compressor(compressor_), threads(threads_), out_stream(out_stream_), progress(progress_), data_block(data_block_) {}
	};

	static void ThreadFn(void* pwork, int unused);

	std::array<CoderBuffer, 2> data_buffers;
	size_t unpack_size;
	size_t pack_size;
	ThreadPool::Thread compress_thread;
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