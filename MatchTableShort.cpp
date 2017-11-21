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

#include "common.h"
#include "MatchTableShort.h"

namespace Radyx {

size_t MatchTableShort::GetMatches(const DataBlock& block,
	ptrdiff_t index,
	unsigned cycles,
	MatchResult match,
	MatchCollection<kLenMin, kLenMax>& matches)
{
	matches.Clear();
	if (match.length < 3) {
		matches.push_back(match);
		return match.length;
	}
	if (index > end_index) {
		BuildTable(block, index);
	}
	ptrdiff_t link = index - base_index;
	ptrdiff_t end_link = std::max<ptrdiff_t>(link - match.dist, 0);
	ptrdiff_t match_2 = (int_fast32_t)match_table[link].next[0];
	if (match_2 < end_link) {
		matches.push_back(match);
		return match.length;
	}
	size_t max_len = 2;
	const uint8_t* data = block.data + index;
	const uint8_t* base_data = block.data + base_index;
	const uint8_t* data_2 = base_data + match_2;
	size_t limit = std::min(block.end - index, kLenMax);
	for (; max_len < limit && data[max_len] == data_2[max_len]; ++max_len) {
	}
	matches.push_back(MatchResult(static_cast<unsigned>(max_len),
		static_cast<UintFast32>(link - match_2 - 1)));
	unsigned end_length = match.length - 1;
	if (max_len < end_length) {
		for (ptrdiff_t match_3 = (int_fast32_t)match_table[link].next[1]; match_3 >= end_link && cycles > 0; match_3 = (int_fast32_t)match_table[match_3].next[1]) {
			if (match_3 >= match_2) {
				continue;
			}
			--cycles;
			data_2 = base_data + match_3;
			size_t len_test = 3;
			for (; data[len_test] == data_2[len_test] && len_test < limit; ++len_test) {
			}
			if (len_test > max_len) {
				matches.push_back(MatchResult(static_cast<unsigned>(len_test),
					static_cast<UintFast32>(link - match_3 - 1)));
				max_len = len_test;
				if (len_test >= end_length) {
					break;
				}
			}
		}
	}
	if (static_cast<unsigned>(max_len) < match.length) {
		matches.push_back(match);
		return match.length;
	}
	return max_len;
}

void MatchTableShort::Reset()
{
	base_index = 0;
	end_index = 0;
}

void MatchTableShort::BuildTable(const DataBlock& block_large, ptrdiff_t index)
{
	if (end_index + 3 >= static_cast<ptrdiff_t>(block_large.end))
		return;
	ptrdiff_t overlap = std::min((dictionary_size * kOverlap) >> 4, index);
	base_index = index - overlap;
	end_index = base_index + dictionary_size;
	end_index = std::min<ptrdiff_t>(end_index, block_large.end);
	DataBlock block(block_large.data + base_index, overlap, end_index - base_index);
	end_index -= 3;
	InitLinks8(block);
	for (size_t i = 0; i < kRadixTableSizeSmall; ++i) {
		if (head_table[i] >= static_cast<int_fast32_t>(block.start) && match_table[head_table[i]].next[1] >= 0) {
			Recurse3(block, head_table[i]);
		}
	}
}

void MatchTableShort::InitLinks8(const DataBlock& block)
{
	for (size_t i = 0; i < kRadixTableSizeSmall; i += 2) {
		head_table[i] = -1;
		head_table[i + 1] = -1;
	}
	const uint8_t* data_block = block.data;
	size_t radix_8 = data_block[0];
	const ptrdiff_t block_size = block.end - 1;
	// Pre-load the next byte
	for (ptrdiff_t i = 0; i < block_size; ++i) {
		size_t next_radix = data_block[i + 1];
		Copy2Bytes(match_table[i].chars, data_block + i + 1);
		// Link this position to the previous occurance
		// Slot 1 is temporary storage to avoid 1-byte matches in slot 0
		match_table[i].next[0] = -1;
		match_table[i].next[1] = head_table[radix_8];
		// Set the previous to this position
		head_table[radix_8] = static_cast<int_fast32_t>(i);
		radix_8 = next_radix;
	}
	// Never a match at the last byte
	match_table[block.end - 1].next[0] = -1;
}

void MatchTableShort::Recurse3(const DataBlock& block, ptrdiff_t link)
{
	StringMatch* match_buffer = match_table.get();
	const ptrdiff_t block_start = block.start;
	int_fast32_t next_link = match_buffer[link].next[1];
	// Clear the bool flags
	flag_table_8.Reset();
	// Stack of new sub-list heads
	std::array<int_fast32_t, kStackSize> stack;
	size_t st_index = 0;
	ptrdiff_t prev_index_8[kRadixTableSizeSmall];
	while(next_link >= 0) {
		size_t radix_8 = match_buffer[link].chars[0];
		// Seen this char before?
		if (flag_table_8.IsSet(radix_8)) {
			const ptrdiff_t prev = prev_index_8[radix_8];
			// Link the previous occurrence to this one and record the new length
			match_buffer[prev].next[0] = static_cast<int_fast32_t>(link);
			match_buffer[prev].next[1] = -1;
		}
		else {
			flag_table_8.Set(radix_8);
			// Add the new sub list to the stack
			stack[st_index] = static_cast<int_fast32_t>(link);
			++st_index;
		}
		prev_index_8[radix_8] = link;
		link = next_link;
		next_link = match_buffer[link].next[1];
	}
	// Do the last element
	size_t radix_8 = match_buffer[link].chars[0];
	// Nothing to do if there was no previous
	if (flag_table_8.IsSet(radix_8)) {
		const ptrdiff_t prev = prev_index_8[radix_8];
		match_buffer[prev].next[0] = static_cast<int_fast32_t>(link);
		match_buffer[prev].next[1] = -1;
	}
	while (!g_break && st_index > 0) {
		// Pop an item off the stack
		--st_index;
		link = stack[st_index];
		if (match_buffer[link].next[0] < 0) {
			// Nothing to match with
			continue;
		}
		if (link < block_start) {
			// Chain starts in the overlap region which is already encoded
			continue;
		}
		flag_table_8.Reset();
		do {
			radix_8 = match_buffer[link].chars[1];
			next_link = match_buffer[link].next[0];
			if (flag_table_8.IsSet(radix_8)) {
				const ptrdiff_t prev = prev_index_8[radix_8];
				match_buffer[prev].next[1] = static_cast<int_fast32_t>(link);
			}
			else {
				flag_table_8.Set(radix_8);
			}
			prev_index_8[radix_8] = link;
			link = next_link;
		} while (link >= 0);
	}
}

void MatchTableShort::Recurse4(const DataBlock& block, ptrdiff_t link)
{
	StringMatch* match_buffer = match_table.get();
	const ptrdiff_t block_start = block.start;
	int_fast32_t next_link = match_buffer[link].next[1];
	// Clear the bool flags
	flag_table_8.Reset();
	// Stack of new sub-list heads
	std::array<int_fast32_t, kStackSize> stack;
	size_t st_index = 0;
	ptrdiff_t prev_index_8[kRadixTableSizeSmall];
	while (next_link >= 0) {
		size_t radix_8 = match_buffer[link].chars[0];
		// Seen this char before?
		if (flag_table_8.IsSet(radix_8)) {
			const ptrdiff_t prev = prev_index_8[radix_8];
			// Link the previous occurrence to this one and record the new length
			match_buffer[prev].next[0] = static_cast<int_fast32_t>(link);
			match_buffer[prev].next[1] = -1;
		}
		else {
			flag_table_8.Set(radix_8);
			// Add the new sub list to the stack
			stack[st_index] = static_cast<int_fast32_t>(link);
			++st_index;
		}
		prev_index_8[radix_8] = link;
		link = next_link;
		next_link = match_buffer[link].next[1];
	}
	// Do the last element
	size_t radix_8 = match_buffer[link].chars[0];
	// Nothing to do if there was no previous
	if (flag_table_8.IsSet(radix_8)) {
		const ptrdiff_t prev = prev_index_8[radix_8];
		match_buffer[prev].next[0] = static_cast<int_fast32_t>(link);
		match_buffer[prev].next[1] = -1;
	}
	std::array<int_fast32_t, kStackSize> stack4;
	size_t st_index4 = 0;
	while (!g_break && st_index > 0) {
		// Pop an item off the stack
		--st_index;
		link = stack[st_index];
		if (match_buffer[link].next[0] < 0) {
			// Nothing to match with
			continue;
		}
		if (link < block_start) {
			// Chain starts in the overlap region which is already encoded
			continue;
		}
		flag_table_8.Reset();
		do {
			radix_8 = match_buffer[link].chars[1];
			next_link = match_buffer[link].next[0];
			if (flag_table_8.IsSet(radix_8)) {
				const ptrdiff_t prev = prev_index_8[radix_8];
				match_buffer[prev].next[1] = static_cast<int_fast32_t>(link);
				match_buffer[prev].next[2] = -1;
			}
			else {
				flag_table_8.Set(radix_8);
				stack4[st_index4] = static_cast<int_fast32_t>(link);
				++st_index4;
			}
			prev_index_8[radix_8] = link;
			link = next_link;
		} while (link >= 0);
		while (st_index4 > 0) {
			// Pop an item off the stack
			--st_index4;
			link = stack4[st_index4];
			if (match_buffer[link].next[1] < 0) {
				// Nothing to match with
				continue;
			}
			if (link < block_start) {
				// Chain starts in the overlap region which is already encoded
				continue;
			}
			flag_table_8.Reset();
			// The last pass at max_depth
			do {
				radix_8 = match_buffer[link].chars[2];
				next_link = match_buffer[link].next[1];
				if (flag_table_8.IsSet(radix_8)) {
					const ptrdiff_t prev = prev_index_8[radix_8];
					match_buffer[prev].next[2] = static_cast<int_fast32_t>(link);
				}
				else {
					flag_table_8.Set(radix_8);
				}
				prev_index_8[radix_8] = link;
				link = next_link;
			} while (link >= 0);
		}
	}
}

}