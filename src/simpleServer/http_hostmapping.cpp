/*
 * http_hostmapping.cpp
 *
 *  Created on: 30. 11. 2017
 *      Author: ondra
 */


#include "http_hostmapping.h"

namespace simpleServer {


bool HostMappingHandler::compareRecord(const Record &rc1, const Record &rc2) {
	return rc1.host < rc2.host;
}

void HostMappingHandler::setMapping(StrViewA mapping) {

	clear();
	if (mapping.empty()) return;

	std::shared_ptr<char> str(new char[mapping.length], std::default_delete<char[]>());
	std::copy(mapping.begin(), mapping.end(),str.get());
	StrViewA m(str.get(), mapping.length);

	buffer = str;

	std::vector<Record> srch;

	m = m.trim(isspace);
	auto splt = m.split(",");
	while  (!!splt) {

		StrViewA item = StrViewA(splt()).trim(isspace);
		Record rc;
		if (item == "*") {
			rc.host = item;
			rc.path = rc.vpath = "/";
			srch.push_back(rc);
		} else {
			auto parts = item.split("->",1);
			StrViewA hostpath = StrViewA(parts()).trim(isspace);
			rc.vpath = StrViewA(parts()).trim(isspace);

			auto s = hostpath.indexOf("/");
			if (s == hostpath.npos) {
				rc.host = hostpath;
				rc.path = rc.vpath;
			} else {
				rc.host = hostpath.substr(0,s).trim(isspace);
				rc.path = hostpath.substr(s);
			}

			srch.push_back(rc);
		}
	}

	std::sort(srch.begin(), srch.end(), &compareRecord);


	srchDataLen = srch.size();
	srchData = std::shared_ptr<Record> (new Record[srchDataLen], std::default_delete<Record[]>());
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
//	req->log.debug("mapping host $1 $2 -> $3", req.getHost(), vpath, newvpath);
	return handler(req, newvpath);
}

StrViewA HostMappingHandler::map(StrViewA host, StrViewA path, std::string &tmpBuffer) const {

	if (srchData == nullptr) return path;

	Record v;
	v.host = host;

	StringView<Record> data(srchData.get(), srchDataLen);

	auto range = std::equal_range(data.begin(), data.end(), v, &compareRecord);

	for (auto i = range.first; i != range.second; ++i) {
		auto offset = i->path.length;
		if (i->path == path.substr(0,offset)) {

			StrViewA vpath = i->vpath;
			if (vpath.length< offset && path.substr(offset-vpath.length, vpath.length) == vpath) {
				return path.substr(offset-vpath.length);
			} else {
				StrViewA adjpath = path.substr(offset);
				if (adjpath.empty()) {
					return vpath;
				} else {
					tmpBuffer.clear();
					tmpBuffer.reserve(vpath.length+adjpath.length);
					tmpBuffer.append(vpath.data, vpath.length);
					tmpBuffer.append(adjpath.data, adjpath.length);
					return tmpBuffer;
				}
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

