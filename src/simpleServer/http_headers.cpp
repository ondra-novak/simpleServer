#include "http_headers.h"

namespace simpleServer {



ReceivedHeaders::HdrMap::const_iterator ReceivedHeaders::begin() const {
	return hdrMap.begin();
}

ReceivedHeaders::HdrMap::const_iterator ReceivedHeaders::end() const {
	return hdrMap.end();
}

HeaderValue ReceivedHeaders::operator [](StrViewA key) const {
	auto x = hdrMap.find(key);
	if (x == hdrMap.end()) {
		return HeaderValue();
	} else {
		return HeaderValue(x->second);
	}
}

bool ReceivedHeaders::parse(const BinaryView& data, BinaryView& putBack) {
	if (acceptLine(data, putBack)) return true;
	parseHeaders(requestHdrLineBuffer);
	return false;
}

bool ReceivedHeaders::parse(const Stream& stream) {
	BinaryView data = stream.read(false);
	BinaryView putBack;
	while (!data.empty() && parse(data,putBack)) {
		data = stream.read(false);
	}
	if (data.empty()) return false;
	stream.putBack(putBack);
	return true;
}

void ReceivedHeaders::parseAsync(const Stream& stream, const std::function<void(AsyncState)>& callback) {
	std::function<void(AsyncState)> cb(callback);
	Stream s(stream);
	stream.readAsync([=](AsyncState st, const BinaryView &data){
		if (st == asyncOK) {
			BinaryView putBack;
			if (parse(data, putBack)) {
				parseAsync(s, cb);
			} else {
				s.putBack(putBack);
				cb(asyncOK);
			}
		} else {
			cb(st);
		}
	});
}

StrViewA ReceivedHeaders::getFirstLine() const {
	return reqLine;
}

bool ReceivedHeaders::acceptLine(const BinaryView &d, BinaryView &putBack) {
	BinaryView data(d);
	while (!data.empty() && requestHdrLineBuffer.empty() && isspace(data[0])) {
		data = data.substr(1);
	}

	std::size_t searchPos = requestHdrLineBuffer.size();
	if (searchPos>=3) {
		searchPos-=3;
	} else {
		searchPos = 0;
	}


	requestHdrLineBuffer.insert(requestHdrLineBuffer.end(),data.begin(),data.end());
	StrViewA hdrline(requestHdrLineBuffer.data(), requestHdrLineBuffer.size());
	auto pos = hdrline.indexOf("\r\n\r\n",searchPos);
	if (pos != hdrline.npos) {
		BinaryView remain(hdrline.substr(pos+4));
		auto ofs = data.length - remain.length;
		putBack = data.substr(ofs);
		requestHdrLineBuffer.resize(pos+4);
		return false;
	} else {
		return true;
	}

}

static StrViewA trim(StrViewA src) {
	while (!src.empty() && isspace(src[0])) src = src.substr(1);
	while (!src.empty() && isspace(src[src.length-1])) src = src.substr(0,src.length-1);
	return src;
}

void ReceivedHeaders::clear() {
	hdrMap.clear();
	requestHdrLineBuffer.clear();
	reqLine = StrViewA();
}

void ReceivedHeaders::parseHeaders(const StrViewA &hdr) {
	hdrMap.clear();
	auto splt = hdr.split("\r\n");
	reqLine = splt();
	while (!!splt) {
		StrViewA line = splt();
		auto kvsp = line.split(":",1);
		StrViewA key = trim(kvsp());
		StrViewA value = trim(kvsp());
		hdrMap.insert(std::make_pair(key, value));
	}
}

static char *putNumber(char *x, std::size_t sz, bool first) {
	if (sz || first) {
		*(x = putNumber(x, sz/10,false)) = '0'+(sz%10);
		return x+1;
	} else {
		return x;
	}
}

SendHeaders&& SendHeaders::contentLength(std::size_t sz) {
	char buff[100];

	char *x = putNumber(buff,sz,true);
	return operator()("Content-Length", StrViewA(buff, x-buff));
}

SendHeaders&& SendHeaders::operator ()(const StrViewA key, const StrViewA value) {
	Pool::String k = pool.add(key);
	Pool::String v = pool.add(value);
	hdrMap[k] = v;
	return std::move(*this);
}

SendHeaders&& SendHeaders::contentType(StrViewA str) {
	return operator()("Content-Type", str);
}

SendHeaders&& SendHeaders::status(StrViewA str) {
	firstLine = pool.add(str);
	return std::move(*this);
}

SendHeaders&& SendHeaders::request(StrViewA str) {
	firstLine = pool.add(str);
	return std::move(*this);
}

void SendHeaders::clear() {
	pool.clear();
	hdrMap.clear();
	firstLine = Pool::String();
}

ReceivedHeaders::ReceivedHeaders(const ReceivedHeaders& other)
	:requestHdrLineBuffer(other.requestHdrLineBuffer)
{
	parseHeaders(requestHdrLineBuffer);
}


HeaderValue simpleServer::SendHeaders::operator [](StrViewA key) const {
		auto x = hdrMap.find(key);
		if (x == hdrMap.end()) {
			return HeaderValue();
		} else {
			return HeaderValue(x->second);
		}
}

}
