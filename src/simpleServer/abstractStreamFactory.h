#pragma once

namespace simpleServer {


class IStreamFactory {
public:


	///creates stream
	virtual Stream create() = 0;

	virtual ~IStreamFactory() {}
};


class AbstractStreamFactory: public IStreamFactory, public RefCntObj {
public:

	class Iterator {
		public:

			Iterator(RefCntPtr<AbstractStreamFactory> owner, Stream first)
				:owner(owner),first(first) {


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

};


}
