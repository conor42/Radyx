///////////////////////////////////////////////////////////////////////////////
//
// Class: HashChain
//        Find string matches using a hash table and linked-list
//
// Copyright 2015 Conor McCarthy
// Hash function derived from LzHash.h in 7-zip by Igor Pavlov
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

#ifndef RADYX_HASH_CHAIN_H
#define RADYX_HASH_CHAIN_H

#include <vector>
#include <array>
#include "DataBlock.h"
#include "MatchResult.h"
#include "Crc32.h"

namespace Radyx {

template<unsigned kTableBits2, unsigned kTableBits3, size_t kMatchLenMax>
class HashChain
{
public:
	HashChain(unsigned dictionary_bits_3) noexcept;
	inline void Reset() noexcept;
	inline void Start(ptrdiff_t prev_index_) noexcept;
	inline bool IsFast() {
		return chain_mask_3 < kMaxFastSize;
	}
	inline int_fast32_t* GetTable2() {
		return table_2.data();
	}
	inline int_fast32_t* GetTable3() {
		return table_3.data();
	}
	inline ptrdiff_t GetMaxDist() {
		return chain_mask_3;
	}
	template<class MatchCollection>
	inline size_t GetMatchesFast(const DataBlock& block,
		ptrdiff_t index,
		size_t max_length,
		MatchResult match,
		MatchCollection& matches);
	template<class MatchCollection>
	size_t GetMatches(const DataBlock& block,
		ptrdiff_t index,
		unsigned cycles,
		MatchResult match,
		MatchCollection& matches);

private:
	static const UintFast32 kHashMask2 = (1 << kTableBits2) - 1;
	static const UintFast32 kHashMask3 = (1 << kTableBits3) - 1;
	static const ptrdiff_t kChainMask2 = kHashMask2;
	static const int_fast32_t kNullLink = -1;
	static const unsigned kMaxFastBits = 10;
	static const ptrdiff_t kMaxFastSize = 1 << kMaxFastBits;

	std::unique_ptr<int_fast32_t[]> hash_chain_3;
	std::array<int_fast32_t, 1 << kTableBits2> hash_chain_2;
	std::array<int_fast32_t, 1 << kTableBits3> table_3;
	std::array<int_fast32_t, 1 << kTableBits2> table_2;
	ptrdiff_t chain_mask_3;
	ptrdiff_t prev_index;

