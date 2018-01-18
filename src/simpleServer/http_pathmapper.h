#pragma once

#include <functional>
#include "stringview.h"
#include "shared/constref.h"
#include <initializer_list>

#include "http_parser.h"

namespace simpleServer {

using ondra_shared::const_ref;

typedef std::function<bool(const HTTPRequest &req, const StrViewA &vpath)> HTTPMappedHandler;




/** a HTTP handler which maps request to other handlers (type HTTPMappedHandler)
 *
 *  The handler is just container which accepts function performing the real mapping.
 *  There can be more paths and handlers
 *
 *  HttpPathMapper allows two kinds of mapping. First is direct mapping, where the
 *  path is the directly converted to vpath (virtual path) and that path is used
 *  to call the handler.  Other mapping is the base mapping, when the
 *  path is converted to vpath by removing its base, so the handler will see
 *  the path to relative its root.
 *
 *  @tparam PathMapFunction must accept HTTPRequest and StrViewA as vpath. The
 *  mapper should not generate any output. It should instead to find
 *  mapping for the request and return an object which should implement
 *  following methods.
 *
 *  StrViewA getPath() - contains result path for the mapping
 *  HTTPMappedHandler getHandler() - contains handler which will process the request
 *  bool isBase() - is set to true,if the path is the base (for the base-mapping).
 *   If this value is false, then the path is used as direct path
 *
 *  In base mapping, the handler can reject the request by returning the false. In
 *  this case, the object tries to find another handler on the more general base
 *  path. It can search up to the root. When handler is not found, it can
 *  generate the page 404.
 *
 *  The HttpPathMapper can be chained, because it can be used both as HTTPHandler
 *  and HTTPMappedHandler. If used as the HTTPMappedHandler, it cannot generate
 *  the page 404 for failed search instead it just rejects the request.
 *
 *  The argument PathMapFunction can be refered by value and by reference as well.
 *
 */
template<typename PathMapFunction>
class HttpPathMapper {
public:

	HttpPathMapper(const_ref<PathMapFunction> mapper):mapper(mapper) {}


	///Call mapper as HTTPHandler
	void operator()(const HTTPRequest &req) const {

		StrViewA path = req.getPath();

		if (!mapPath(req, path, path)) {
			req.sendErrorPage(404);
		}

	}


	///Call mapper as HTTPMappedHandler
	bool operator()(const HTTPRequest &req, const StrViewA &vpath) const {
		return mapPath(req, vpath, vpath);
	}

protected:

	PathMapFunction mapper;

	template<typename T>
	bool mapPath(const HTTPRequest &req, const StrViewA &originPath, const T &qpath) const {

		auto hinfo = mapper(req,qpath);
		auto mappedPath = hinfo.getPath();
		auto h = hinfo.getHandler();
		bool isbase = hinfo.isBase();
		if (isbase) {
			std::size_t bpathlen = mappedPath.length;
			if (bpathlen && mappedPath[bpathlen-1] == '/') --bpathlen;
			StrViewA vpath = originPath.substr(bpathlen);

			bool res = h == nullptr?false:h(req, vpath);
			if (res == false) {

				if (mappedPath.empty()) {
					return false;
				} else {
					return mapPath(req, originPath, mappedPath.substr(0, mappedPath.length-1));
				}
			} else {
				return true;
			}
		} else if (h == nullptr){
			return false;
		} else {
			return h(req,mappedPath);
		}
	}
};

StrViewA commonPart(const StrViewA &a, const StrViewA &b);

///Static mapper
/** For fininal list of path and handlers
 *
 * Static path mapper is initialized with list of handlers. After its initialization,
 * the list cannot be changed (immutable). This gennerates slighly better performance
 * because there are no locks.
 */
class HttpStaticPathMapper {
public:

	struct MapRecord {
		StrViewA path;
		HTTPMappedHandler handler;

		const StrViewA &getPath() const {return path;}
		const HTTPMappedHandler &getHandler() const {return handler;}
		static constexpr bool isBase() {return true;}
	};


	typedef std::vector<MapRecord> PathDir;

	static MapRecord emptyResult;

	const MapRecord &operator()(const HTTPRequest &, const StrViewA &path) const;

	HttpStaticPathMapper() {}
	HttpStaticPathMapper(PathDir &&data):pathDir(std::move(data)) {
		sort();
	}
	HttpStaticPathMapper(const PathDir &data):pathDir(data) {
		sort();
	}

	template<int N>	HttpStaticPathMapper(const MapRecord (&data)[N]) {
		for (auto &&x: data) {
			pathDir.push_back(x);
		}
		sort();
	}

	HttpStaticPathMapper(const std::initializer_list<MapRecord> &data) {
		for (auto &&x: data) {
			pathDir.push_back(x);
		}
		sort();
	}


protected:

	PathDir pathDir;
	const HttpStaticPathMapper::MapRecord &find(const StrViewA &path) const;
	static bool compareMapRecord (const MapRecord &a, const MapRecord &b);

	void sort();

};

typedef HttpPathMapper<HttpStaticPathMapper> HttpStaticPathMapperHandler;
}
