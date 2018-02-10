#include "OutStream7z.h"

OutputStream& OutStream7z::Write(const char* s, size_t n)
{
	HRESULT err;
	do {
		UInt32 outSize = 0;
		err = stream->Write((void*)s, (n > UINT32_MAX) ? UINT32_MAX : (UInt32)n, &outSize);
		n -= outSize;
		s += outSize;
	} 
	while (err == S_OK && n != 0);

	failbit |= (err != S_OK);
}
