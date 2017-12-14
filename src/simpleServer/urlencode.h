#pragma once

#include <cstring>
#include "shared/constref.h"
#include "shared/stringview.h"

namespace simpleServer {



using ondra_shared::const_ref;
using ondra_shared::StrViewA;

extern const char *urlEncode_validChards;

template<typename Fn>
class UrlEncode {
public:
	UrlEncode(const_ref<Fn> fn):fn(fn) {}

	void operator()(char item) const {
		//char validchrs =
		if (std::strchr(urlEncode_validChards, item) == 0) {
			fn('%');
			unsigned char h = ((unsigned char )item)/16;
			fn(h<10?'0'+h:'A'+h-10);
			unsigned char l = ((unsigned char )item)%16;
			fn(l<10?'0'+l:'A'+l-10);
		} else {
			fn(item);
		}
	}
protected:
	Fn fn;

};

template<typename Fn>
class UrlDecode {
public:

	UrlDecode(const_ref<Fn> fn):fn(fn),acc(0) {}

	void operator()(char item) const {

		if (item == '%') {
			phase = 1;
			acc = 0;
		} else if (phase) {
			if (item >= '0' && item <= '9') acc = acc * 16 + (item - '0');
			else if (item >= 'A' && item <= 'F') acc = acc * 16 + (item - 'A'+10);
			else if (item >= 'a' && item <= 'f') acc = acc * 16 + (item - 'a'+10);
			phase++;
			if (phase==3) {
				phase = 0;
				fn(acc);
			}
		} else {
			fn(item);
		}

	}
protected:
	Fn fn;
	mutable unsigned char acc;
	mutable unsigned char phase = 0;

};

}
