#pragma once

//fpaq0f2 - Adaptive order 0 compressor.
//originally implemented by Matt Mahoney, 2008
#include <cassert>
#include <vector>

namespace fpaq
{
	typedef unsigned char  U8;
	typedef unsigned short U16;
	typedef unsigned int   U32;

	// A StateMap maps a context to a probability.  After a bit prediction, the
	// map is updated in the direction of the actual value to improve future
	// predictions in the same context.

	class StateMap
	{
	protected:
		const int N;  // Number of contexts
		int cxt;      // Context of last prediction
		//U32 *t;       // cxt -> prediction in high 24 bits, count in low 8 bits
		std::vector<U32> t;
		static int dt[256];  // reciprocal table: i -> 16K/(i+1.5)
	public:
		StateMap(int n = 256);  // create allowing n contexts

		// Predict next bit to be updated in context cx (0..n-1).
		// Return prediction as a 16 bit number (0..65535) that next bit is 1.
		int p(int cx);

		// Update the model with bit y (0..1).
		// limit (1..255) controls the rate of adaptation (higher = slower)
		void update(int y, int limit = 255);
	};


	/* A Predictor estimates the probability that the next bit of
	   uncompressed data is 1.  Methods:
	   p() returns P(1) as a 16 bit number (0..65535).
	   update(y) trains the predictor with the actual bit (0 or 1).
	*/
	class Predictor
	{
		int cxt;  // Context: 0=not EOF, 1..255=last 0-7 bits with a leading 1
		StateMap sm;
		int state[256];
	public:
		Predictor();
		// Assume order 0 stream of 9-bit symbols
		int p();
		void update(int y);
	};

	/* An Encoder does arithmetic encoding.  Methods:
	   Encoder(COMPRESS, f) creates encoder for compression to archive f, which
		 must be open past any header for writing in binary mode
	   Encoder(DECOMPRESS, f) creates encoder for decompression from archive f,
		 which must be open past any header for reading in binary mode
	   encode(bit) in COMPRESS mode compresses bit to file f.
	   decode() in DECOMPRESS mode returns the next decompressed bit from file f.
	   flush() should be called when there is no more to compress.
	*/

	enum class Mode
	{
		COMPRESS,
		DECOMPRESS
	};
	
	template<class Iter1, class Iter2>
	class Encoder
	{
	private:
		Predictor predictor;   // Computes next bit probability (0..65535)
		const Mode mode;       // Compress or decompress?
		U32 x1, x2;            // Range, initially [0, 1), scaled by 2^32
		U32 x;                 // Last 4 input bytes of archive.
		//yeah, it is bad and i'm feel bad
		Iter1& start_;
		const Iter1& end_;
		Iter2& out_;
	public:
		Encoder(Mode m, Iter1& start, Iter1& end, Iter2& out);
		void encode(int y);    // Compress bit y
		int decode();          // Uncompress and return bit y
		void flush();          // Call when done compressing
	};

	size_t fpaqCompress(byte* start, size_t count, byte* out);
	size_t fpaqDecompress(byte* start, size_t count, byte* out);
}
