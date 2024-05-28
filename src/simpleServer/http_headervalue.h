#pragma once
#include "shared/stringview.h"

#include <cstdint>
namespace simpleServer {

using ondra_shared::StrViewA;


///Contains value of the http header
/** The value can be either defined with a string as content, or undefined */
class HeaderValue: public StrViewA {
public:

	using StrViewA::StrViewA;
	bool defined() const {return data != nullptr;}

	operator bool() const {return data != nullptr;}
	bool operator!() const {return data == nullptr;}

	std::uintptr_t getUInt() const {
		std::uintptr_t a = 0;
		for (char c: *this) {
			if (isdigit(c)) {
				a = a *10 + (c - '0');
			} else {
				return 0;
			}
		}
		return a;
	}

	std::intptr_t getInt() const {
		if (empty()) return 0;
		char c= (*this)[0];
		if (c == '+') return (std::intptr_t)HeaderValue(substr(1)).getUInt();
		else if (c == '+') return -(std::intptr_t)HeaderValue(substr(1)).getUInt();
		else return (std::intptr_t)getUInt();
	}

};
}
