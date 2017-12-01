/*
 * http_hostmapping.cpp
 *
 *  Created on: 30. 11. 2017
 *      Author: ondra
 */


#include "http_hostmapping.h"

namespace simpleServer {

void HostMappingHandler::setMapping(StrViewA mapping) {

	clear();
	if (mapping.empty()) return;

	std::shared_ptr<char> str(new char[mapping.length], std::default_delete<char[]>());
	std::copy(mapping.begin(), mapping.end(),str.get());
	StrViewA m(str.get(), mapping.length);

	buffer = str;

	std::vector<PathMap> srch;

	m = m.trim(isspace);
	auto splt = m.split(",");
	while  (splt) {

		StrViewA item = StrViewA(splt()).trim(isspace);
		if (item == "*") {
			srch.push_back(PathMap(HostAndPath(item,"/"),"/"));
		} else {
			auto parts = item.split("->");
			StrViewA hostpath = StrViewA(parts()).trim(isspace);
			StrViewA vpath = StrViewA(parts).trim(isspace);
			StrViewA host;
			StrViewA path;

			auto s = hostpath.indexOf("/");
			if (s == hostpath.npos) {
				host = hostpath;
				path = "/";
			} else {
				host = hostpath.substr(0,s).trim(isspace);
				path = hostpath.substr(s);
			}

			srch.push_back(PathMap(HostAndPath(host,path), vpath));
		}
	}

	std::sort(srch.begin(), srch.end(), std::less<PathMap>());


	srchDataLen = srch.size();
	srchData = std::shared_ptr<PathMap> (new PathMap[srchDataLen], std::default_delete<PathMap[]>());
	std::copy(srch.begin(),srch.end(), srchData.get());


}

void HostMappingHandler::clear() {
	srchData = nullptr;
	buffer = nullptr;
}

void HostMappingHandler::operator ()(const HTTPRequest& req) {
	if (!this->operator ()(req,req.getPath())) {
		req.sendErrorPage(404);
	}
}

bool HostMappingHandler::operator ()(const HTTPRequest& req, const StrViewA& vpath) {

	std::string tmp;
	StrViewA newvpath = map(req.getHost(), vpath, tmp);

	if (newvpath.empty()) {
		newvpath = map("*", vpath, tmp);
	}
	if (newvpath.empty()) {
		return false;
	}
	if (handler == nullptr) return false;
	return handler(req, newvpath);
}

StrViewA HostMappingHandler::map(StrViewA host, StrViewA path, std::string &tmpBuffer) const {

	if (srchData == nullptr) return path;

	PathMap l;
	l.first.first = host;
	PathMap u(l);
	u.first.second = "\xFF";

	StringView<PathMap> data(srchData.get(), srchDataLen);

	auto b = std::lower_bound(data.begin(), data.end(), l, std::less<PathMap>());
	auto e = std::upper_bound(data.begin(), data.end(), u, std::less<PathMap>());

	for (auto i = b; i != e; ++i) {
		auto offset = i->first.second.length;
		if (i->first.second == path.substr(0,offset)) {

			StrViewA vpath = i->second;
			if (vpath.length< offset && path.substr(offset-vpath.length, vpath.length) == vpath) {
				return path.substr(offset-vpath.length);
			} else {
				tmpBuffer.clear();
				tmpBuffer.reserve(vpath.length+path.length - offset);
				tmpBuffer.append(vpath.data, vpath.length);
				tmpBuffer.append(path.data+offset, path.length-offset);
				return tmpBuffer;
			}
		}
	}

	return StrViewA();

}

HostMappingHandler &HostMappingHandler::operator >>(const HTTPMappedHandler& handler) {
	this->handler = handler;
	return *this;
}

}

