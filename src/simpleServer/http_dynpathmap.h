#pragma once
#include <map>
#include <mutex>

namespace simpleServer {

class HttpDynamicPathMapper {

	typedef std::map<std::string, HTTPMappedHandler, std::greater<std::string> > HMap;
	typedef HMap::const_iterator HMapIter;

public:
	HttpDynamicPathMapper();
	virtual ~HttpDynamicPathMapper();

	class Result {
	public:
		StrViewA getBase() const;
		const HTTPMappedHandler &getHandler() const;

		Result(const std::string &base,const HTTPMappedHandler &handler)
			:base(base),handler(handler) {}

	protected:
		std::string base;
		HTTPMappedHandler handler;

	};

	Result operator()(const StrViewA &path) const;

protected:

	std::mutex lock;
	typedef std::lock_guard<std::mutex> Sync;
	HMap hmap;

};

} /* namespace simpleServer */

