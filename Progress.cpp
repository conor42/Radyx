///////////////////////////////////////////////////////////////////////////////
//
// Class: Progress
//        Progress meter
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

#include "common.h"
#include "Progress.h"

namespace Radyx {

Progress::Progress()
	: total_bytes(0),
	progress_bytes(0),
	next_update(100),
	build_weight(0),
	encode_weight(0),
	display_length(0)
{
}

Progress::~Progress()
{
	Erase();
}

void Progress::Init(uint_least64_t total_bytes_, unsigned encode_weight_)
{
	total_bytes = total_bytes_;
	next_update = total_bytes_ / 100;
	build_weight = kWeightUnitTotal - encode_weight_;
	encode_weight = encode_weight_;
}

unsigned Progress::ShowLocked()
{
	if (display_length != 0) {
		RewindLocked();
	}
	unsigned percent = static_cast<unsigned>(progress_bytes * 100 / (total_bytes + (total_bytes == 0)));
	if (!g_break) {
#ifdef _UNICODE
		_TCHAR buf[8];
		display_length = swprintf_s(buf, L" %u%%", percent);
		std::wcerr << buf;
#else
		display_length = fprintf(stderr, " %u%%", percent);
#endif
	}
	return percent;
}

void Progress::RewindLocked()
{
	if (!g_break) {
		std::Tcerr.write(_T("\b\b\b\b\b\b\b"), display_length & 7);
		display_length = 0;
	}
}

void Progress::Update(size_t bytes_done)
{
	uint_least64_t progress = progress_bytes.fetch_add(bytes_done);
	if (progress >= next_update) {
		std::unique_lock<std::mutex> lock(mtx);
		if (progress >= next_update) {
			uint_least64_t percent = ShowLocked() + 1;
			next_update = total_bytes * percent / 100;
		}
	}
}

}