	HashChain(const HashChain&) = delete;
	HashChain& operator=(const HashChain&) = delete;
	HashChain(HashChain&&) = delete;
	HashChain& operator=(HashChain&&) = delete;
};

template<unsigned kTableBits2, unsigned kTableBits3, size_t kMatchLenMax>
HashChain<kTableBits2, kTableBits3, kMatchLenMax>::HashChain(unsigned dictionary_bits_3) noexcept
	: hash_chain_3(dictionary_bits_3 > kMaxFastBits ? new int_fast32_t[size_t(1) << dictionary_bits_3] : nullptr),
	chain_mask_3((1 << dictionary_bits_3) - 1),
	prev_index(-1)
{
	Reset();
}

template<unsigned kTableBits2, unsigned kTableBits3, size_t kMatchLenMax>
inline void HashChain<kTableBits2, kTableBits3, kMatchLenMax>::Start(ptrdiff_t prev_index_) noexcept
{
	prev_index = prev_index_;
}

template<unsigned kTableBits2, unsigned kTableBits3, size_t kMatchLenMax>
inline void HashChain<kTableBits2, kTableBits3, kMatchLenMax>::Reset() noexcept
{
	// GCC is strict about passing a reference to fill()
	int_fast32_t n = kNullLink;
	table_2.fill(n);
	table_3.fill(n);
}

static inline size_t GetHash2(const uint8_t* data) noexcept
{
	return Crc32::GetHash(data[0]) ^ data[1];
}

static inline size_t GetHash3(const uint8_t* data, size_t hash) noexcept
{
	hash ^= size_t(data[2]) << 8;
	return hash;
}

template<unsigned kTableBits2, unsigned kTableBits3, size_t kMatchLenMax>
template<class MatchCollection>
inline size_t HashChain<kTableBits2, kTableBits3, kMatchLenMax>::GetMatchesFast(const DataBlock& block,
	ptrdiff_t index,
	size_t max_length,
	MatchResult match,
	MatchCollection& matches)
{
	matches.Clear();
	const uint8_t* data = block.data;
	prev_index = std::max(prev_index, index - chain_mask_3);
	while (++prev_index < index) {
		size_t hash = GetHash2(data + prev_index);
		table_2[hash & kHashMask2] = static_cast<int_fast32_t>(prev_index);
		hash = GetHash3(data + prev_index, hash) & kHashMask3;
		table_3[hash] = static_cast<int_fast32_t>(prev_index);
	}
	data += index;
	size_t main_len = 0;
	size_t hash_2 = GetHash2(data);
	size_t hash_3 = GetHash3(data, hash_2) & kHashMask3;
	hash_2 &= kHashMask2;
	uint_fast32_t index_2 = table_2[hash_2];
	if (index_2 != ~0) {
		ptrdiff_t dist = index - index_2 - 1;
		if (dist < match.dist) {
			if (dist < chain_mask_3) {
				const uint8_t* data_2 = block.data + index_2;
				if (data[0] == data_2[0]) {
					main_len = 2;
					for (; data[main_len] == data_2[main_len] && main_len < max_length; ++main_len) {
					}
					matches.push_back(MatchResult(static_cast<unsigned>(main_len),
						static_cast<UintFast32>(dist)));
				}
			}
			uint_fast32_t index_3 = table_3[hash_3];
			if (index_3 < index_2 && index - index_3 <= chain_mask_3 && main_len < max_length) {
				const uint8_t* data_2 = block.data + index_3;
				if (data[0] == data_2[0]) {
					size_t len_test = 3;
					for (; data[len_test] == data_2[len_test] && len_test < max_length; ++len_test) {
					}
					if (len_test > main_len) {
						matches.push_back(MatchResult(static_cast<unsigned>(len_test),
							static_cast<UintFast32>(index - index_3 - 1)));
						main_len = len_test;
					}
				}
			}
		}
	}
	table_2[hash_2] = static_cast<UintFast32>(index);
	table_3[hash_3] = static_cast<UintFast32>(index);
	if (static_cast<unsigned>(main_len) < match.length) {
		matches.push_back(match);
		return match.length;
	}
	return main_len;
}

template<unsigned kTableBits2, unsigned kTableBits3, size_t kMatchLenMax>
template<class MatchCollection>
size_t HashChain<kTableBits2, kTableBits3, kMatchLenMax>::GetMatches(const DataBlock& block,
	ptrdiff_t index,
	unsigned cycles,
	MatchResult match,
	MatchCollection& matches)
{
	if (index == prev_index) {
		return matches.GetMaxLength();
	}
	assert(index > prev_index);
	matches.Clear();
	if (match.length < 3) {
		matches.push_back(match);
		return match.length;
	}
	const uint8_t* data = block.data;
	// Max match length
	size_t limit = std::min(block.end - index, kMatchLenMax);
	if (limit < 2) {
		return 0;
	}
	// Update hash tables and chains for any positions that were skipped
	prev_index = std::max(prev_index, index - chain_mask_3);
	while (++prev_index < index) {
		size_t hash = GetHash2(data + prev_index);
		hash_chain_2[prev_index & kChainMask2] = table_2[hash & kHashMask2];
		table_2[hash & kHashMask2] = static_cast<int_fast32_t>(prev_index);
		hash = GetHash3(data + prev_index, hash) & kHashMask3;
		hash_chain_3[prev_index & chain_mask_3] = table_3[hash];
		table_3[hash] = static_cast<int_fast32_t>(prev_index);
	}
	data += index;
	ptrdiff_t end_index = index - match.dist;
	// The lowest position to be searched
	ptrdiff_t end = std::max(end_index, index - kChainMask2 - 1);
	size_t max_len = 0;
	size_t hash = GetHash2(data);
	int_fast32_t first_match = table_2[hash & kHashMask2];
	table_2[hash & kHashMask2] = static_cast<int_fast32_t>(index);
	ptrdiff_t match_2 = first_match;
	unsigned end_length = match.length;
	for (; match_2 >= end && cycles > 0; match_2 = hash_chain_2[match_2 & kChainMask2]) {
		--cycles;
		const uint8_t* data_2 = block.data + match_2;
		if (data[0] != data_2[0]) {
			continue;
		}
		max_len = 2;
		for (; data[max_len] == data_2[max_len] && max_len < limit; ++max_len) {
		}
		matches.push_back(MatchResult(static_cast<unsigned>(max_len),
			static_cast<UintFast32>(index - match_2 - 1)));
		// Prevent this position being tested again
		--match_2;
		break;
	}
	hash_chain_2[index & kChainMask2] = first_match;
	hash = GetHash3(data, hash) & kHashMask3;
	first_match = table_3[hash];
	table_3[hash] = static_cast<int_fast32_t>(index);
	--end_length;
	if (max_len < end_length && match_2 >= index - chain_mask_3 - 1) {
		end = std::max(end_index, index - chain_mask_3 - 1);
		ptrdiff_t match_3 = first_match;
		for (; match_3 >= end && cycles > 0; match_3 = hash_chain_3[match_3 & chain_mask_3]) {
			if (match_3 > match_2) {
				continue;
			}
			--cycles;
			const uint8_t* data_2 = block.data + match_3;
			// Only the first byte needs to be checked if the 2nd and 3rd bytes don't overlap in the hash value
			if (data[0] != data_2[0]) {
				continue;
			}
			size_t len_test = 3;
			for (; data[len_test] == data_2[len_test] && len_test < limit; ++len_test) {
			}
			if (len_test > max_len) {
				matches.push_back(MatchResult(static_cast<unsigned>(len_test),
					static_cast<UintFast32>(index - match_3 - 1)));
				max_len = len_test;
				if (len_test >= end_length) {
					break;
				}
			}
		}
	}
	hash_chain_3[index & chain_mask_3] = first_match;
	if (static_cast<unsigned>(max_len) < match.length) {
		matches.push_back(match);
		return match.length;
	}
	return max_len;
}

}
#endif // RADYX_HASH_CHAIN_H