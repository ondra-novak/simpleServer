#pragma once
#include <condition_variable>
#include <mutex>
#include <queue>




template<typename Msg, typename QueueImpl = std::queue<Msg> >
class MsgQueue {
public:

	///Push message to the queue (no blocking)
	void push(const Msg &msg);

	///Push message to the queue (no blocking)
	void push(Msg &&msg);

	///Pop message from the queue
	/** Function blocks if there is no message
	 *
	 * @return message retrieved from the queue
	 */
	Msg pop();

	///Determines whether queue is empty
	bool empty();

	///checks the queue, If there is an message, it calls the function with the message as an argument
	/**
	 *
	 * @param fn function called with the message
	 * @retval true pumped a message
	 * @retval false there were no message
	 */
	template<typename Fn>
	bool try_pump(Fn fn);

	///Pumps one message or block the thread
	/**
	 * Function works similar as pop(), but the message is not returned, instead it
	 * is passed to the function as an argument
	 *
	 * @param fn function called with message
	 *
	 * @note only one message is processed
	 */
	template<typename Fn>
	void pump(Fn fn);

	///Pumps one message or blocks the thread for the given period of the time
	/**
	 *
	 * @param rel_time relative time - duration
	 * @param fn function called with the message
	 * @retval true pumped a message
	 * @retval false there were no message and timeout ellapsed
	 */
	template< class Rep, class Period, class Fn>
	bool pump_for(const std::chrono::duration<Rep, Period>& rel_time, Fn fn);

	///Pumps one message or blocks the thread for the given period of the time
	/**
	 *
	 * @param rel_time relative time - duration
	 * @param fn function called with the message
	 * @retval true pumped a message
	 * @retval false there were no message and timeout ellapsed
	 */
	template< class Clock, class Duration, class Fn >
	bool pump_until(const std::chrono::time_point<Clock, Duration>& timeout_time,Fn pred );


	///Allows to modify content of the queue.
	/** Function locks the object and calls the function with the instance
	 * of the queue as the argument. Function can modify content of the queue.
	 * The queue is unlocked upon the return
	 * @param fn function called to modify the queue.
	 */
	template<typename Fn>
	void modifyQueue(Fn fn);

	void clear();


protected:
	QueueImpl queue;
	std::mutex lock;
	std::condition_variable condvar;
	typedef std::unique_lock<std::mutex> Sync;
};


template<typename Msg, typename QueueImpl>
inline void MsgQueue<Msg, QueueImpl>::push(const Msg& msg) {
	Sync _(lock);
	queue.push(msg);
	condvar.notify_one();
}

template<typename Msg, typename QueueImpl>
inline void MsgQueue<Msg, QueueImpl>::push(Msg&& msg) {
	Sync _(lock);
	queue.push(std::move(msg));
	condvar.notify_one();
}

template<typename Msg, typename QueueImpl>
inline Msg MsgQueue<Msg, QueueImpl>::pop() {
	Sync _(lock);
	condvar.wait(_, [&]{return !queue.empty();});
	Msg ret = queue.front();
	queue.pop();
	return ret;
}

template<typename Msg, typename QueueImpl>
inline bool MsgQueue<Msg, QueueImpl>::empty() {
	Sync _(lock);
	return queue.empty();
}

template<typename Msg, typename QueueImpl>
template<typename Fn>
inline bool MsgQueue<Msg, QueueImpl>::try_pump(Fn fn) {
	Sync _(lock);
	if (queue.empty()) return false;
	Msg ret = queue.front();
	queue.pop();
	_.unlock();
	fn(ret);
	return true;
}

template<typename Msg, typename QueueImpl>
template<typename Fn>
inline void MsgQueue<Msg, QueueImpl>::pump(Fn fn) {
	Sync _(lock);
	condvar.wait(_, [&]{return !queue.empty();});
	Msg ret = queue.front();
	queue.pop();
	_.unlock();
	fn(ret);
}

template<typename Msg, typename QueueImpl>
template<class Rep, class Period, class Fn>
inline bool MsgQueue<Msg, QueueImpl>::pump_for(
		const std::chrono::duration<Rep, Period>& rel_time, Fn fn) {
	Sync _(lock);
	if (!condvar.wait_for(_, rel_time, [&]{return !queue.empty();})) return false;
	Msg ret = queue.front();
	queue.pop();
	_.unlock();
	fn(ret);
}

template<typename Msg, typename QueueImpl>
template<class Clock, class Duration, class Fn>
inline bool MsgQueue<Msg, QueueImpl>::pump_until(
		const std::chrono::time_point<Clock, Duration>& timeout_time, Fn fn) {
	Sync _(lock);
	if (!condvar.wait_until(_, timeout_time, [&]{return !queue.empty();})) return false;
	Msg ret = queue.front();
	queue.pop();
	_.unlock();
	fn(ret);
}

template<typename Msg, typename QueueImpl>
template<typename Fn>
inline void MsgQueue<Msg, QueueImpl>::modifyQueue(Fn fn) {
	Sync _(lock);
	fn(queue);
	condvar.notify_one();
}

template<typename Msg, typename QueueImpl>
inline void MsgQueue<Msg, QueueImpl>::clear() {
	Sync _(lock);
	queue = QueueImpl();
}
