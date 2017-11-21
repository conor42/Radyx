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

#include "RadyxLzma2Enc.h"

namespace Radyx {

void RadyxLzma2Enc::Create(Lzma2Options& params, size_t read_extra, unsigned numThreads)
{
	threads.reset(new ThreadPool(numThreads - 1));

	if (params.dictionary_size > PackedMatchTable::kMaxDictionary
		|| (params.fast_length > PackedMatchTable::kMaxLength)) {
		compressor.reset(new Lzma2Compressor<StructuredMatchTable>(params));
	}
	else {
		compressor.reset(new Lzma2Compressor<PackedMatchTable>(params));
	}
	unit_comp.reset(new UnitCompressor(compressor->GetDictionarySize(),
		compressor->GetMaxBufferOverrun(),
		read_extra,
		(params.dictionary_size * params.block_overlap) >> Lzma2Options::kOverlapShift,
		false));
}

void RadyxLzma2Enc::Encode(ArchiveStreamIn* in_stream,
	FilterList* filters,
	std::list<CoderInfo>& coder_info,
	Progress* progress)
{
	in_processed = 0;
	pack_size = 0;
	while (!g_break)
	{
		size_t inSize = unit_comp->Read(in_stream);
		in_processed += inSize;

		if (unit_comp->Unprocessed()) {
			unit_comp->Compress(filters, *compressor, *threads, out_stream, progress);
			if (unit_comp->IsFull()) {
				unit_comp->Shift();
			}
			else {
				unit_comp->WaitCompletion();
				pack_size = unit_comp->GetPackSize() + compressor->Finalize(out_stream);
				break;
			}
		}
	}
	coder_info.clear();
	for (auto& f : *filters) {
		if (f->DidEncode()) {
			coder_info.push_front(f->GetCoderInfo());
		}
		f->Reset();
	}
	coder_info.push_front(compressor->GetCoderInfo());
	unit_comp->Reset();
	compressor->Reset();
}

OutputStream& RadyxLzma2Enc::Write(const void* data, size_t count)
{
	unit_comp->Write(data, count, *compressor, *threads, out_stream);
	return *this;
}

void RadyxLzma2Enc::Flush()
{
	if (unit_comp->Unprocessed()) {
		unit_comp->Compress(nullptr, *compressor, *threads, out_stream, nullptr);
	}
	unit_comp->WaitCompletion();
	pack_size = unit_comp->GetPackSize() + compressor->Finalize(out_stream);
}

size_t RadyxLzma2Enc::GetMemoryUsage() const noexcept
{
	return compressor->GetMemoryUsage(threads->GetCount()) + unit_comp->GetMemoryUsage();
}

CoderInfo RadyxLzma2Enc::GetCoderInfo() const noexcept
{
	return compressor->GetCoderInfo();
}

}