///////////////////////////////////////////////////////////////////////////////
//
// Class:   ArchiveStreamIn
//          Interface for reading data into a buffer
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

#ifndef RADYX_ARCHIVE_STREAM_IN_H
#define RADYX_ARCHIVE_STREAM_IN_H

#include "common.h"

namespace Radyx {

class ArchiveStreamIn
{
public:
	virtual size_t Read(uint8_t* const buffer, size_t const length) = 0;
	virtual bool Complete() const noexcept = 0;
};

} // Radyx

#endif