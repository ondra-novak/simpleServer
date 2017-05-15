#include <thread>
#include "async.h"

#include "linux/epollAsync.h"
namespace simpleServer {


AsyncDispatcher AsyncDispatcher::start() {


	AsyncDispatcher c = create();
	std::thread thr([c] {c.run();});
	thr.detach();
	return c;
}

AsyncDispatcher AsyncDispatcher::start( CallbackExecutor executor) {


	AsyncDispatcher c = create();
	std::thread thr([c,executor] {c.run(executor);});
	thr.detach();
	return c;
}


AsyncDispatcher AsyncDispatcher::getSingleton() {

	static AsyncDispatcher singleton = AsyncDispatcher::start();
	return singleton;

}

}
