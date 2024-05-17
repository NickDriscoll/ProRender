#pragma once

#include <vector>
#include <stack>
#include <assert.h>

#define LIVE_BIT 0x80000000
#define EXTRACT_IDX(key) (key & 0xFFFFFFFF)
#define EXTRACT_GENERATION(key) ((key >> 32) &= ~LIVE_BIT)

#define SLOTMAP_IMPLEMENTATION  //TODO: Figure out how to remove this


//Slotmap key type
template<typename PHANTOM>
struct Key {
    Key() : data(0) {}
	Key(uint64_t key) : data(key) {}
	inline uint64_t value() { return this->data; }
    bool operator==(const Key<PHANTOM> other) { return data == other.value(); }
private:
	uint64_t data;
};

template<typename T, typename Tkey = Key<T>>
struct slotmap {
    //Iterator impl for slotmap
    struct iterator {
        using Element = T;
        using Pointer = Element*;
        using Reference = Element&;

        iterator(Pointer c, Pointer s, uint32_t e, uint32_t* gen_bits) {
            current = c;
            start = s;
            end_idx = e;
            _generation_bits = gen_bits;
        }
        Reference operator*() const { return *current; }
        iterator& operator++() {
            current += 1;
            while ((current != start + end_idx) && (_generation_bits[current - start] & LIVE_BIT) == 0) {
                current += 1;
            }
            return *this;
        }
        iterator operator++(int) {
            iterator tmp = this;
            ++(*this);
            return tmp;
        }
        Reference operator[](int index) { return *(start + index); }
        Pointer operator->() { return current; }
        bool operator==(const iterator& other) { return current == other.current; }
        bool operator!=(const iterator& other) { return current != other.current; }
        auto operator<=>(const iterator &) const = default; // three-way comparison C++20

        uint32_t slot_index() {
            return (uint32_t)(current - start);
        }

        uint32_t generation_bits() {
            return _generation_bits[current - start];
        }

    private:
        Pointer current, start;
        uint32_t end_idx;
        uint32_t* _generation_bits;
    };
    iterator begin() {
        if (_count == 0) return iterator(_data.data(), _data.data(), largest_free_idx, generation_bits.data());
        T* current = _data.data();
        while ((generation_bits[current - _data.data()] & LIVE_BIT) == 0) {
            current += 1;
        }
        return iterator(current, _data.data(), largest_free_idx, generation_bits.data());
    }

    iterator end() {
        return iterator(_data.data() + largest_free_idx, _data.data(), largest_free_idx, generation_bits.data());
    }

    void alloc(uint32_t size);
    uint32_t count();
    void clear();
    T* data();
    T* get(Tkey key);
    Tkey insert(T thing);
    void remove(uint32_t idx);
    uint32_t size();

private:
    std::vector<T> _data = {};
    std::vector<uint32_t> generation_bits = {};
    std::stack<uint32_t, std::vector<uint32_t>> free_indices;
    uint32_t _count = 0;
    uint32_t largest_free_idx = 0;
};

#ifdef SLOTMAP_IMPLEMENTATION
template<typename T, typename Tkey>
void slotmap<T, Tkey>::alloc(uint32_t size) {
    assert(_data.size() == 0);
    _data.resize(size);
    generation_bits.resize(size);
    std::vector<uint32_t> free_inds;
    free_inds.resize(size);
    //TODO: There has to be a better way!
    for (uint32_t i = 0; i < size; i++) {
        free_inds[i] = size - i - 1;
        generation_bits[i] = 1;
    }
    
    free_indices = std::stack(free_inds);
}

template<typename T, typename Tkey>
uint32_t slotmap<T, Tkey>::count() {
    return _count;
}

template<typename T, typename Tkey>
uint32_t slotmap<T, Tkey>::size() {
    return static_cast<uint32_t>(_data.capacity());
}

template<typename T, typename Tkey>
void slotmap<T, Tkey>::clear() {
    _count = 0;
    largest_free_idx = 0;
    size_t size = _data.capacity();
    _data.clear();
    _data.resize(size);
    generation_bits.clear();
    generation_bits.resize(size);
    
    std::vector<uint32_t> free_inds;
    free_inds.resize(size);
    //TODO: There has to be a better way!
    for (uint32_t i = 0; i < size; i++) {
        free_inds[i] = (uint32_t)size - i - 1;
    }
    
    free_indices = std::stack(free_inds);
}

template<typename T, typename Tkey>
T* slotmap<T, Tkey>::data() {
    return _data.data();
}

template<typename T, typename Tkey>
T* slotmap<T, Tkey>::get(Tkey key) {
    uint32_t idx = EXTRACT_IDX(key.value());
    uint32_t gen = static_cast<uint32_t>(key.value() >> 32);
    T* d = nullptr;
    if (gen == generation_bits[idx]) {
        d = &_data[idx];
    }
    return d;
}

template<typename T, typename Tkey>
Tkey slotmap<T, Tkey>::insert(T thing) {
    uint32_t free_idx = free_indices.top();
    free_indices.pop();
    if (free_idx >= largest_free_idx) largest_free_idx += 1;
    _data[free_idx] = thing;
    generation_bits[free_idx] |= LIVE_BIT;
    uint32_t generation = generation_bits[free_idx];
    _count += 1;
    uint64_t data = (static_cast<uint64_t>(generation) << 32) | static_cast<uint64_t>(free_idx);
    return Tkey(data);
}

template<typename T, typename Tkey>
void slotmap<T, Tkey>::remove(uint32_t idx) {
    free_indices.push(idx);
    generation_bits[idx] &= ~LIVE_BIT;
    generation_bits[idx] += 1;
    _count -= 1;

    //Recalculate largest free index
    if (largest_free_idx == idx + 1) {
        uint32_t n = idx;
        while ((generation_bits[n] & LIVE_BIT) == 0) {
            largest_free_idx = n;
            if (n == 0) break;
            n -= 1;
        }
    }
}

#endif