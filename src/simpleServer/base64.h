#pragma once

#include "stringview.h"

namespace simpleServer {

template<typename WriteFn>
class Base64Encode {
public:
	Base64Encode(const WriteFn &fn, const char *chars, bool notail)
		:fn(fn),nx(0),rdpos(0),chars(chars),notail(notail) {}
	~Base64Encode() {
		finish();
	}

	void operator()(int b) {
		if (b == -1) finish();
		else  {
			int c = b & 0xFF;
			switch (rdpos) {
			case 0:
				fn(chars[c >> 2]);
				nx = (c << 4) & 0x3F;
				break;
			case 1:
				fn(chars[nx | (c >> 4)]);
				nx = (c << 2) & 0x3F;
				break;
			case 2:
				fn(chars[nx | (c >> 6)]);
				fn(chars[c & 0x3F]);
				break;
			}

			rdpos = (rdpos+1) % 3;
		}
	}

	void finish() {
		switch (rdpos) {
		case 1: fn(chars[nx]);
				if (!notail) {
					fn('=');
					fn('=');
				}
				break;
		case 2: fn(chars[nx]);
				if (!notail) fn('=');
				break;
		}
		rdpos = 0;
	}


protected:
	WriteFn fn;
	unsigned char nx;
	unsigned char rdpos;
	const char *chars;
	bool notail;


};


extern const char *base64_standard;
extern const char *base64_url;

template<typename Fn> Base64Encode<Fn> base64encode(const Fn &output, const char *chars = base64_standard, bool notail = false) {
	return Base64Encode<Fn>(output, chars, notail);
}

inline static std::string base64encode(BinaryView bin, const char *chars = base64_standard, bool notail = false) {
	std::string s;
	s.reserve((bin.length*4+2)/3);
	auto enc = base64encode([&](char c) {s.push_back(c);}, chars, notail);
	for(auto &&c:bin) enc(c);
	return s;
}

}

