#pragma once
#include "shared/stringview.h"

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

};
}
