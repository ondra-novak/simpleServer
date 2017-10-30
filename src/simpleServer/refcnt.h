#pragma once

#include <atomic>

namespace simpleServer {


	template<typename T> class RefCntPtr;

	///Simple refcounting 
	/** Because std::shared_ptr is too heavy a bloated and slow and wastes a lot memory */

	class RefCntObj {
	public:

		void addRef() const noexcept {
			++counter;
		}

		bool release() const noexcept {
			return --counter == 0;
		}

		RefCntObj():counter(0) {}

		bool isShared() const {
			return counter > 1;
		}


	protected:
		mutable std::atomic_int counter;

		//this function is dormant, but we need it to correctly compile
		void onRelease() noexcept {}

		template<typename T> friend class RefCntPtr;
	};



	///Simple refcounting, but with virtual table
	/** It allows to overwrite onRelease() */

	template<typename Base>
	class RefCntObjEx: public Base, public RefCntObj {
	public:
		using Base::Base;

	protected:
		template<typename T> friend class RefCntPtr;

		virtual ~RefCntObjEx() noexcept {}

		///Called before the object destroyed. It can throw exception
		virtual void onRelease() noexcept(false) {}

	};

	///Simple refcounting, but with virtual table
	/** It allows to overwrite onRelease() */
	template<>
	class RefCntObjEx<void>: public RefCntObj {
	protected:

		template<typename T> friend class RefCntPtr;

		virtual ~RefCntObjEx() noexcept {}

		///Called before the object destroyed. It can throw exception
		virtual void onRelease() noexcept(false) {}

	};


	///Simple refcounting smart pointer
	/** Because std::shared_ptr is too heavy a bloated and slow and wastes a lot memory */
	template<typename T>
	class RefCntPtr {
	public:

		RefCntPtr() :ptr(0)  {}
		RefCntPtr(T *ptr) :ptr(ptr)  { addRefPtr(); }
		RefCntPtr(const RefCntPtr &other)  :ptr(other.ptr) { addRefPtr(); }
		~RefCntPtr() noexcept(false) {
			releaseRefPtr();
		}
		RefCntPtr(RefCntPtr &&other) :ptr(other.ptr)  {
			other.ptr = nullptr;
		}



		RefCntPtr &operator=(const RefCntPtr &other) noexcept(false) {
			if (other.ptr != ptr) {
				if (other.ptr) other.ptr->addRef();
				releaseRefPtr();
				ptr = other.ptr;
			}
			return *this;
		}

		RefCntPtr &operator=(RefCntPtr &&other) {
			if (this != &other) {
				releaseRefPtr();
				ptr = other.ptr;
				other.ptr = nullptr;
			}
			return *this;
		}

		operator T *() const noexcept { return ptr; }
		T &operator *() const noexcept { return *ptr; }
		T *operator->() const noexcept { return ptr; }

		template<typename X>
		operator X *() const noexcept { return ptr; }

		template<typename X>
		static RefCntPtr<T> dynamicCast(const RefCntPtr<X> &other) noexcept {
			return RefCntPtr<T>(dynamic_cast<T *>((X *)other));
		}
		template<typename X>
		static RefCntPtr<T> staticCast(const RefCntPtr<X> &other) noexcept {
			return RefCntPtr<T>(static_cast<T *>((X *)other));
		}

		bool operator==(std::nullptr_t) const noexcept { return ptr == 0; }
		bool operator!=(std::nullptr_t) const noexcept { return ptr != 0; }
		bool operator==(const RefCntPtr &other) const noexcept { return ptr == other.ptr; }
		bool operator!=(const RefCntPtr &other) const noexcept { return ptr != other.ptr; }

	protected:
		T *ptr;

		void addRefPtr() noexcept {
			if (ptr) ptr->addRef();
		}
		void releaseRefPtr()  {
			if (ptr) {
				T *save = ptr;
				ptr = 0;
				if (save->release()) {
					try {
						save->onRelease();
					} catch (...) {
						delete save;
						throw;
					}
					delete save;
				}
			}
		}
	};

	template<typename T>
	class RefCntPtrNoExcept: public RefCntPtr<T> {
	public:
		using RefCntPtr<T>::RefCntPtr;
		RefCntPtrNoExcept() {}

		~RefCntPtrNoExcept() noexcept {}
	};


}
