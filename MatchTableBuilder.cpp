///////////////////////////////////////////////////////////////////////////////
//
// Class: MatchTableBuilder
//        Create a table of string matches concurrently with other threads
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

#include "common.h"
#include "MatchTableBuilder.h"

namespace Radyx {

const unsigned MatchTableBuilder::kBufferedOverlapBits[2] = { 6, 4 };

void MatchTableBuilder::AllocateMatchBuffer(size_t match_buffer_size, unsigned match_buffer_overlap_)
{
	match_buffer_size = std::min<size_t>(match_buffer_size, kBufferLinkMask + 1);
	if (match_buffer_.size() < match_buffer_size) {
		match_buffer_.resize(match_buffer_size);
	}
	match_buffer_overlap = match_buffer_overlap_;
}

// Copy the list into a buffer and recurse it there. This decreases cache misses and allows
// data characters to be loaded every second pass and stored for use in the next 2 passes
void MatchTableBuilder::RecurseListsBuffered(const DataBlock& block,
	uint8_t depth,
	uint8_t max_depth,
	UintFast32 list_count,
	size_t stack_base) noexcept
{
	// Faster than vector operator[] and possible because the vector is not reallocated in this method
	StringMatch* match_buffer = match_buffer_.data();
	const size_t block_start = block.start;
	// Create an offset data buffer pointer for reading the next bytes
	const uint8_t base_depth = depth;
	++depth;
	// Clear the bool flags
	flag_table_8.Reset();
	// The last element is done separately and won't be copied back at the end
	--list_count;
	size_t st_index = stack_base;
	size_t prev_index_8[kRadixTableSizeSmall];
	size_t index = 0;
	do {
		size_t radix_8 = match_buffer[index].chars[0];
		// Seen this char before?
		if (flag_table_8.IsSet(radix_8)) {
			const size_t prev = prev_index_8[radix_8];
			++sub_tails[radix_8].list_count;
			// Link the previous occurrence to this one and record the new length
			match_buffer[prev].next = static_cast<UintFast32>(index) | (UintFast32(depth) << 24);
		}
		else {
			flag_table_8.Set(radix_8);
			sub_tails[radix_8].list_count = 1;
			// Add the new sub list to the stack
			stack[st_index].head = static_cast<UintFast32>(index);
			// This will be converted to a count at the end
			stack[st_index].count = static_cast<UintFast32>(radix_8);
			++st_index;
		}
		prev_index_8[radix_8] = index;
		++index;
	} while (index < list_count);
	// Do the last element
	size_t radix_8 = match_buffer[index].chars[0];
	// Nothing to do if there was no previous
	if (flag_table_8.IsSet(radix_8)) {
		const size_t prev = prev_index_8[radix_8];
		++sub_tails[radix_8].list_count;
		match_buffer[prev].next = static_cast<UintFast32>(index) | (UintFast32(depth) << 24);
	}
	// Convert radix values on the stack to counts
	for (size_t j = stack_base; j < st_index; ++j) {
		stack[j].count = sub_tails[stack[j].count].list_count;
	}
	while (!g_break && st_index > stack_base) {
		// Pop an item off the stack
		--st_index;
		list_count = stack[st_index].count;
		if (list_count < 2) {
			// Nothing to match with
			continue;
		}
		// Check stack space. The first comparison is unnecessary but it's a constant so should be faster
		if (st_index > stack.size() - kRadixTableSizeSmall
			&& st_index > stack.size() - list_count)
		{
			// Stack may not be able to fit all possible new items. This is very rare.
			continue;
		}
		index = stack[st_index].head;
		size_t link = match_buffer[index].from;
		if (link < block_start) {
			// Chain starts in the overlap region which is already encoded
			continue;
		}
		depth = match_buffer[index].next >> 24;
		// Check for overlapping repeated matches. These are rare but potentially a big time waster.
		if ((depth & kRptCheckMask) == 0 && list_count > kMaxOverlappingRpt + 1) {
			list_count = RepeatCheck(match_buffer, index, depth, list_count);
		}
		size_t slot = (depth - base_depth) & 3;
		if (list_count <= kMaxBruteForceListSize) {
			// Quicker to use brute force, each string compared with all previous strings
			BruteForceBuffered(block,
				match_buffer,
				index,
				list_count,
				slot,
				depth,
				max_depth);
			continue;
		}
		++depth;
		// Update the offset data buffer pointer
		const uint8_t* data_src = block.data + depth;
		flag_table_8.Reset();
		// Last pass is done separately
		if (depth < max_depth) {
			size_t prev_st_index = st_index;
			// Last element done separately
			--list_count;
			// slot is the char cache index. If 3 then chars need to be loaded.
			if (slot == 3) do {
				radix_8 = match_buffer[index].chars[3];
				const size_t next_index = match_buffer[index].next & kBufferLinkMask;
				// Pre-load the next link and data bytes to avoid waiting for RAM access
				if (depth < max_depth - 2) {
					Copy4Bytes(match_buffer[index].chars, data_src + link);
				}
				else {
					Copy2Bytes(match_buffer[index].chars, data_src + link);
				}
				const size_t next_link = match_buffer[next_index].from;
				if (flag_table_8.IsSet(radix_8)) {
					const size_t prev = prev_index_8[radix_8];
					++sub_tails[radix_8].list_count;
					match_buffer[prev].next = static_cast<UintFast32>(index) | (UintFast32(depth) << 24);
				}
				else {
					flag_table_8.Set(radix_8);
					sub_tails[radix_8].list_count = 1;
					stack[st_index].head = static_cast<UintFast32>(index);
					stack[st_index].count = static_cast<UintFast32>(radix_8);
					++st_index;
				}
				prev_index_8[radix_8] = index;
				index = next_index;
				link = next_link;
			} while (--list_count != 0);
			else do {
				radix_8 = match_buffer[index].chars[slot];
				const size_t next_index = match_buffer[index].next & kBufferLinkMask;
				// Pre-load the next link to avoid waiting for RAM access
				const size_t next_link = match_buffer[next_index].from;
				if (flag_table_8.IsSet(radix_8)) {
					const size_t prev = prev_index_8[radix_8];
					++sub_tails[radix_8].list_count;
					match_buffer[prev].next = static_cast<UintFast32>(index) | (UintFast32(depth) << 24);
				}
				else {
					flag_table_8.Set(radix_8);
					sub_tails[radix_8].list_count = 1;
					stack[st_index].head = static_cast<UintFast32>(index);
					stack[st_index].count = static_cast<UintFast32>(radix_8);
					++st_index;
				}
				prev_index_8[radix_8] = index;
				index = next_index;
				link = next_link;
			} while (--list_count != 0);
			radix_8 = match_buffer[index].chars[slot];
			if (flag_table_8.IsSet(radix_8)) {
				if (slot == 3) Copy4Bytes(match_buffer[index].chars, data_src + link);
				const size_t prev = prev_index_8[radix_8];
				++sub_tails[radix_8].list_count;
				match_buffer[prev].next = static_cast<UintFast32>(index) | (UintFast32(depth) << 24);
			}
			for (size_t j = prev_st_index; j < st_index; ++j) {
				stack[j].count = sub_tails[stack[j].count].list_count;
			}
		}
		else {
			// The last pass at max_depth
			do {
				radix_8 = match_buffer[index].chars[slot];
				const size_t next_index = match_buffer[index].next & kBufferLinkMask;
				// Pre-load the next link.
				// The last element in match_buffer is circular so this is never an access violation.
				const size_t next_link = match_buffer[next_index].from;
				if (flag_table_8.IsSet(radix_8)) {
					const size_t prev = prev_index_8[radix_8];
					match_buffer[prev].next = static_cast<UintFast32>(index) | (UintFast32(depth) << 24);
				}
				else {
					flag_table_8.Set(radix_8);
				}
				prev_index_8[radix_8] = index;
				index = next_index;
				link = next_link;
			} while (--list_count != 0);
		}
	}
}

UintFast32 MatchTableBuilder::RepeatCheck(StringMatch* match_buffer,
	size_t index,
	UintFast32 depth,
	UintFast32 list_count) noexcept
{
	int_fast32_t n = list_count - 1;
	int_fast32_t rpt = -1;
	size_t rpt_tail = index;
	do {
		size_t next_i = match_buffer[index].next & 0xFFFFFF;
		if (match_buffer[index].from - match_buffer[next_i].from <= depth) {
			if (++rpt == 0)
				rpt_tail = index;
		}
		else {
			if (rpt > kMaxOverlappingRpt - 1) {
				match_buffer[rpt_tail].next = static_cast<UintFast32>(index) | (UintFast32(depth) << 24);
				list_count -= rpt;
			}
			rpt = -1;
		}
		index = next_i;
	} while (--n);
	if (rpt > kMaxOverlappingRpt - 1) {
		match_buffer[rpt_tail].next = static_cast<UintFast32>(index) | (UintFast32(depth) << 24);
		list_count -= rpt;
	}
	return list_count;
}

void MatchTableBuilder::BruteForceBuffered(const DataBlock& block,
	StringMatch* match_buffer,
	size_t index,
	size_t list_count,
	size_t slot,
	size_t depth,
	size_t max_depth) noexcept
{
	BruteForceMatch buffer[kMaxBruteForceListSize + 1];
	const uint8_t* data_src = block.data + depth;
	size_t i = 0;
	for (;;) {
		buffer[i].index = index;
		buffer[i].data_src = data_src + match_buffer[index].from;
		*(uint32_t*)buffer[i].chars = *(uint32_t*)match_buffer[index].chars;
		if (++i >= list_count) {
			break;
		}
		index = match_buffer[index].next & 0xFFFFFF;
	}
	size_t limit = max_depth - depth;
	const uint8_t* start = data_src + block.start;
	i = 0;
	do {
		size_t longest = 0;
		size_t j = i + 1;
		size_t longest_index = j;
		const uint8_t* data = buffer[i].data_src;
		do {
			size_t len_test = slot;
			while (len_test < 4 && buffer[i].chars[len_test] == buffer[j].chars[len_test] && len_test - slot < limit) {
				++len_test;
			}
			len_test -= slot;
			if (len_test) {
				const uint8_t* data_2 = buffer[j].data_src;
				while (data[len_test] == data_2[len_test] && len_test < limit) {
					++len_test;
				}
			}
			if (len_test > longest) {
				longest_index = j;
				longest = len_test;
				if (len_test >= limit) {
					break;
				}
			}
		} while (++j < list_count);
		if (longest > 0) {
			index = buffer[i].index;
			match_buffer[index].next = static_cast<UintFast32>(buffer[longest_index].index | ((depth + longest) << 24));
		}
		++i;
	} while (i < list_count - 1 && buffer[i].data_src >= start);
}

}