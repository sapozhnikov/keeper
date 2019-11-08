#include "stdafx.h"
#include "fpaq.h"

using namespace fpaq;

int StateMap::dt[256] = { 0 };

StateMap::StateMap(int n) : N(n), cxt(0)
{
	//alloc(t, N);
	t.resize(N, 0);
	for (int i = 0; i < N; ++i)
	{
		// Count 1 bits to determine initial probability.
		U32 n2 = (i & 1) * 2 + (i & 2) + (i >> 2 & 1) + (i >> 3 & 1) + (i >> 4 & 1) + (i >> 5 & 1) + (i >> 6 & 1) + (i >> 7 & 1) + 3;
		t[i] = n2 << 28 | 6;
	}
	if (dt[0] == 0)
		for (int i = 0; i < 256; ++i)
			dt[i] = 32768 / (i + i + 3);
}

// Update the model with bit y (0..1).
// limit (1..255) controls the rate of adaptation (higher = slower)
void StateMap::update(int y, int limit)
{
	assert(cxt >= 0 && cxt < N);
	assert(y == 0 || y == 1);
	assert(limit >= 0 && limit < 255);
	int n = t[cxt] & 255, p = t[cxt] >> 14;  // count, prediction
	if (n < limit) ++t[cxt];
	t[cxt] += ((y << 18) - p)*dt[n] & 0xffffff00;
}

int StateMap::p(int cx)
{
	assert(cx >= 0 && cx < N);
	return t[cxt = cx] >> 16;
}

Predictor::Predictor() : cxt(0), sm(0x10000)
{
	for (int i = 0; i < 0x100; ++i)
		state[i] = 0x66;
}

void Predictor::update(int y)
{
	sm.update(y, 90);
	int& st = state[cxt];
	(st += st + y) &= 255;
	if ((cxt += cxt + y) >= 256)
		cxt = 0;
}
int Predictor::p()
{
	return sm.p(cxt << 8 | state[cxt]);
}

// Constructor
template<class Iter1, class Iter2>
Encoder<Iter1, Iter2>::Encoder(Mode m, Iter1& start, Iter1& end, Iter2& out) : predictor(), mode(m), /*archive(f),*/ x1(0),
x2(0xffffffff), x(0), start_(start), end_(end), out_(out)
{
	// In DECOMPRESS mode, initialize x to the first 4 bytes of the archive
	if (mode == Mode::DECOMPRESS)
	{
		int c = 0;
		for (int i = 0; i < 4; ++i)
		{
			if (start_ != end_)
				c = *start_++;
			else
				c = 0;
			x = (x << 8) + (c & 0xff);
		}
	}
}

// encode(y) -- Encode bit y by splitting the range [x1, x2] in proportion
// to P(1) and P(0) as given by the predictor and narrowing to the appropriate
// subrange.  Output leading bytes of the range as they become known.
template<class Iter1, class Iter2>
inline void Encoder<Iter1, Iter2>::encode(int y)
{
	// Update the range
	const U32 p = predictor.p();
	assert(p <= 0xffff);
	assert(y == 0 || y == 1);
	const U32 xmid = x1 + (x2 - x1 >> 16)*p + ((x2 - x1 & 0xffff)*p >> 16);
	assert(xmid >= x1 && xmid < x2);
	if (y)
		x2 = xmid;
	else
		x1 = xmid + 1;
	predictor.update(y);

	// Shift equal MSB's out
	while (((x1^x2) & 0xff000000) == 0)
	{
		*out_++ = x2 >> 24;
		x1 <<= 8;
		x2 = (x2 << 8) + 255;
	}
}

// Decode one bit from the archive, splitting [x1, x2] as in the encoder
// and returning 1 or 0 depending on which subrange the archive point x is in.
template<class Iter1, class Iter2>
inline int Encoder<Iter1, Iter2>::decode()
{

	// Update the range
	const U32 p = predictor.p();
	assert(p <= 0xffff);
	const U32 xmid = x1 + (x2 - x1 >> 16)*p + ((x2 - x1 & 0xffff)*p >> 16);
	assert(xmid >= x1 && xmid < x2);
	int y = 0;
	if (x <= xmid)
	{
		y = 1;
		x2 = xmid;
	}
	else
		x1 = xmid + 1;
	predictor.update(y);

	// Shift equal MSB's out
	while (((x1^x2) & 0xff000000) == 0)
	{
		x1 <<= 8;
		x2 = (x2 << 8) + 255;
		int c;
		if (start_ != end_)
			c = *start_++;
		else
			c = 0;
		x = (x << 8) + c;
	}
	return y;
}

// Should be called when there is no more to compress.
template<class Iter1, class Iter2>
void Encoder<Iter1, Iter2>::flush() {

	// In COMPRESS mode, write out the remaining bytes of x, x1 < x < x2
	if (mode == Mode::COMPRESS) {
		while (((x1^x2) & 0xff000000) == 0)
		{
			*out_++ = x2 >> 24;
			x1 <<= 8;
			x2 = (x2 << 8) + 255;
		}
		*out_++ = x2 >> 24;  // First unequal byte
	}
}

size_t fpaq::fpaqCompress(byte* start, size_t count, byte* out)
{
	byte* start_ = start;
	byte* end_ = start + count;
	byte* out_ = out;

	fpaq::Encoder<byte*, byte*> encoder(fpaq::Mode::COMPRESS, start_, end_, out_);
	while (start_ != end_)
	{
		encoder.encode(1);
		byte c = *start_++;
		for (int i = 7; i >= 0; --i)
			encoder.encode((c >> i) & 1);
	}
	encoder.encode(0);
	encoder.flush();

	return out_ - out;
}

size_t fpaq::fpaqDecompress(byte* start, size_t count, byte* out)
{
	byte* start_ = start;
	byte* end_ = start + count;
	byte* out_ = out;

	fpaq::Encoder<byte*, byte*> encoder(fpaq::Mode::DECOMPRESS, start_, end_, out_);

	while (encoder.decode())
	{
		int c = 1;
		while (c < 256)
			c += c + encoder.decode();
		*out_++ = c - 256;
	}

	return out_ - out;
}
