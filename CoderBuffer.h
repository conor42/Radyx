///////////////////////////////////////////////////////////////////////////////
//
// Class: CoderBuffer
//        Management of encoder data buffer and data block overlap
//
// Copyright 2017 Conor McCarthy
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

#ifndef RADYX_CODER_BUFFER_H
#define RADYX_CODER_BUFFER_H

#include "common.h"
#include <stdexcept>
#include "ArchiveStreamIn.h"
#include "InPlaceFilter.h"
#include "DataBlock.h"

namespace Radyx {

// Manages a buffer with an overlap section at the start and a short overrun section at the end.
// In-place filtering can process into the overrun.
// Encoding always starts at block_start and stops at min(block_end, buffer_main_size).
// Reset and Shift keep the overrun.
// read_extra < buffer_main_size && overlap < buffer_main_size
class CoderBuffer
{
public:
	CoderBuffer(size_t buffer_main_size_, size_t max_buffer_overrun, size_t read_extra_, size_t overlap_)
		: data_buffer(buffer_main_size_ ? new uint8_t[buffer_main_size_ + std::max(max_buffer_overrun, read_extra_)] : nullptr),
		buffer_main_size(buffer_main_size_ & kBoundaryMask),
		read_extra(read_extra_),
		overlap(overlap_ & kBoundaryMask),
		block_start(0),
		block_end(0)
	{
		if (buffer_main_size_ && read_extra >= buffer_main_size) {
			throw std::runtime_error("CoderBuffer read_extra");
		}
		if (buffer_main_size_ && overlap >= buffer_main_size) {
			throw std::runtime_error("CoderBuffer overlap");
		}
	}

	size_t GetMainBufferSize() const noexcept {
		return buffer_main_size;
	}

	size_t GetAvailableSpace() {
		return buffer_main_size - GetMainEnd();
	}

	// Return true if there is any data remaining to compress, either read into overrun or unread
	bool IsFull() const {
		return block_end >= buffer_main_size;
	}

	size_t Read(ArchiveStreamIn* in) {
		if (IsFull())
			return 0;
		size_t bytes = in->Read(data_buffer.get() + block_end, buffer_main_size + read_extra - block_end);
		block_end += bytes;
		return bytes;
	}

	void Put(uint8_t c) {
		if (block_end >= buffer_main_size + read_extra) {
			throw std::runtime_error("Buffer overflow");
		}
		data_buffer[block_end] = c;
		++block_end;
	}

	void Write(const void* data, size_t count) {
		if (block_end + count > buffer_main_size + read_extra) {
			throw std::runtime_error("Buffer overflow");
		}
		memcpy(data_buffer.get() + block_end, data, count);
		block_end += count;
	}

	void Reset() {
		if (block_end > buffer_main_size) {
			block_end -= buffer_main_size;
			memcpy(data_buffer.get(), data_buffer.get() + buffer_main_size, block_end);
		}
		else {
			block_end = 0;
		}
		block_start = 0;
	}

	DataBlock GetMainData() {
		return DataBlock(data_buffer.get(), block_start, GetMainEnd());
	}

	// For writing uncompressed data, no filters
	DataBlock GetAllData() {
		return DataBlock(data_buffer.get(), block_start, block_end);
	}

	DataBlock RunFilters(FilterList* filters) {
		size_t end = block_end;
		if (filters) {
			for (auto& f : *filters) {
				end = block_start + f->Encode(data_buffer.get() + block_start, GetMainEnd() - block_start, end - block_start);
			}
		}
		// Process max amount if eof
		if (block_end < buffer_main_size + read_extra) {
			end = GetMainEnd();
		}
		else {
			end = std::min(end, GetMainEnd());
		}
		return DataBlock(data_buffer.get(), block_start, end);
	}

	void Shift() {
		if ((block_end & kBoundaryMask) > overlap) {
			size_t src = std::min((block_end & kBoundaryMask) - overlap, buffer_main_size);
			size_t shift_len = block_end - src;
			if (src >= shift_len) {
				memcpy(data_buffer.get(), data_buffer.get() + src, shift_len);
			}
			else {
				memmove(data_buffer.get(), data_buffer.get() + src, shift_len);
			}
			block_start = shift_len - GetOverrun();
			block_end = shift_len;
		}
	}

	void Shift(CoderBuffer& other) {
		block_start = other.block_start;
		block_end = other.block_end;
		size_t src = 0;
		if ((block_end & kBoundaryMask) > overlap) {
			src = std::min((block_end & kBoundaryMask) - overlap, buffer_main_size);
		}
		size_t shift_len = block_end - src;
		memcpy(data_buffer.get(), other.data_buffer.get() + src, shift_len);
		block_start = shift_len - GetOverrun();
		block_end = shift_len;
	}

	bool Unprocessed() const noexcept {
		return block_end > block_start;
	}
	
	operator bool() const {
		return data_buffer.operator bool();
	}

	uint8_t* get() {
		return data_buffer.get();
	}

	const uint8_t* get() const {
		return data_buffer.get();
	}

private:
	size_t GetMainEnd() const noexcept {
		return std::min(block_end, buffer_main_size);
	}

	size_t GetOverrun() const noexcept {
		return (block_end > buffer_main_size) ? block_end - buffer_main_size : 0;
	}

	std::unique_ptr<uint8_t[]> data_buffer;
	const size_t buffer_main_size;
	const size_t read_extra;
	const size_t overlap;
	size_t block_start;
	size_t block_end;
};

}

#endif