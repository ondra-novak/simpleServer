#include "query_parser.h"

#include "urlencode.h"

namespace simpleServer {



QueryParser::QueryParser(StrViewA vpath) {
	parse(vpath);
}

QueryParser::ParamMap::const_iterator QueryParser::begin() const {
	return pmap.begin();
}

QueryParser::ParamMap::const_iterator QueryParser::end() const {
	return pmap.end();
}

HeaderValue QueryParser::operator [](StrViewA key) const {
	Item srch;
	srch.first = key;
	auto itr = std::lower_bound(pmap.begin(), pmap.end(), srch, &orderItems);
	if (itr == pmap.end() || itr->first != key) return HeaderValue();
	else return HeaderValue(itr->second);
}

void QueryParser::clear() {
}

typedef std::pair<std::size_t, std::size_t> Part;
typedef std::pair<Part, Part> KeyValuePart;

void QueryParser::parse(StrViewA vpath) {

	enum State {
		readingPath,
		readingKey,
		readingValue
	};


	clear();
	std::vector<KeyValuePart> parts;
	std::vector<char> data;

	Part pathPart;
	Part keyPart;
	State state =readingPath;


	auto iter = vpath.begin();
	auto e = vpath.end();
	std::size_t mark = data.size();

	auto wrfn = [&](char c) {
		data.push_back(c);
	};

	auto commitData = [&] {
		Part p(mark, data.size() - mark);
		data.push_back((char)0);
		mark = data.size();
		return p;
	};

	UrlDecode<decltype(wrfn)> urlDecode(wrfn);

	while (iter != e) {
		char c = *iter++;

		switch (state) {
		case readingPath:
			if (c == '?') {
				pathPart = commitData();
				state = readingKey;
			} else{
				wrfn(c);
			}
			break;
		case readingKey:
			if (c == '&') {
				keyPart = commitData();
				parts.push_back(KeyValuePart(keyPart,Part(0,0)));
			} else if (c == '=') {
				keyPart = commitData();
				state = readingValue;
			} else {
				wrfn(c);
			}
			break;
		case readingValue:
			if (c == '&') {
				Part v = commitData();
				parts.push_back(KeyValuePart(keyPart, v));
				state = readingKey;
			} else {
				wrfn(c);
			}
			break;
		}
	}

	switch (state) {
	case readingPath:
		pathPart = commitData();
		break;
	case readingKey:
		keyPart = commitData();
		parts.push_back(KeyValuePart(keyPart,Part(0,0)));
		break;
	case readingValue:
		Part v = commitData();
		parts.push_back(KeyValuePart(keyPart, v));
		break;
	}

	pmap.clear();
	pmap.reserve(parts.size());
	for (auto &&x: parts) {
		StrViewA key (data.data()+x.first.first, x.first.second);
		StrViewA value (data.data()+x.second.first, x.second.second);
		pmap.push_back(Item(key,value));
	}
	path = StrViewA(data.data()+pathPart.first,pathPart.second);
	std::sort(pmap.begin(),pmap.end(),&orderItems);
	std::swap(this->data, data);
}

StrViewA QueryParser::getPath() const {
	return path;
}

bool QueryParser::orderItems(const Item& a, const Item& b) {
	return a.first < b.first;
}

}
