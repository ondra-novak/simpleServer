#pragma once

#include <functional>
#include "stringview.h"
#include "shared/constref.h"
#include <initializer_list>

#include "http_parser.h"

namespace simpleServer {

using ondra_shared::const_ref;

typedef std::function<bool(HTTPRequest req, StrViewA vpath)> HTTPMappedHandler;




/**
 * PathMapFunction is function which returns a collection which
 * supports begin() and end() functions. Using ranged-for, the mapper enumerates all of the
 * available paths and calls handler which accepts the path
 *
 * Each item in the collection must be convertible to HTTPMappedHandler to receive handler, and to StrViewA
 * to receive vpath
 *
 */
template<typename PathMapFunction>
class HttpPathMapper {
public:

	HttpPathMapper(const_ref<PathMapFunction> mapper):mapper(mapper) {}


	void operator()(const HTTPRequest &req) {

		StrViewA path = req.getPath();

		mapPath(req, path, path);


	}


protected:

	PathMapFunction mapper;

	template<typename T>
	void mapPath(const HTTPRequest &req, const StrViewA &originPath, const T &qpath) {

		auto hinfo = mapper(qpath);
		auto basePath = hinfo.getBase();
		auto h = hinfo.getHandler();

		bool res = h == nullptr?false:h(req, originPath.substr(basePath.length));
		if (res == false) {

			if (basePath.empty()) {
				req.sendErrorPage(404);
			} else {
				mapPath(req, originPath, basePath.substr(0, basePath.length-1));
			}
		}
	}
};

StrViewA commonPart(const StrViewA &a, const StrViewA &b);


class HttpStaticPathMapper {
public:

	struct MapRecord {
		StrViewA path;
		HTTPMappedHandler handler;

		const StrViewA &getBase() const {return path;}
		const HTTPMappedHandler &getHandler() const {return handler;}
	};


	typedef std::vector<MapRecord> PathDir;

	static MapRecord emptyResult;

	const MapRecord &operator()(const StrViewA &path);

	HttpStaticPathMapper() {}
	HttpStaticPathMapper(PathDir &&data):pathDir(std::move(data)) {
		sort();
	}
	HttpStaticPathMapper(const PathDir &data):pathDir(data) {
		sort();
	}

	template<int N>	HttpStaticPathMapper(const MapRecord (&data)[N]) {
		for (auto &&x: data) pathDir.push_back(x); sort();
	}

	HttpStaticPathMapper(const std::initializer_list<MapRecord> &data) {
		for (auto &&x: data) pathDir.push_back(x); sort();
	}


protected:

	PathDir pathDir;
	static bool compareMapRecord (const MapRecord &a, const MapRecord &b);
	void sort();

};

typedef HttpPathMapper<HttpStaticPathMapper> HttpStaticPathMapperHandler;
}
