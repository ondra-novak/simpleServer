#pragma once

#include "abstractStream.h"

namespace simpleServer {

	class LimitedStream: public AbstractStream {
	public:

		LimitedStream(Stream source, std::size_t readLimit, std::size_t writeLimit, unsigned char fillChar)
			:source(source), readLimit(readLimit), writeLimit(writeLimit),fillChar(fillChar) {}

		~LimitedStream() noexcept {
			try {
				padding();
			} catch (...) {

			}

		}


		virtual int setIOTimeout(int timeoutms) override {
			return source.setIOTimeout(timeoutms);
		}
protected:
		//template<typename T> friend class RefCntPtr;



		virtual BinaryView implRead(bool nonblock) override {
			if (readLimit == 0) return eofConst;
			BinaryView b = source.read(nonblock);
			BinaryView r = b.substr(0,readLimit);
			source.putBack(b.substr(r.length));
			readLimit -= r.length;
			return r;
		}


		virtual BinaryView implRead(MutableBinaryView buffer, bool nonblock) override {
			if (readLimit == 0) return eofConst;
			MutableBinaryView x = buffer.substr(0,readLimit);
			BinaryView r = source.read(x, nonblock);
			readLimit-=r.length;
			return r;
		}
		virtual BinaryView implWrite(BinaryView buffer, bool nonblock) override {
			if (writeLimit == 0) return BinaryView(0,0);
			BinaryView b = buffer.substr(0,writeLimit);
			BinaryView r = source->getDirectWrite().write(b,nonblock);
			writeLimit-=b.length - r.length;
			return r;
		}
		virtual bool implWrite(WrBuffer &curBuffer, bool nonblock) override {
			if (writeLimit < curBuffer.wrpos)
				curBuffer.wrpos = writeLimit;
			writeLimit -= curBuffer.wrpos;
			bool r = source->getDirectWrite().write(curBuffer,nonblock);
			writeLimit += curBuffer.wrpos;
			return r;
		}
		virtual void implReadAsync(const Callback &cb)  override {
			if (readLimit == 0) {
				cb(asyncEOF, eofConst);
				return;
			}
			RefCntPtr<LimitedStream> me(this);
			Callback ccb(cb);
			source.readAsync([=](AsyncState st, const BinaryView &b) {
				BinaryView x = b.substr(0,me->readLimit);
				source.putBack(b.substr(x.length));
				me->readLimit-=x.length;
				ccb(st, x);
			});
		}
		virtual void implReadAsync(const MutableBinaryView &buffer, const Callback &cb)  override {
			if (readLimit == 0) {
				cb(asyncEOF, eofConst);
				return;
			}
			MutableBinaryView b = buffer.substr(0,readLimit);
			RefCntPtr<LimitedStream> me(this);
			Callback ccb(cb);
			source.readAsync(b,[=](AsyncState st, const BinaryView &b) {
				me->readLimit-=b.length;
				ccb(st,b);
			});
		}
		virtual void implWriteAsync(const BinaryView &data, const Callback &cb)  override {
			if (writeLimit == 0) return;

			BinaryView b = data.substr(0,writeLimit);
			RefCntPtr<LimitedStream> me(this);
			Callback ccb(cb);
			writeLimit-= b.length;

			source->getDirectWrite().writeAsync(b, [=](AsyncState st, const BinaryView &data){
				me->writeLimit+=data.length;
				ccb(st, data);
			});
		}
		virtual bool implWaitForRead(int timeoutms)  override {
			if (readLimit == 0) return true;
			else return source.waitForInput(timeoutms);
		}
		virtual bool implWaitForWrite(int timeoutms)  override {
			if (writeLimit == 0) return true;
			else return source.waitForOutput(timeoutms);

		}
		virtual void implCloseInput()  override {
			source.putBackEof();
		}
		virtual void implCloseOutput()  override {
			padding();
		}
		virtual bool implFlush()  override {
			return source->getDirectWrite().flush();
		}
		virtual bool canRunAsync() const override {
			return source->canRunAsync();
		}



	protected:
		Stream source;
		std::size_t readLimit;
		std::size_t writeLimit;
		unsigned char fillChar;

		void padding() {
			while (writeLimit) {
				source(fillChar);
				writeLimit--;
			}
		}

	};

}
