/*
 * asyncProvider.cpp
 *
 *  Created on: 26. 6. 2018
 *      Author: ondra
 */

#include "shared/worker.h"
#include "asyncProvider.h"

namespace simpleServer {

AsyncProvider::operator Worker() const {

	class Wrk: public ondra_shared::AbstractWorker {
	public:
		Wrk(RefCntPtr<AbstractAsyncProvider> async):async(async) {}

		virtual void dispatch(Msg &&msg){
			async->runAsync(std::move(msg));
		}

		virtual void run() noexcept	{}

		virtual void flush() noexcept {}

		virtual void clear() noexcept  {}

		RefCntPtr<AbstractAsyncProvider> async;
	};

	return Worker(new Wrk(*this));

}


}

