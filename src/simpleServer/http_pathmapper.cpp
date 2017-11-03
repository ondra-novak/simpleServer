#include "http_pathmapper.h"

namespace simpleServer {


HttpStaticPathMapper::MapRecord HttpStaticPathMapper::emptyResult;


const HttpStaticPathMapper::MapRecord &HttpStaticPathMapper::operator()(const StrViewA &path) {

	MapRecord dummy;
	dummy.path = path;
	auto itr = std::lower_bound(pathDir.begin(), pathDir.end(),dummy,&compareMapRecord);
	if (itr == pathDir.end()) {return emptyResult;}

	const MapRecord &found = *itr;
	StrViewA cp = commonPart(dummy.path, found.path);
	if (cp.length == found.path.length) return found;
	else return operator()(cp);
}



bool HttpStaticPathMapper::compareMapRecord (const MapRecord &a,const MapRecord &b) {
	return a.path > b.path;
}

void HttpStaticPathMapper::sort()
{
	std::sort(pathDir.begin(),pathDir.end(),&compareMapRecord);
}

StrViewA commonPart(const StrViewA &a, const StrViewA &b) {
	std::size_t maxCommon = std::min(a.length, b.length);
	for (std::size_t i = 0; i < maxCommon; ++i) {
		if (a[i] != b[i]) return a.substr(0,i);
	}
	return a.substr(0,maxCommon);
}




}
