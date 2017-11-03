#include "http_pathmapper.h"

namespace simpleServer {


HttpStaticPathMapper::HttpStaticPathMapper() {
}

bool HttpStaticPathMapper::compareMapRecord (const MapRecord &a,const MapRecord &b) {
	return a.path > b.path;
}

HttpStaticPathMapper::HttpStaticPathMapper( std::initializer_list<MapRecord> mappings)
	:pathDir(mappings)
{
	std::sort(pathDir.begin(),pathDir.end(),&compareMapRecord);
}

static StrViewA getCommonPart(const StrViewA &a, const StrViewA &b) {
	std::size_t maxCommon = std::min(a.length, b.length);
	for (std::size_t i = 0; i < maxCommon; ++i) {
		if (a[i] != b[i]) return a.substr(0,i);
	}
	return a.substr(0,maxCommon);
}

HttpStaticPathMapper::PathDir::const_iterator HttpStaticPathMapper::find(StrViewA path) const {
	MapRecord dummy;
	dummy.path = path;
	auto itr = std::lower_bound(pathDir.begin(), pathDir.end(),dummy,&compareMapRecord);
	if (itr == pathDir.end()) {return itr;}

	const MapRecord &found = *itr;
	StrViewA commonPart = getCommonPart(dummy.path, found.path);
	if (commonPart.length == found.path.length) return itr;
	else return find(commonPart);
}

HttpStaticPathMapper::Collection HttpStaticPathMapper::operator ()(StrViewA path) const {
	return Collection(*this,path);
}

HttpStaticPathMapper::PathDir::const_iterator HttpStaticPathMapper::getEnd() const {
	return pathDir.end();
}



}
