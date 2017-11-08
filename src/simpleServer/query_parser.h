#pragma once
#include "shared/stringview.h"

#include "http_headervalue.h"

using ondra_shared::StrViewA;


namespace simpleServer {

class QueryParser{

public:
	QueryParser() {}
	explicit QueryParser(StrViewA vpath);

	typedef std::pair<StrViewA, StrViewA> Item;
	typedef std::vector<Item> ParamMap;

	///Begin of the headers
	ParamMap::const_iterator begin() const;
	///End of the headers
	ParamMap::const_iterator end() const;

	///Retrieves header value
	HeaderValue operator[](StrViewA key) const;

	void clear();

	bool empty() const {return pmap.empty();}

	void parse(StrViewA vpath);

	StrViewA getPath() const;

protected:
	ParamMap pmap;
	StrViewA path;
	std::vector<char> data;

	static bool orderItems(const Item &a, const Item &b);

};













}
