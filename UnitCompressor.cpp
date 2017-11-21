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

#include <cstring>
#include "common.h"
#include "UnitCompressor.h"

#ifdef UI_EXCEPTIONS
#include "IoException.h"
#include "Strings.h"
#endif

namespace Radyx {

#ifdef UI_EXCEPTIONS

inline void throw_write_exception(int os_error)
{
	throw IoException(Strings::kCannotWriteArchive, os_error, _T(""));
}

#else

inline void throw_write_exception(int /*os_error*/)
{
	throw std::ios_base::failure("");
}

#endif

UnitCompressor::UnitCompressor(size_t dictionary_size_,
	size_t max_buffer_overrun,
	size_t read_extra_,
	size_t overlap_,
	bool async_read_)
	: data_buffers{ { CoderBuffer(dictionary_size_, max_buffer_overrun, read_extra_, overlap_),
		CoderBuffer(async_read_ ? dictionary_size_ : 0, max_buffer_overrun, read_extra_, overlap_)} },
	buffer_index(0),
	async_read(async_read_),
	working(false)
{
	Reset(async_read_);
}

void UnitCompressor::Reset()
{
	Reset(async_read);
}

void UnitCompressor::Reset(bool async_read_)
{
	CheckError();
	WaitCompletion();
	unpack_size = 0;
	pack_size = 0;
	async_read = async_read_ && data_buffers[1];
	buffer_index = 0;
	data_buffers[0].Reset();
}

void UnitCompressor::ThreadFn(void* pwork, int /*unused*/)
{
	UnitCompressor* unit_comp = reinterpret_cast<UnitCompressor*>(pwork);
	try {
		unit_comp->pack_size += unit_comp->args.compressor->Compress(
			unit_comp->args.data_block,
			*unit_comp->args.threads,
			*unit_comp->args.out_stream,
			unit_comp->error_code,
			unit_comp->args.progress);
	}
	// The compressor shouldn't throw anything except bad_alloc
	catch (std::bad_alloc&) {
		unit_comp->error_code.type = ErrorCode::kMemory;
	}
	catch (std::exception&) {
		unit_comp->error_code.type = ErrorCode::kUnknown;
	}
	unit_comp->working = false;
}

void UnitCompressor::CheckError() const
{
	if (error_code.type == ErrorCode::kGood) {
		return;
	}
	if (error_code.type == ErrorCode::kMemory) {
		throw std::bad_alloc();
	}
	else if (error_code.type == ErrorCode::kWrite) {
		throw_write_exception(error_code.os_code);
	}
	else {
		throw std::exception();
	}
}

void UnitCompressor::Write(const void* data,
	size_t count,
	CompressorInterface& compressor,
	ThreadPool& threads,
	OutputStream& out_stream)
{
	const char* src = reinterpret_cast<const char*>(data);
	while (count && !g_break) {
		size_t to_write = std::min(count, data_buffers[buffer_index].GetAvailableSpace());
		data_buffers[buffer_index].Write(src, to_write);
		if (to_write < count) {
			Compress(nullptr, compressor, threads, out_stream, nullptr);
			Shift();
		}
		src += to_write;
		count -= to_write;
	}
}

void UnitCompressor::Compress(FilterList* filters,
	CompressorInterface& compressor,
	ThreadPool& threads,
	OutputStream& out_stream,
	Progress* progress)
{
	CheckError();
	if (data_buffers[buffer_index].Unprocessed())
	{
		DataBlock data_block = data_buffers[buffer_index].RunFilters(filters);
		if (async_read) {
			WaitCompletion();
			args = ThreadArgs(&compressor,
				&threads,
				&out_stream,
				progress,
				data_block);
			working = true;
			compress_thread.SetWork(ThreadFn, this, 0);
			buffer_index ^= 1;
		}
		else {
			pack_size += compressor.Compress(data_block, threads, out_stream, error_code, progress);
			CheckError();
		}
		unpack_size += data_block.Length();
	}
}

void UnitCompressor::Shift()
{
	CheckError();
	if (async_read) {
		data_buffers[buffer_index].Shift(data_buffers[buffer_index ^ 1]);

	}
	else {
		data_buffers[buffer_index].Shift();
	}
}

void UnitCompressor::Write(OutputStream& out_stream)
{
	CheckError();
	DataBlock data_block = data_buffers[buffer_index].GetAllData();
	size_t to_write = data_block.Length();
	out_stream.Write(data_block.data + data_block.start, to_write);
	if (!g_break && out_stream.Fail()) {
		error_code.LoadOsErrorCode();
		throw_write_exception(error_code.os_code);
	}
	unpack_size += to_write;
	pack_size += to_write;
	data_buffers[buffer_index].Reset();
}

}