/*
 * dispatchqueue.h
 *
 *  Created on: Sep 9, 2017
 *      Author: ondra
 */

#pragma once


#include <functional>
#include "msgqueue.h"


///Queue which contains function to dispatch (message loop)
class Dispatcher {
public:

	typedef std::function<void()> Msg;
protected:
	MsgQueue<std::function<void()> > queue;

public:
	///starts message loop. Function processes messages
	void run() {
		Msg a = queue.pop();
		while (a != nullptr) {
			a();
			a = nullptr;
			a = queue.pop();
		}
	}

	///pump one message
	/**
	 * @retval true one message pumped
	 * @retval false there were no message to pump
	 */
	bool pump() {
		return queue.try_pump([](Msg msg){
			msg();
		});
	}

	///quits the dispatcher
	/**
	 * Dispatcher is quit once the dispatching thread returns to the pump
	 *
	 */
	void quit() {
		queue.push(nullptr);
	}


	///dispatch function
	void operator<<(const Msg &msg) {
		queue.push(msg);
	}

	///clear the queue
	/** Dispatching thread should clear the queue on exit. Otherwise the
	 * dispatched messages are cleared by the destructor
	 */
	void clear() {
		queue.clear();
	}

	///allows to use syntax function >> dispatcher
	friend void operator>>(const Msg &msg, Dispatcher &dispatcher) {
		dispatcher << msg;
	}

};

