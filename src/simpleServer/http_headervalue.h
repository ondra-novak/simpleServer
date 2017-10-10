#pragma once

namespace simpleServer {

///Contains value of the http header
/** The value can be either defined with a string as content, or undefined */
class HeaderValue: public StrViewA {
public:
	///it is set to true, when value is defined
	bool defined;
	HeaderValue():defined(false) {}
	HeaderValue(const StrViewA &str):defined(true),StrViewA(str) {}
};


class HeaderKeyValue {
public:

	StrViewA key;
	HeaderValue value;

	HeaderKeyValue(StrViewA key,HeaderValue value):key(key),value(value) {}
	HeaderKeyValue() {}

};
}
