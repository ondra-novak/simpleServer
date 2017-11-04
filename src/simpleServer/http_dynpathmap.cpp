/*
 * http_dynpathmap.cpp
 *
 *  Created on: 3. 11. 2017
 *      Author: ondra
 */

#include "http_dynpathmap.h"

namespace simpleServer {

HttpDynamicPathMapper::HttpDynamicPathMapper() {
	// TODO Auto-generated constructor stub

}

HttpDynamicPathMapper::~HttpDynamicPathMapper() {
	// TODO Auto-generated destructor stub
}

HttpDynamicPathMapper::Result HttpDynamicPathMapper::operator ()(const HTTPRequest &, const StrViewA& path) const {
	Sync _(lock);
	return findlk(path);
}
HttpDynamicPathMapper::Result HttpDynamicPathMapper::findlk(const StrViewA& path) const {
	auto itr = hmap.lower_bound(path);
	if (itr == hmap.end()) {
		return Result(path, nullptr);
	} else {
		StrViewA cp = commonPart(itr->first, path);
		if (cp.length == itr->first.length()) return Result(itr->first, itr->second);
		else return findlk(cp);
	}
}


StrViewA HttpDynamicPathMapper::Result::getPath() const {
	return base;
}

const HTTPMappedHandler& HttpDynamicPathMapper::Result::getHandler() const {
	return handler;
}

void HttpDynamicPathMapper::add(std::string&& s,HTTPMappedHandler&& handler) {
	Sync _(lock);
	hmap[std::move(s)] = std::move(handler);
}

void HttpDynamicPathMapper::add(const std::string &str, const HTTPMappedHandler &handler) {
	Sync _(lock);
	hmap[str] = handler;
}

void HttpDynamicPathMapper::remove(const std::string& str) {
	Sync _(lock);
	hmap.erase(str);
}

void HttpDynamicPathMapper::remove(std::string&& str) {
	Sync _(lock);
	hmap.erase(str);
}

} /* namespace simpleServer */
