#pragma once

#include "abstractStream.h"

namespace simpleServer {


class IStreamFactory {
public:


	///creates stream
	/**
	 * @return newly created connected stream. Function may block until new stream is connected. Function
	 * can also return nullptr when there is no more streams available, or process has been stopped
	 */
	virtual Stream create() = 0;

	typedef std::function<void(AsyncState, Stream)> Callback;


	///create stream asynchronously
	/**
	 * @param provuider pointer to async. provider
	 * @param cb callback which is called once the stream is created
	 */
	virtual void createAsync(AsyncProviderImpl *provider, const Callback &cb) = 0;

	///Asynchronously stops process of creating of the new stream
	/**
	 * If the stream factory is server, this function closes the opened port and exits the function create()
	 * if it blocking the thread. Anytime later, the function create() returns nullptr. There is no way to
	 * restart the stream factory. You need to recreate it
	 */
	virtual void stop() = 0;




	virtual ~IStreamFactory() {}
};


class AbstractStreamFactory: public IStreamFactory, public RefCntObj {
public:

	class Iterator {
		public:

			Iterator(RefCntPtr<AbstractStreamFactory> owner, Stream first)
				:owner(owner),conn(first) {


			}
			Stream operator *() {return conn;}
			Iterator &operator++() {conn = owner->create(); return *this;}
			Iterator operator++(int) {Iterator cp = *this; conn = owner->create(); return cp;}
			bool operator==(const Iterator &other) const {return conn == other.conn;}
			bool operator!=(const Iterator &other) const {return conn != other.conn;}

		protected:
			RefCntPtr<AbstractStreamFactory> owner;
			Stream conn;
		};

		Iterator begin() {return Iterator(this, create());}
		Iterator end() {return Iterator(this, nullptr);}


};


class StreamFactory: public RefCntPtr<AbstractStreamFactory> {
public:

	using RefCntPtr<AbstractStreamFactory>::RefCntPtr;


	Stream operator()() const {
		return ptr->create();
	}

	typedef AbstractStreamFactory::Iterator Iterator;

	Iterator begin() const {
		return ptr->begin();
	}
	Iterator end() const {
		return ptr->end();
	}

	void stop() {
		return ptr->stop();
	}


	void operator()(AsyncProviderImpl *provider, const IStreamFactory::Callback &cb) const {
		ptr->createAsync(provider, cb);
	}

};


}
