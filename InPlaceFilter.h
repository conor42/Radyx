#pragma once

#include "common.h"
#include <list>
#include <memory>
#include "CoderInfo.h"

namespace Radyx {

class InPlaceFilter
{
public:
	InPlaceFilter()
		: enabled(true) {}
	virtual size_t GetMaxOverrun() const noexcept = 0;
	virtual size_t Encode(uint8_t* buffer, size_t main_end, size_t block_end) = 0;
	virtual size_t Decode(uint8_t* buffer, size_t main_end, size_t block_end) = 0;
	virtual void Reset() noexcept = 0;
	virtual CoderInfo GetCoderInfo() const noexcept = 0;

	virtual void Enable(bool enable) noexcept {
		enabled = enable;
	}
	virtual bool IsEnabled() const noexcept {
		return enabled;
	}

private:
	bool enabled;
};

typedef std::list<std::unique_ptr<InPlaceFilter>> FilterList;

} //Radyx