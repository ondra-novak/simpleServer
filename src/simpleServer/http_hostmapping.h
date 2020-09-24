#pragma once

#include "../../../shared/shared_object.h"
#include "stringview.h"

#include "http_parser.h"

#include "http_pathmapper.h"

namespace simpleServer {

///HostMappingHandler maps host+path to vpath
/**You can define multiple hosts and specify how various maps are mapped to vpath. You can
 * generate various hierarchy paths.
 *
 * HostMappingHandler receives a string, where each entity is separated by comma
 *
 *  host/path->path
 *
 *  For example
 *
 *  @code
 *
 *  # maps localhost/admin to /local/admin
 *  localhost/admin->/local/admin
 *  # maps www.example.com/api/service/RPC to /RPC
 *  www.example.com/api/service/RPC->/RPC
 *
 *  @endcode
 *
 *  Whole example
 *  localhost/admin->/local/admin,www.example.com/api/service/RPC->/RPC
 *
 *  It is allowed to have white characters between separators
 *  localhost/admin -> /local/admin , www.example.com/api/service/RPC -> /RPC
 *
 *  If there is * instead host, it is used as "all other hosts".
 *  If there is only * for whole item, it is interpreted as "all other hosts map directly"
 *
 *  To use this handler, chain it with path mapper, for example
 *
 *  server >> HostMappingHandler >> PathMapper
 *
 *
 *  Initialized object can be copied and shared, because it contains immutable data. Changing state
 *  of the object doesn't affect the copies.
 */

class HostMappingHandler {
public:


	void setMapping(StrViewA mapping);

	void clear();

	void operator()(const HTTPRequest &req);
	bool operator()(const HTTPRequest &req, const StrViewA &vpath);

	StrViewA map(StrViewA host, StrViewA path, std::string &tmpBuffer) const;

	HostMappingHandler & operator>>(const HTTPMappedHandler &handler);

protected:

	std::shared_ptr<char> buffer;

	struct Record {
		StrViewA host;
		StrViewA path;
		StrViewA vpath;
	};


	std::shared_ptr<Record> srchData;
	std::size_t srchDataLen;

	HTTPMappedHandler handler;

	static bool compareRecord(const Record &rc1, const Record &rc2);


};

class AutoHostMappingHandler {
public:

	AutoHostMappingHandler();

	void operator()(const HTTPRequest &req);
	bool operator()(const HTTPRequest &req, const StrViewA &vpath);

	AutoHostMappingHandler & operator>>(const HTTPMappedHandler &handler);



protected:

	HTTPMappedHandler handler;

	struct MapItem {
		std::string host;
		std::string path;
	};

	using MapData = std::vector<MapItem>;
	using PMapData = ondra_shared::SharedObject<MapData>;

	PMapData mapData;

	struct CmpMapItems {
		bool operator()(const MapItem &a, const StrViewA &b) const;
		bool operator()(const StrViewA &a, const MapItem &b) const;
		bool operator()(const MapItem &a, const MapItem &b) const;
	};

};



}
