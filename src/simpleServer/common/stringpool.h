#pragma once

#include <vector>

#include "../stringview.h"

namespace simpleServer {



template<typename T>
class StringPoolTraits {
public:
	static inline void putEnd(std::vector<T> &buffer) {}
	static inline const T *empty(std::vector<T> &buffer) {return nullptr;}
};

template<>
class StringPoolTraits<char> {
public:
	static inline void putEnd(std::vector<char> &buffer) {buffer.push_back((char)0);}
	static inline const char *empty() {return "";}
};

template<>
class StringPoolTraits<wchar_t> {
public:
	static inline void putEnd(std::vector<wchar_t> &buffer) {buffer.push_back((wchar_t)0);}
	static inline const wchar_t *empty() {return L"";}
};



template<typename T>
class StringPool {
public:

	class String {
	public:
		String(const std::vector<T> &buffer, std::size_t offset, std::size_t length)
			:buffer(&buffer),offset(offset),length(length) {}
		String(const StringView<T> &str):buffer(nullptr),ptr(str.data),length(str.length) {}
		String():buffer(nullptr),ptr(StringPoolTraits<T>::empty()),length(0) {}

		operator StringView<T>() const {
			return getView();
		}

		StringView<T> getView() const {
			if (buffer) return StringView<T>(buffer->data(), buffer->size()).substr(offset,length);
			else return StringView<T>(ptr, length);
		}

		bool operator==(const String &other) const {return getView() == other.getView();}
		bool operator!=(const String &other) const {return getView() != other.getView();}
		bool operator>(const String &other) const {return getView() > other.getView();}
		bool operator<(const String &other) const {return getView() < other.getView();}
		bool operator>=(const String &other) const {return getView()>= other.getView();}
		bool operator<=(const String &other) const {return getView()<= other.getView();}
	protected:
		const std::vector<T> *buffer;
		union {
			std::size_t offset;
			const T *ptr;
		};
		std::size_t length;
	};


	String add(const StringView<T> &str) {
		if (str.empty()) return String();
		std::size_t pos = data.size();
		data.insert(data.end(), str.begin(), str.end());
		StringPoolTraits<T>::putEnd(data);
		return String(data, pos, str.length);
	}

	std::size_t begin_add() {
		return data.size();
	}

	void push_back(const T &t) {
		data.push_back(t);
	}
	String end_add(std::size_t mark) {
		std::size_t pos = data.size();
		StringPoolTraits<T>::putEnd(data);
		return String(data, mark, pos-mark);
	}

	void clear() {
		data.clear();
	}

protected:
	std::vector<T> data;
};

}

