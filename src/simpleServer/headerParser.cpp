/*
 * headerParser.cpp
 *
 *  Created on: 11. 10. 2017
 *      Author: ondra
 */

#include "headerParser.h"

#include <sstream>

namespace simpleServer {


void KeyValueHeaders::parse(StrViewA hdrText) {
	clear();
	txtBuffer.reserve(hdrText.length);
	for (auto &&x: hdrText) txtBuffer.push_back(x);
	parse_internal();
}

void KeyValueHeaders::parse(TextBuffer&& buffer) {
	clear();
	txtBuffer = std::move(buffer);
	parse_internal();
}

HeaderValue KeyValueHeaders::operator [](const StrViewA& field) {

	auto itr = hdrMap.find(StrItem(field));
	if (itr == hdrMap.end()) return HeaderValue();
	else return HeaderValue(itr->second);

}

void KeyValueHeaders::parse_internal() {
}

KeyValueHeaders::StrItem KeyValueHeaders::addStrItem(StrViewA txt) {

	std::size_t curEnd = txtBuffer.size();
	for (auto && c : txt) {
		txtBuffer.push_back(c);
	}
	txtBuffer.push_back((char)0);
	return StrItem(txtBuffer, curEnd, txt.length);

}


void KeyValueHeaders::clear() {
	hdrMap.clear();
	txtBuffer.clear();

}

void KeyValueHeaders::add(const StrViewA key, const StrViewA value) {
	StrItem strKey = addStrItem(key);
	StrItem strValue = addStrItem(value);
	hdrMap[strKey] = strValue;
}

std::string KeyValueHeaders::serialize() {
	std::ostringstream buffer;
	for (auto && x : hdrMap) {

		buffer << StrViewA(x.first) << ": " << StrViewA(x.second) << "\r\n";
	}
	return buffer.str();

}

KeyValueHeaders::StrItem::StrItem(const TextBuffer& data,
		std::size_t offset, std::size_t length)

:length(length), data(&data), offset(offset)
{

}

KeyValueHeaders::StrItem::StrItem(StrViewA data)
:length(data.length), data(nullptr), ptr(data.data)

{
}

KeyValueHeaders::StrItem::StrItem():length(0),data(0),ptr(0) {
}


KeyValueHeaders::StrItem::operator StrViewA() const {
if (data) {
	return StrViewA(data->data()+offset, length);
}
}

bool KeyValueHeaders::StrItemOrder::operator ()(const StrItem& a,const StrItem& b) const {
	StrViewA sa(a);
	StrViewA sb(b);
	return sa < sb;
}


} /* namespace simpleServer */

