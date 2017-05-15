#include <thread>
#include "async.h"

#include "linux/epollAsync.h"
namespace simpleServer {


AsyncControl AsyncControl::start() {


	AsyncControl c = create();
	std::thread thr([c] {c.run();});
	thr.detach();

}

AsyncControl AsyncControl::start( CallbackExecutor executor) {


	AsyncControl c = create();
	std::thread thr([c,executor] {c.run(executor);});
	thr.detach();

}


AsyncControl AsyncControl::getSingleton() {

	static AsyncControl singleton = AsyncControl::start();
	return singleton;

}

}
