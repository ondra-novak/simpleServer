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

Result HttpDynamicPathMapper::operator ()(const StrViewA& path) const {
	Sync _(lock);
	auto itr = hmap.lower_bound(path);
	if (itr == hmap.end()) {
		return Result(path, nullptr);
	} else {
		StrViewA cp = commonPart(itr->first, path);
		if (cp.length == found.path.length) return found;
		else return operator()(cp);


	}
}


StrViewA HttpDynamicPathMapper::Result::getBase() const {
	return base;
}

const HTTPMappedHandler& HttpDynamicPathMapper::Result::getHandler() const {
	return handler;
}

} /* namespace simpleServer */
