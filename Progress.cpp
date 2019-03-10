///////////////////////////////////////////////////////////////////////////////
//
// Class: Progress
//        Progress meter
//
// Copyright 2015-present Conor McCarthy
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

Progress::Progress(uint_least64_t total_bytes_)
	: total_bytes(total_bytes_),
    prev_done(0),
	progress_bytes(0),
	next_update(total_bytes_ / 100),
	display_length(0)
{
}

Progress::~Progress()
{
	Erase();
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
		std::Tcerr << _T('\r');
		display_length = 0;
	}
}

void Progress::Update(uint_least64_t bytes_done)
{
    progress_bytes = prev_done + bytes_done;
	if (bytes_done >= next_update) {
		std::unique_lock<std::mutex> lock(mtx);
		if (bytes_done >= next_update) {
			uint_least64_t percent = ShowLocked() + 1;
			next_update = total_bytes * percent / 100;
		}
	}
}

void Progress::AddUnit(uint_least64_t unit_size)
{
    prev_done += unit_size;
    progress_bytes = prev_done;
}

}