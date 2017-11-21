///////////////////////////////////////////////////////////////////////////////
//
// Class:   BcjTransform
//          Abstract base class for BCJ transforms
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

#ifndef RADYX_BCJ_TRANSFORM_H
#define RADYX_BCJ_TRANSFORM_H

#include "DataBlock.h"
#include "CoderInfo.h"

namespace Radyx {

class BcjTransform : public InPlaceFilter
{
public:
	static const size_t kMaxUnprocessed = 4;

	virtual inline ~BcjTransform();
	virtual size_t Transform(uint8_t* buffer, size_t main_end, size_t block_end, bool encoding) = 0;
};

BcjTransform::~BcjTransform()
{
}

}

#endif // RADYX_BCJ_TRANSFORM_H