#pragma once

#include <random>

#include "stringview.h"
#include <vector>


namespace simpleServer {



///Some constants defined for websockets
class WebSocketsConstants {
public:
	static const unsigned int opcodeContFrame = 0;
	static const unsigned int opcodeTextFrame = 1;
	static const unsigned int opcodeBinaryFrame = 2;
	static const unsigned int opcodeConnClose = 8;
	static const unsigned int opcodePing = 9;
	static const unsigned int opcodePong = 10;

	static const unsigned int closeNormal = 1000;
	static const unsigned int closeGoingAway = 1001;
	static const unsigned int closeProtocolError = 1002;
	static const unsigned int closeUnsupportedData = 1003;
	static const unsigned int closeNoStatus = 1005;
	static const unsigned int closeAbnormal = 1006;
	static const unsigned int closeInvalidPayload = 1007;
	static const unsigned int closePolicyViolation = 1008;
	static const unsigned int closeMessageTooBig = 1009;
	static const unsigned int closeMandatoryExtension = 1010;
	static const unsigned int closeInternalServerError = 1011;
	static const unsigned int closeTLSHandshake = 1015;

};

class WebSocketParser {
public:

	enum FrameType {
		///frame is not complete yet
		incomplette,
		///text frame
		text,
		///binary frame
		binary,
		///connection close frame
		connClose,
		///ping frame
		ping,
		///pong frame
		pong,
		///Object is initial state
		/**
		 * No data has been retrieved yet
		 *
		 * This frame has no data
		 */
		init
	};

	///parse received block
	/**
	 * @param data read from the stream
	 * @return unused data
	 *
	 * @note If incomplete data are received, you should
	 * call it again with additional data. Test throught function isCoplette();
	 */
	BinaryView parse(const BinaryView &data);


	///Returns true, when previous parse() finished whole frame and the frame is ready to collect
	bool isComplette() const;


	FrameType getFrameType() const;

	///Retrieve data as binary view
	BinaryView getData() const;

	///Retrieve data as text view
	StrViewA getText() const;

	///Get code (for opcodeConnClose)
	unsigned int getCode() const;

	///Resets the state
	void reset();
public:

	enum State {
		opcodeFlags,
		sizeMask,
		sizeMulti,
		masking,
		payload
	};

	State currentState = opcodeFlags;
	std::size_t stateRemain;

	std::size_t size;
	FrameType ftype = init;
	unsigned int closeCode;
	unsigned char opcode;
	unsigned char mask[4];
	unsigned char maskPos;
	bool masked;
	bool fin;

	std::vector<unsigned char> receivedData;

	void afterSize();
	void epilog();




};

class WebSocketSerializer {
public:

	WebSocketSerializer(std::default_random_engine *randomEngine)
		:randomEnginemasking(randomEnginemasking) {}

	///Create server serializer
	static WebSocketSerializer server();
	///Create client serializer
	/** Client serializer need random generator to create masked frames */
	static WebSocketSerializer client(std::default_random_engine& rnd);


	BinaryView forgeBinaryFrame(const BinaryView &data);
	BinaryView forgeTextFrame(const StrViewA &data);
	BinaryView forgePingFrame(const BinaryView &data);
	BinaryView forgePongFrame(const BinaryView &data);
	BinaryView forgeCloseFrame(unsigned int code = WebSocketsConstants::closeNormal);
protected:

	BinaryView forgeFrame(int opcode, const BinaryView &data);

	std::default_random_engine *randomEnginemasking;

	std::vector<unsigned char> frameData;


};


}
