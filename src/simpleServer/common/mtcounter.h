#pragma once
#include <condition_variable>
#include <mutex>


///Counts up and down, and allows to thread to wait for the zero
/**
 * One group of threads is able to increase and decrease the counter.
 * Other group of threads can be stopped until the zero is reached.
 *
 * Useful in destructor while waiting to complete all pending operations.
 * Counter is used to count pending operations. Once the counter
 * reaches the zero, destruction can continue
 */
class MTCounter {
public:

	void inc() {
		std::unique_lock<std::mutex> _(lock);
		counter++;
	}
	void dec() {
		std::unique_lock<std::mutex> _(lock);
		if (counter) {
			counter--;
			if (counter == 0) {
				waiter.notify_all();
			}
		}
	}
	bool zeroWait(unsigned int timeout_ms) {
		std::unique_lock<std::mutex> _(lock);
		return waiter.wait_for(_,std::chrono::milliseconds(timeout_ms), [=]{return counter == 0;});
	}
	void zeroWait() {
		std::unique_lock<std::mutex> _(lock);
		waiter.wait(_,[=]{return counter == 0;});
	}

	unsigned int getCounter() const {
		return counter;
	}

	void setCounter(unsigned int counter) {
		std::unique_lock<std::mutex> _(lock);
		this->counter = counter;
		if (counter == 0) {
			waiter.notify_all();
		}
	}

protected:
	std::mutex lock;
	std::condition_variable waiter;
	unsigned int counter = 0;
};
