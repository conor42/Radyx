///////////////////////////////////////////////////////////////////////////////
//
// Class:   BcjX86
//          BCJ transform for x86/x64
//          
// Authors: Igor Pavlov
//          Conor McCarthy
//
// Copyright 2015 Conor McCarthy
// Based on 7-zip 9.20 copyright 2010 Igor Pavlov
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

#include "BcjX86.h"

namespace Radyx {

const bool BcjX86::kMaskToAllowedStatus[8] = { 1, 1, 1, 0, 1, 0, 0, 0 };
const uint8_t BcjX86::kMaskToBitNumber[8] = { 0, 1, 2, 2, 3, 3, 3, 3 };

BcjX86::BcjX86()
	: ip(0),
	prev_mask(0)
{
}

size_t BcjX86::Transform(MutableDataBlock& block, bool encoding)
{
	size_t index = block.start;
	size_t offset_ip = ip + 5 - index;
	size_t prev_index = index - 1;
	uint8_t* data_block = block.data;
	size_t end = block.end;
	size_t limit = end - 4;
	for (;;)
	{
		for (; index < end; ++index) {
			if ((data_block[index] & 0xFE) == 0xE8) {
				break;
			}
		}
		prev_index = index - prev_index;
		if (index >= limit) {
			break;
		}
		if (prev_index > 3) {
			prev_mask = 0;
		}
		else {
			prev_mask = (prev_mask << (prev_index - 1)) & 7;
			if (prev_mask != 0) {
				if (!kMaskToAllowedStatus[prev_mask]
					|| Test86MSByte(data_block[index + 4 - kMaskToBitNumber[prev_mask]])) {
					prev_index = index;
					prev_mask = ((prev_mask << 1) & 7) | 1;
					++index;
					continue;
				}
			}
		}
		prev_index = index;
		uint8_t hibyte = data_block[index + 4];
		if (Test86MSByte(hibyte)) {
			uint_fast32_t src = (uint_fast32_t(hibyte) << 24)
				| (uint_fast32_t(data_block[index + 3]) << 16)
				| (uint_fast32_t(data_block[index + 2]) << 8)
				| data_block[index + 1];
			uint_fast32_t dest;
			for (;;) {
				if (encoding) {
					dest = static_cast<uint_fast32_t>(offset_ip + index) + src;
				}
				else {
					dest = src - static_cast<uint_fast32_t>(offset_ip + index);
				}
				if (prev_mask == 0) {
					break;
				}
				uint8_t shift = kMaskToBitNumber[prev_mask] * 8u;
				uint8_t b = static_cast<uint8_t>(dest >> (24u - shift));
				if (!Test86MSByte(b)) {
					break;
				}
				src = dest ^ ((1 << (32u - shift)) - 1);
			}
			data_block[index + 4] = static_cast<uint8_t>(~(((dest >> 24) & 1) - 1));
			data_block[index + 3] = static_cast<uint8_t>(dest >> 16);
			data_block[index + 2] = static_cast<uint8_t>(dest >> 8);
			data_block[index + 1] = static_cast<uint8_t>(dest);
			index += 5;
		}
		else {
			prev_mask = ((prev_mask << 1) & 7) | 1;
			++index;
		}
	}
	ip = offset_ip - 5 + index;
	prev_mask = ((prev_index > 3) ? 0 : ((prev_mask << (prev_index - 1)) & 0x7));
	return index;
}

void BcjX86::Reset()
{
	ip = 0;
	prev_mask = 0;
}

CoderInfo BcjX86::GetCoderInfo() const
{
	return CoderInfo(nullptr, 0, 0x03030103, 1, 1);
}

}