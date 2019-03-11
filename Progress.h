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

#ifndef RADYX_PROGRESS_H
#define RADYX_PROGRESS_H

#include <atomic>
#include <mutex>
#include <iostream>
#include "CharType.h"

namespace Radyx {

class Progress
{
public:
	Progress(uint_least64_t total_bytes_);
	~Progress();
	inline void Show();
	inline void Rewind();
	void RewindLocked();
	inline void Erase();
    void Update(uint_least64_t bytes_done);
    void AddUnit(uint_least64_t unit_size);
    inline void Adjust(int_least64_t size_change);
	inline std::mutex& GetMutex() { return mtx; }

private:
	unsigned ShowLocked();

	uint_least64_t total_bytes;
    uint_least64_t prev_done;
    uint_least64_t progress_bytes;
	uint_least64_t next_update;
	std::mutex mtx;
	int display_length;

	Progress(const Progress&) = delete;
	Progress& operator=(const Progress&) = delete;
};

void Progress::Adjust(int_least64_t size_change)
{
	total_bytes += size_change;
}

void Progress::Show()
{
	if (display_length == 0) {
		std::unique_lock<std::mutex> lock(mtx);
		ShowLocked();
	}
}

void Progress::Rewind()
{
	std::unique_lock<std::mutex> lock(mtx);
	RewindLocked();
}

void Progress::Erase()
{
    Rewind();
    std::Tcerr << "     \b\b\b\b\b";
}

}

#endif // RADYX_PROGRESS_H