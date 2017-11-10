#ifndef RADYX_OUT_STREAM_7Z_H
#define RADYX_OUT_STREAM_7Z_H

#include "../../7-zip-zstd/CPP/7zip/IStream.h"
#include "../OutputStream.h"

class OutStream7z : public OutputStream
{
public:
	OutStream7z(ISequentialOutStream* stream_)
		: stream(stream_),
		failbit(false)
	{}

	OutputStream& Put(char c) {
		Write(&c, 1);
	}
	OutputStream& Write(const char* s, size_t n);
	void DisableExceptions() {}
	void RestoreExceptions() {}
	bool Fail() const noexcept {
		return failbit;
	}

private:
	ISequentialOutStream* stream;
	bool failbit;
};

#endif