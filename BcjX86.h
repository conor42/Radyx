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

#ifndef RADYX_BCJ_X86_H
#define RADYX_BCJ_X86_H

#include "BcjTransform.h"

namespace Radyx {

class BcjX86 : public BcjTransform
{
public:
	BcjX86() noexcept;
	size_t Transform(MutableDataBlock& block, bool encoding) noexcept;
	void Reset() noexcept;
	CoderInfo GetCoderInfo() const noexcept;

private:
	static const bool kMaskToAllowedStatus[8];
	static const uint8_t kMaskToBitNumber[8];

	inline bool Test86MSByte(uint8_t b) const noexcept {
		return uint8_t(b + 1) < 2;
	}

	size_t ip;
	size_t prev_mask;
};

}

#endif // RADYX_BCJ_X86_H