#pragma once
#include <functional>
#include "shared/refcnt.h"
#include "stringview.h"



namespace simpleServer {

class Stream;

using ondra_shared::RefCntObj;
using ondra_shared::RefCntPtr;


typedef StringView<StrViewA> ArgList;


typedef std::function<int(ArgList, Stream output)> UserCommandFn;




class AbstractServiceControl: public RefCntObj {
public:

	///Dispatches service messages
	/** The main thread should stop here and dispatch messages.
	 * Function is left when stop command is dispatched
	 *
	 * @note on the beginning of the function, the program detaches from the console
	 * and closes standard input and output
	 */
	virtual void dispatch() = 0;
	///Adds custom command to the dispatcher
	/**
	 * @param command name
	 * @param fn function to call
	 *
	 * @note function can be called from the other thread, or in a handler
	 */
	virtual void addCommand(StrViewA command, UserCommandFn fn) = 0;
	///Stops dispatching
	/** It has the same effect as command "stop" */
	virtual void stop() = 0;

};


class ServiceControl;


///Definition if service handler
/**
 * Service handler is called when service starts. It expects, that service
 * performs an initialization and then calls ServiceControl::dispatch.
 * When that function returns, it performs cleanup and exits the handler
 *
 * @param ServiceControl control object
 * @param ArgList arguments (first argument is command used to start the service)
 *
 * Inside od handler, std::cin and std::cout are available, until the dispatch()
 * is called
 *
 * @return return value from the service initialization
 *
 */
typedef std::function<int(ServiceControl, StrViewA name, ArgList)> ServiceHandler;


///Service control object.
/** You need to create this object in order to run your application as a service.
 *
 * To create the object, call the ServiceControl::create()
 */
class ServiceControl: public RefCntPtr<AbstractServiceControl> {
public:

	using RefCntPtr<AbstractServiceControl>::RefCntPtr;


	///Enter to service mode
	/**
	 * @param argc argument count
	 * @param argv arguments from main
	 * @param name name of the service
	 * @param handler function called to implement service
	 * @return function returns the exit code from the program
	 */
	static int create(int argc, char **argv, StrViewA name, ServiceHandler handler);


	void dispatch() const {
		ptr->dispatch();
	}

	void addCommand(StrViewA command, UserCommandFn fn) const {
		ptr->addCommand(command, fn);
	}
	void stop() const {
		ptr->stop();
	}

};




}
