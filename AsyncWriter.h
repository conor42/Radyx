///////////////////////////////////////////////////////////////////////////////
//
// Class:   AsyncWriter
//          Write output data in a worker thread
//
// Copyright 2015 Conor McCarthy
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

#ifndef RADYX_ASYNC_WRITER_H
#define RADYX_ASYNC_WRITER_H

#include <atomic>
#include <ostream>
#include "ThreadPool.h"
#include "ErrorCode.h"

namespace Radyx {

class AsyncWriter
{
public:
	AsyncWriter(std::ostream& out_stream_, ThreadPool::Thread& thread_)
		: out_stream(out_stream_), thread(thread_), out_buffer(nullptr) {}
	void Write(uint8_t* out_buffer_, size_t buffer_size_);
	bool Fail() const { return error.type != ErrorCode::kGood; }
	ErrorCode GetErrorCode() const { return error; }
private:
	static void ThreadFn(void* pwork, int unused);

	std::ostream& out_stream;
	ThreadPool::Thread& thread;
	uint8_t* out_buffer;
	std::atomic_size_t buffer_size;
	ErrorCode error;
	AsyncWriter(const AsyncWriter&) = delete;
	AsyncWriter& operator=(const AsyncWriter&) = delete;
};

}

#endif // RADYX_ASYNC_WRITER_H