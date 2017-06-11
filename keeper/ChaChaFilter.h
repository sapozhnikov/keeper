#pragma once
#include <boost\iostreams\concepts.hpp>
#include <boost\iostreams\filter\stdio.hpp>
#include <boost\iostreams\categories.hpp>

using namespace boost::iostreams;

class ChaCha20filter
{
public:
	typedef char char_type;
	struct category : output_filter_tag, flushable_tag, multichar_tag { };

	explicit ChaCha20filter(const byte* key, const byte* nonce)
	{
		memcpy(key_, key, crypto_stream_chacha20_KEYBYTES);
		memcpy(nonce_, nonce, crypto_stream_chacha20_NONCEBYTES);
	}

	~ChaCha20filter()
	{
		sodium_memzero(key_, crypto_stream_chacha20_KEYBYTES);
		sodium_memzero(nonce_, crypto_stream_chacha20_NONCEBYTES);
	}

	ChaCha20filter& operator=(const ChaCha20filter&) = delete;

	//WARNING!!! it's writes to the input buffer!!!
	template<typename Sink>
	std::streamsize write(Sink& dest, const char* s, std::streamsize n)
	{
		int64_t inlen = int(n);

		crypto_stream_chacha20_xor_ic((unsigned char*)s, (unsigned char*)s, inlen, nonce_, counter_, key_);
		counter_ += inlen;

		boost::iostreams::write(dest, s, inlen);
		return n;
	}

	template<typename Sink>
	bool flush(Sink& /*dest*/)
	{
		if (!isFlushed_)
		{
			//nothing to do
			isFlushed_ = true;
		}
		return true;
	}

private:

	unsigned char key_[crypto_stream_chacha20_KEYBYTES];
	unsigned char nonce_[crypto_stream_chacha20_NONCEBYTES];
	uint64_t counter_ = 0;

	bool isFlushed_ = false;
};
