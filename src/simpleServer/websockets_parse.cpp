#include "websockets_parser.h"
#include "websockets_stream.h"

namespace simpleServer {




BinaryView WebSocketParser::parse(const BinaryView& data) {
	if (data.empty()) return data;
	unsigned char lopc;
	std::size_t p = 0;
	do {
		unsigned char b = data[p];
		switch (currentState) {
		case opcodeFlags:
			fin = (b & 0x80) != 0;
			lopc = (b & 0xF);
			currentState = sizeMask;
			if (lopc != WebSocketsConstants::opcodeContFrame) {
				receivedData.clear();
				opcode = lopc;
			}

			ftype = incomplette;

			mask[0] = mask[1] = mask[2] = mask[3] = 0;

			break;

		case sizeMask:
			masked = (b & 0x80) != 0;
			size = b & 0x7f;
			if (size == 126) {
				size = 0;
				currentState = sizeMulti;
				stateRemain = 2;
			} else if (size == 127) {
				size = 0;
				currentState = sizeMulti;
				stateRemain = 8;
			} else {
				afterSize();
			}
			break;
		case sizeMulti:
			size = (size << 8)+b;
			if (--stateRemain == 0) {
				afterSize();
			}
			break;
		case masking:
			mask[4-stateRemain] = b;
			if (--stateRemain == 0) {
				masked = false;
				afterSize();
			}
			break;
		case payload:
			receivedData.push_back(b ^ mask[maskPos]);
			maskPos = (maskPos + 1) & 0x3;
			if (--stateRemain == 0) {
				epilog();
			}
		}
		++p;
	} while (p < data.length && ftype == incomplette);
	return data.substr(p);
}

void WebSocketParser::afterSize() {
	if (masked) {
		currentState = masking;
		stateRemain = 4;
	}
	if (size == 0) {
		epilog();
	} else {
		receivedData.reserve(receivedData.size()+ size);
		currentState = payload;
		stateRemain = size;
	}
}

void WebSocketParser::reset() {
	currentState = opcodeFlags;
	ftype = init;
}

void WebSocketParser::epilog(){
	currentState = opcodeFlags;
	if (fin) {
		switch (opcode) {
		case WebSocketsConstants::opcodeBinaryFrame: ftype = binary;break;
		case WebSocketsConstants::opcodeTextFrame: ftype = text;break;
		case WebSocketsConstants::opcodePing: ftype = ping;break;
		case WebSocketsConstants::opcodePong: ftype = pong;break;
		case WebSocketsConstants::opcodeConnClose: ftype = connClose;
			if (receivedData.size() < 2) closeCode = 0;
			else closeCode = (receivedData[0] << 8) + receivedData[1];
			break;
		default: return;
		}
	}
}


bool WebSocketParser::isComplette() const {
	return ftype != incomplette;
}

WebSocketParser::FrameType WebSocketParser::getFrameType() const {
	return ftype;
}

BinaryView WebSocketParser::getData() const {
	return BinaryView(receivedData);
}

StrViewA WebSocketParser::getText() const {
	return StrViewA(getData());
}

unsigned int WebSocketParser::getCode() const {
	return closeCode;
}

WebSocketSerializer WebSocketSerializer::server() {
	return WebSocketSerializer(nullptr);
}

WebSocketSerializer WebSocketSerializer::client(std::default_random_engine& rnd) {
	return WebSocketSerializer(&rnd);
}

BinaryView WebSocketSerializer::forgeBinaryFrame(const BinaryView& data) {
	return forgeFrame(WebSocketsConstants::opcodeBinaryFrame, data);
}

BinaryView WebSocketSerializer::forgeTextFrame(const StrViewA& data) {
	return forgeFrame(WebSocketsConstants::opcodeTextFrame, BinaryView(data));
}

BinaryView WebSocketSerializer::forgePingFrame(const BinaryView& data) {
	return forgeFrame(WebSocketsConstants::opcodePing, data);
}

BinaryView WebSocketSerializer::forgePongFrame(const BinaryView& data) {
	return forgeFrame(WebSocketsConstants::opcodePong, data);
}

BinaryView WebSocketSerializer::forgeCloseFrame(unsigned int code) {
	unsigned char bc[2];
	bc[0] = code >> 8;
	bc[1] = code & 0xFF;
	return forgeFrame(WebSocketsConstants::opcodeConnClose, BinaryView(bc,2));
}


BinaryView WebSocketSerializer::forgeFrame(int opcode, const BinaryView& data) {
	frameData.clear();
	frameData.push_back(((unsigned char)opcode & 0xF) | 0x80);
	unsigned char szcode;
	unsigned char szbytes;
	if (data.length<126) {szcode = (unsigned char)data.length;szbytes = 0;}
	else if (data.length<65536) {szcode = 126;szbytes = 2;}
	else {szcode = 127;szbytes = 8;}
	if (randomEnginemasking) szcode |=0x80;
	frameData.push_back(szcode);
	for (unsigned char i = szbytes; i > 0; ) {
		--i;
		frameData.push_back((data.length>>(i*8)) & 0xFF);
	}
	unsigned char masking[4];
	if (randomEnginemasking) {
		for (unsigned char i = 0; i < 4; ++i) {
			masking[i] = (unsigned char)((*randomEnginemasking)() & 0xFF);
			frameData.push_back(masking[i]);
		}
	} else {
		for (unsigned char i = 0; i < 4; ++i) masking[i] = 0;
	}

	for (std::size_t i = 0; i < data.length; ++i) {
		frameData.push_back(masking[i & 0x3] ^ data[i]);
	}


	return BinaryView(frameData.data(), frameData.size());
}

}
