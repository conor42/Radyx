///////////////////////////////////////////////////////////////////////////////
//
// Class: RadyxLzma2Enc
//        Radyx encoder class for 7-zip
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

#ifndef __RADYX_LZMA2ENC_H
#define __RADYX_LZMA2ENC_H

#include "Lzma2Compressor.h"
#include "UnitCompressor.h"
#include "ArchiveStreamIn.h"
#include "InPlaceFilter.h"
#include "OutputStream.h"

namespace Radyx {

class RadyxLzma2Enc : public OutputStream
{
public:
	RadyxLzma2Enc(OutputStream& out_stream_)
		: out_stream(out_stream_),
		in_processed(0),
		pack_size(0)
	{ }
	void Create(Lzma2Options& params, size_t read_extra, unsigned numThreads);
	size_t GetMemoryUsage() const noexcept;
	unsigned GetEncodeWeight() const noexcept {
		return compressor->GetEncodeWeight();
	}
	void Encode(ArchiveStreamIn* inStream, FilterList* filters, std::list<CoderInfo>& coder_info, Progress* progress);
	OutputStream& Write(const void* data, size_t count);
	OutputStream& Put(char c) {
		Write(&c, 1);
		return *this;
	}
	void Flush();
	void DisableExceptions() {}
	void RestoreExceptions() {}
	bool Fail() const noexcept {
		return false;
	}
	size_t GetPackSize() const noexcept {
		return pack_size;
	}
	size_t GetUnpackSize() const noexcept {
		return unit_comp->GetUnpackSize();
	}
	CoderInfo GetCoderInfo() const noexcept;
	operator bool() {
		return compressor.operator bool();
	}
private:
	OutputStream& out_stream;
    std::unique_ptr<CompressorInterface> compressor;
    std::unique_ptr<UnitCompressor> unit_comp;
	std::unique_ptr<ThreadPool> threads;
	uint_least64_t in_processed;
	size_t pack_size;
};

}
#endif