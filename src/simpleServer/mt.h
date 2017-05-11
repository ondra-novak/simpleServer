#pragma once

#include <thread>
#include <vector>

namespace simpleServer {

template<typename Fn, class ...Args>
void runNoExcept(Fn fn, Args... x) noexcept(true) {
	fn(x...);
}

template<typename Fn1, typename Fn2>
void runThreads(unsigned int count, const Fn1 &threaded, const Fn2 &control) {

	std::vector<std::thread> threads;
	threads.reserve(count);
	for (unsigned int i = 0; i < count; i++) {
		std::thread thr(threaded);
		threads.push_back(std::move(thr));
	}
	runNoExcept(control);
	for (unsigned int i = 0; i < count; i++) {
		threads[i].join();
	}
}

template<typename Fn1>
void runThreads(unsigned int count, const Fn1 &threaded) {

	for (unsigned int i = 0; i < count; i++) {
		std::thread thr(threaded);
		thr.detach();
	}
}

template<typename Fn1, typename Fn2>
void forkThread(const Fn1 &child, const Fn2 &parent) {
	std::thread thr(child);
	runNoExcept(parent);
	thr.join();
}


}
