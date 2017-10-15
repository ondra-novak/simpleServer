#pragma once



///Declares type with automatick cleanup when variable of this type is destroyed
/**
 * @tparam Res type of resource
 * @tparam FnType type of cleanup function. You can use decltype(&cleanupFn)
 * @tparam fn existing function. The function must have external linkage
 */
template<typename Res, typename FnType, FnType fn>
class RAII {
public:
	///Declaration of empty uninitialized resource
	RAII():owner(false) {}
	///Create initialized resource
	RAII(const Res &res):res(res),owner(true) {}
	///Move constructor - moves ownership to the new instance
	RAII(RAII &&other):res(other.res),owner(other.owner) {other.owner = false;}
	///Move assignment
	/**
	 * First it releases current resource and then moves the other resource to this resource
	 * @param other
	 * @return
	 */
	RAII &operator=(RAII &&other) {
		if (owner) {
			fn(res);
		}
		res = other.res;
		owner = other.owner;
		other.owner = false;
		return *this;
	}
	///Cleanups resource if it is owned
	~RAII() {
		if (owner) fn(res);
	}
	///Returns the resource
	operator const Res &() const {return res;}
	///Detaches from ownership
	/** Useful if you giving the resource to the 3rd party */
	const Res &detach() {
		owner = false;
		return res;
	}
	void swap(RAII &b) {
		std::swap(res,b.res);
		std::swap(owner, b.owner);
	}

protected:
	Res res;
	bool owner;
};


namespace std {

template<typename Res, typename FnType, FnType fn>
void swap(RAII<Res,FnType,fn> &a, RAII<Res,FnType,fn> &b) {
	a.swap(b);
}

}
