#pragma once

#include "shared/constref.h"
#include "shared/stringview.h"


namespace simpleServer {

using ondra_shared::const_ref;
using ondra_shared::StrViewA;

template<typename Fn>
class HtmlEscape {
public:
	HtmlEscape(const_ref<Fn> fn):fn(fn) {}

	void operator()(char item) {
		switch (item) {
		case '<': send("&lt;");break;
		case '>': send("&gt;");break;
		case '&': send("&amp;");break;
		case '"': send("&quot;");break;
		default: fn(item);break;
		}

	}

	void operator()(StrViewA txt) {
		for (auto x: txt) (*this)(x);
	}

protected:
	Fn fn;

	void send(const char *x) {
		while (*x) {
			fn(*x);
			++x;
		}
	}
};




}
