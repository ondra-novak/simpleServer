/*
 * realpath.cpp
 *
 *  Created on: Jul 19, 2018
 *      Author: ondra
 */


#include "../realpath.h"
#include "../exceptions.h"

std::string realpath(const std::string &path) {

	using namespace ondra_shared;

	CPtr<char> p(realpath(path.c_str(), nullptr));
	if (!p) {
		if (path == ".") throw simpleServer::SystemException(errno);

		auto p = path.rfind('/');
		if (p == path.npos) {
			return realpath(".")+"/"+path;
		} else {
			return realpath(path.substr(0,p))+"/"+path.substr(p+1);
		}
	}
	return p.get();




}


