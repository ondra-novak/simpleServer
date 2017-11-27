#pragma once
#include "shared/stringview.h"
#include "http_parser.h"

using ondra_shared::StrViewA;

namespace simpleServer {

class HttpFileMapper {
public:

	HttpFileMapper(std::string &&documentRoot, std::string &&index);

	virtual bool mapFile(const HTTPRequest &req, StrViewA fullPathname);
	virtual StrViewA mapMime(StrViewA fullPathname);


	void operator()(const HTTPRequest &req);
	bool operator()(const HTTPRequest &req, const StrViewA &vpath);


	static char pathSeparator;

protected:
	std::string documentRoot;
	std::string index;


};

}
