///////////////////////////////////////////////////////////////////////////////
//
// Class: MatchTableShort
//        Create a table of string matches concurrently with other threads
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

#ifndef RADYX_MATCH_TABLE_SHORT_H
#define RADYX_MATCH_TABLE_SHORT_H

#include <array>
#include <memory>
#include "common.h"
#include "DataBlock.h"
#include "FlagTable.h"
#include "MatchResult.h"
#include "OptionalSetting.h"

namespace Radyx {

class MatchTableShort
{
public:
	static const int_fast32_t kNullLink = UINT32_MAX;
	static const size_t kLenMin = 2;
	static const size_t kLenMax = 273;

public:
	MatchTableShort(size_t dictionary_size_) 
	: match_table(new StringMatch[dictionary_size_]),
		dictionary_size(dictionary_size_),
		base_index(0),
		end_index(0)
	{
	}
	size_t GetMatches(const DataBlock& block,
		ptrdiff_t index,
		unsigned cycles,
		MatchResult match,
		MatchCollection<kLenMin, kLenMax>& matches);
	void Reset();

private:
	static const unsigned kOverlap = 2;
	static const unsigned kRadixBitsSmall = 8;
	static const size_t kRadixTableSizeSmall = 1 << kRadixBitsSmall;
	static const size_t kStackSize = kRadixTableSizeSmall * 2;
	static const int_fast32_t kMaxRepeat = 16;
	static const unsigned kMaxBruteForceListSize = 6;

	struct StringMatch {
		int_fast32_t next[2];
		uint8_t chars[4];
	};

	static inline void Copy2Bytes(uint8_t* dest, const uint8_t* src) noexcept;
	static inline void Copy4Bytes(uint8_t* dest, const uint8_t* src) noexcept;

	void BuildTable(const DataBlock& block, ptrdiff_t index);
	void InitLinks8(const DataBlock& block);
	ptrdiff_t HandleRepeat(const DataBlock& block, ptrdiff_t block_size, ptrdiff_t i, size_t radix_8) noexcept;
	void Recurse3(const DataBlock& block, ptrdiff_t index);
	void Recurse4(const DataBlock& block, ptrdiff_t index);

	// Fast-reset bool tables for recording initial radix value occurrence
	FlagTable<UintFast32, 8> flag_table_8;
	// Table of list heads
	int_fast32_t head_table[kRadixTableSizeSmall];
	std::unique_ptr<StringMatch[]> match_table;
	ptrdiff_t dictionary_size;
	ptrdiff_t base_index;
	ptrdiff_t end_index;

	MatchTableShort(const MatchTableShort&) = delete;
	MatchTableShort(MatchTableShort&&) = delete;
	MatchTableShort& operator=(const MatchTableShort&) = delete;
	MatchTableShort& operator=(MatchTableShort&&) = delete;
};

void MatchTableShort::Copy2Bytes(uint8_t* dest, const uint8_t* src) noexcept
{
#ifndef DISALLOW_UNALIGNED_ACCESS
	(reinterpret_cast<uint16_t*>(dest))[0] = (reinterpret_cast<const uint16_t*>(src))[0];
#else
	dest[0] = src[0];
	dest[1] = src[1];
#endif
}

void MatchTableShort::Copy4Bytes(uint8_t* dest, const uint8_t* src) noexcept
{
#ifndef DISALLOW_UNALIGNED_ACCESS
	(reinterpret_cast<uint32_t*>(dest))[0] = (reinterpret_cast<const uint32_t*>(src))[0];
#else
	dest[0] = src[0];
	dest[1] = src[1];
	dest[2] = src[2];
	dest[3] = src[3];
#endif
}

}

#endif // RADYX_MATCH_TABLE_BUILDER_H