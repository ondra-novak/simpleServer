#include <thread>
#include "async.h"

namespace simpleServer {


AsyncControl AsyncControl::start() {


	AsyncControl c = create();
	std::thread thr([c] {c.run();});
	thr.detach();

}

AsyncControl AsyncControl::create() {


}

AsyncControl AsyncControl::getSingleton() {

	static AsyncControl singleton = AsyncControl::start();
	return singleton;

}

}
