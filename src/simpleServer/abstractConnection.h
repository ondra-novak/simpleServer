#pragma once

#include "connection.h"

namespace simpleServer {





template<std::size_t bufferSize >
class AbstractConnection: public IConnection {
public:

	AbstractConnection()
		:rdbuff_used(0)
		,rdbuff_pos(0)
		,wrbuff_pos(0)
		,eof(false)
		,eofReported(false) {}

	virtual BinaryView readData(unsigned int prevRead) override {
		BinaryView buff = getReadBuffer();
		if (prevRead == buff.length) {
			rdbuff_used = rdbuff_pos = 0;
		} else if (prevRead > buff.length) {
			rdbuff_used = rdbuff_pos = 0;
			prevRead = 0;
		} else{
			rdbuff_pos += prevRead;
		}
		buff = getReadBuffer();
		if (prevRead) {
			if (buff.length == 0 && !eof) {
				int res = recvData(rdbuff,bufferSize,true);
				if (res == 0) {
					eof = true;
				} else if (res > 0) {
					rdbuff_used = res;
					rdbuff_pos = 0;
					buff = getReadBuffer();
				}
			}
			return buff;
		} else {
			if (buff.length == 0) {
				if (!eof) {
					flush();
					int res = recvData(rdbuff,bufferSize,false);
					if (res == 0) {
						eof = true;
						eofReported = true;
					} else if (res > 0) {
						rdbuff_used = res;
						rdbuff_pos = 0;
						buff = getReadBuffer();
					}
				} else {
					if (eofReported)
						throw EndOfStreamException();
					eofReported = true;
				}
			}
			return buff;
		}
	}


	virtual void writeData(const BinaryView &data) override {
		if (data.length) {
			if (wrbuff_pos == 0 && data.length > bufferSize) {
				sendAll(data);
			} else {
				std::size_t remain = bufferSize - wrbuff_pos;
				std::size_t transmit = std::min(remain,data.length);
				std::memcpy(wrbuff+wrbuff_pos,data.data,transmit);
				wrbuff_pos += transmit;
				if (wrbuff_pos == bufferSize) {
					flush();
				}
				BinaryView newdata = data.substr(transmit);
				if (newdata.length) writeData(newdata);
			}
		} else {
			closeOutput();
		}

	}
	virtual void flush() override {
		if (wrbuff_pos) {
			BinaryView data(wrbuff, wrbuff_pos);
			wrbuff_pos = 0;
			sendAll(data);
		}
	}
protected:
	///must be implemented
	virtual void sendAll(BinaryView data) = 0;
	///must be implemented
	virtual int recvData(unsigned char *buffer, std::size_t size, bool nonblock) = 0;
	///must be implemented
	virtual void closeOutput() = 0;

	unsigned int rdbuff_used;
	unsigned int rdbuff_pos;
	unsigned int wrbuff_pos;
	bool eof;
	bool eofReported;

	unsigned char rdbuff[bufferSize];
	unsigned char wrbuff[bufferSize];

	BinaryView getReadBuffer() const {
		return BinaryView(rdbuff+rdbuff_pos, rdbuff_used - rdbuff_pos);
	}

};


}
