#pragma once
#include "shared/stringview.h"
#include "http_parser.h"

using ondra_shared::StrViewA;

namespace simpleServer {

///Maps path to files on filesystem
/**
 * It allows to customize mapping. By default it maps path to files and selects proper mime type
 */
class HttpFileMapper {
public:

	HttpFileMapper(std::string &&documentRoot, std::string &&index, std::size_t cache_secs = 0);

	virtual bool mapFile(const HTTPRequest &req, StrViewA fullPathname);
	virtual StrViewA mapMime(StrViewA fullPathname);


	void operator()(const HTTPRequest &req);
	bool operator()(const HTTPRequest &req, const StrViewA &vpath);

	virtual ~HttpFileMapper() {}

	static char pathSeparator;

protected:
	std::string documentRoot;
	std::string index;
	std::size_t cache_secs;


};

}
