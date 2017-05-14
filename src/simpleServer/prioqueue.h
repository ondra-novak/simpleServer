#pragma once
#include <queue>
#include <vector>

template<typename V, typename Cmp = std::less<V> >
class PrioQueue: public std::priority_queue<V, std::vector<V>, Cmp> {
public:

	using std::priority_queue<V, std::vector<V>, Cmp>::priority_queue;
	typedef std::priority_queue<V, std::vector<V>, Cmp> Super;


	void remove_at(std::size_t pos) {
		std::size_t last = this->c.size()-1;
		if (pos > last) return;
		if (pos != last) {
			std::swap(this->c[pos],this->c[last]);
			std::size_t p2 = pos*2;
			while (p2 < last) {
				if (p2+1 < last && this->comp(this->c[p2],this->c[p2+1])) {
					++p2;
				}
				if (this->comp(this->c[pos],this->c[p2])) {
					std::swap(this->c[pos],this->c[p2]);
					pos = p2;
					p2 = pos*2;
				}else {
					break;
				}
			}
		}
		this->c.pop_back();

	}

	typedef  std::vector<V> Container;
	const Container &getContent() const {return this->c;}
	Container &getContent() {return this->c;}



};

