///////////////////////////////////////////////////////////////////////////////
//
// Class: PackedMatchTable
//        Match table with each link & length packed into 32 bits
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

#ifndef RADYX_PACKED_MATCH_TABLE_H
#define RADYX_PACKED_MATCH_TABLE_H

#include <cinttypes>
#include <memory>

namespace Radyx {

class PackedMatchTable
{
private:
	static const UintFast32 kNullLink = MatchTableBuilder::kNullLink;
	static const std::array<UintFast32, 4> kMatchBufferSize;
	static const unsigned kLinkBits = 26;
	static const unsigned kLengthBits = 6;
	static const UintFast32 kMask = (1 << kLinkBits) - 1;
public:
	static const UintFast32 kMaxLength = (1 << kLengthBits) - 1;
	static const size_t kMaxDictionary = size_t(1) << kLinkBits;
	inline PackedMatchTable(size_t dictionary_size);
	inline void InitMatchLink(size_t index, UintFast32 link);
	inline UintFast32 GetInitialMatchLink(size_t index) const;
	inline UintFast32 GetMatchLink(size_t index) const;
	inline UintFast32 GetMatchLength(size_t index) const;
	inline UintFast32 GetMatchLinkAndLength(size_t index, uint8_t& length) const;
	inline UintFast32 GetMatchLinkAndLength(size_t index, unsigned& length) const;
	inline void SetMatchLink(size_t index, UintFast32 link, UintFast32 length);
	inline void SetMatchLength(size_t index, UintFast32 link, UintFast32 length);
	inline void SetMatchLinkAndLength(size_t index, UintFast32 link, uint8_t length);
	inline void SetMatchLinkAndLength(size_t index, UintFast32 link, UintFast32 length);
	inline void RestrictMatchLength(size_t index, UintFast32 length);
	inline size_t CalcMatchBufferSize(size_t block_size, unsigned extra_thread_count) const;
	void SetNull(size_t index) { match_table[index] = kNullLink; }
	bool HaveMatch(size_t index) const { return match_table[index] != kNullLink; }
	UintFast32* GetBuffer(size_t index) { return match_table.get() + index; }

private:
	std::unique_ptr<UintFast32[]> match_table;

	PackedMatchTable(const PackedMatchTable&) = delete;
	PackedMatchTable& operator=(const PackedMatchTable&) = delete;
};

PackedMatchTable::PackedMatchTable(size_t dictionary_size) : match_table(new UintFast32[dictionary_size])
{
	if (dictionary_size > kMaxDictionary) {
		throw std::runtime_error("Internal error: incorrect match table type.");
	}
}

void PackedMatchTable::InitMatchLink(size_t index, UintFast32 link)
{
	match_table[index] = link;
}

UintFast32 PackedMatchTable::GetInitialMatchLink(size_t index) const
{
	return match_table[index];
}

UintFast32 PackedMatchTable::GetMatchLink(size_t index) const
{
	return match_table[index] & kMask;
}

UintFast32 PackedMatchTable::GetMatchLength(size_t index) const
{
	return match_table[index] >> kLinkBits;
}

UintFast32 PackedMatchTable::GetMatchLinkAndLength(size_t index, uint8_t& length) const
{
	UintFast32 link = match_table[index];
	length = static_cast<uint8_t>(link >> kLinkBits);
	return link & kMask;
}

UintFast32 PackedMatchTable::GetMatchLinkAndLength(size_t index, unsigned& length) const
{
	UintFast32 link = match_table[index];
	length = static_cast<unsigned>(link >> kLinkBits);
	return link & kMask;
}

void PackedMatchTable::SetMatchLink(size_t index, UintFast32 link, UintFast32 length)
{
	match_table[index] = link | (length << kLinkBits);
}

void PackedMatchTable::SetMatchLength(size_t index, UintFast32 link, UintFast32 length)
{
	match_table[index] = link | (length << kLinkBits);
}

void PackedMatchTable::SetMatchLinkAndLength(size_t index, UintFast32 link, uint8_t length)
{
	match_table[index] = link | (static_cast<UintFast32>(length) << kLinkBits);
}

void PackedMatchTable::SetMatchLinkAndLength(size_t index, UintFast32 link, UintFast32 length)
{
	match_table[index] = link | (length << kLinkBits);
}

void PackedMatchTable::RestrictMatchLength(size_t index, UintFast32 length)
{
	if (match_table[index] != kNullLink && (match_table[index] >> kLinkBits) > length)
		match_table[index] = (match_table[index] & kMask) | (length << kLinkBits);
}

size_t PackedMatchTable::CalcMatchBufferSize(size_t block_size, unsigned extra_thread_count) const
{
	return std::min<size_t>(block_size,
		kMatchBufferSize[std::min<size_t>(extra_thread_count, kMatchBufferSize.size() - 1)]);
}

}

#endif // RADYX_PACKED_MATCH_TABLE_H