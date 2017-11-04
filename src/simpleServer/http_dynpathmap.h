#pragma once
#include <map>
#include <mutex>
#include <string>

#include "http_pathmapper.h"

namespace simpleServer {

class HttpDynamicPathMapper {

	typedef std::map<std::string, HTTPMappedHandler, std::greater<std::string> > HMap;
	typedef HMap::const_iterator HMapIter;

public:
	HttpDynamicPathMapper();
	virtual ~HttpDynamicPathMapper();

	class Result {
	public:
		StrViewA getPath() const;
		const HTTPMappedHandler &getHandler() const;
		static constexpr bool isBase() {return true;}

		Result(const std::string &base,const HTTPMappedHandler &handler)
			:base(base),handler(handler) {}

	protected:
		std::string base;
		HTTPMappedHandler handler;

	};

	Result operator()(const HTTPRequest &, const StrViewA &path) const;
	void add(std::string &&str, HTTPMappedHandler &&handler);
	void add(const std::string &str, const HTTPMappedHandler &handler);
	void remove(const std::string &str);
	void remove(std::string &&str);

protected:

	Result findlk(const StrViewA &path) const;

	mutable std::mutex lock;
	typedef std::lock_guard<std::mutex> Sync;
	HMap hmap;

};

} /* namespace simpleServer */

