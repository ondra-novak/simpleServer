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


	void operator()(HTTPRequest req) {

		for (auto &&pathItem : mapper(req.getPath())) {

			HTTPMappedHandler h(pathItem);
			StrViewA bpath(pathItem);
			StrViewA vpath = req.getPath().substr(bpath.length);

			if (h(req, vpath)) {
				return;
			}
		}
	}

protected:

	PathMapFunction mapper;

};


class HttpStaticPathMapper {
public:

	struct MapRecord {
		StrViewA path;
		HTTPMappedHandler handler;


		operator HTTPMappedHandler() const {return handler;}
		operator StrViewA() const {return path;}
	};

	typedef std::vector<MapRecord> PathDir;





	class Iterator {
	public:
		Iterator(const HttpStaticPathMapper &owner, StrViewA lookPath,PathDir::const_iterator cur )
			:	owner(owner)
			,	lookPath(lookPath)
			,	cur(cur)
		{
		}


		bool operator==(const Iterator &other) {return cur == other.cur;}
		bool operator!=(const Iterator &other) {return cur != other.cur;}
		Iterator &operator++() {
			if (lookPath.empty()) cur = owner.getEnd();
			else {
				lookPath = lookPath.substr(0,lookPath.length-1);
				cur = owner.find(lookPath);
			}
			return *this;
		}
		Iterator operator++(int) {
			Iterator save = *this;
			operator++();
			return save;
		}

		const MapRecord &operator *() const {
			return *cur;
		}
		const MapRecord *operator ->() const {
			return &(*cur);
		}

	protected:
		const HttpStaticPathMapper &owner;
		StrViewA lookPath;
		PathDir::const_iterator cur;
	};

	class Collection {
	public:
		Collection (const HttpStaticPathMapper &owner, StrViewA lookPath)
			:	owner(owner)
			,	lookPath(lookPath)
		{
		}

		Iterator begin() const {return Iterator(owner, lookPath, owner.find(lookPath));}
		Iterator end() const {return Iterator(owner, lookPath, owner.getEnd());}
	protected:
		const HttpStaticPathMapper &owner;
		StrViewA lookPath;
	};

	HttpStaticPathMapper();
	template<int N>
	HttpStaticPathMapper(const MapRecord (&mappings)[N]) {
		pathDir.reserve(N);
		for (auto &&x:mappings) pathDir.push_back(x);
		sort();
	}

	Collection operator()(StrViewA path) const;

protected:
	PathDir pathDir;
	PathDir::const_iterator find(StrViewA path) const;
	PathDir::const_iterator getEnd() const;

	static bool compareMapRecord (const MapRecord &a, const MapRecord &b);
	void sort();

};

typedef HttpPathMapper<HttpStaticPathMapper> HttpStaticPathMapperHandler;

}
