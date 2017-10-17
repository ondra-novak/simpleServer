#include <map>
#include <vector>

#include "http_headervalue.h"

#include "stringview.h"

#pragma once

namespace simpleServer {


class KeyValueHeaders {
public:
	typedef std::vector<char> TextBuffer;



	void parse(StrViewA hdrText);
	void parse(TextBuffer &&buffer);


	HeaderValue operator[](const StrViewA &field);

	void clear();
	void add(const StrViewA key, const StrViewA value);

/*	template<typename Fn>
	bool enumHeaders(Fn fn) {
		for (auto &&x : hdrMap) {
			if (!fn(HeaderKeyValue(x.first, x.second))) return false;
1		}
		return true;
	}
*/
	std::string serialize();

protected:


	class StrItem {
	public:

		StrItem(const TextBuffer &data, std::size_t offset, std::size_t length);
		StrItem(StrViewA data);
		StrItem();

		operator StrViewA() const;

	protected:
		std::size_t length;
		const std::vector<char> *data;
		union {
			const char *ptr;
			std::size_t offset;
		};
	};

	struct StrItemOrder {
		bool operator()(const StrItem &a, const StrItem &b) const;
	};

	typedef std::map<StrItem, StrItem, StrItemOrder> HdrMap;
	HdrMap hdrMap;
	TextBuffer txtBuffer;


	void parse_internal();
	StrItem addStrItem(StrViewA txt);
};

} /* namespace simpleServer */

