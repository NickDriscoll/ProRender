#pragma once

#include <vector>
#include <stack>
#include <assert.h>

#define LIVE_BIT 0x80000000
#define EXTRACT_IDX(key) (key & 0xFFFFFFFF)
#define EXTRACT_GENERATION(key) ((key >> 32) &= ~LIVE_BIT)

#define SLOTMAP_IMPLEMENTATION  //TODO: Figure out how to remove this

template<typename T>
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
            generation_bits = gen_bits;
        }
        Reference operator*() const { return *current; }
        iterator& operator++() {
            current += 1;
            while ((current != start + end_idx) && (generation_bits[current - start] & LIVE_BIT) == 0) {
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

        uint32_t underlying_index() {
            return current - start;
        }

    private:
        Pointer current, start;
        uint32_t end_idx;
        uint32_t* generation_bits;
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
    T* get(uint64_t key);
    uint64_t insert(T thing);
    bool is_live(uint32_t idx);
    void remove(uint32_t idx);

private:
    std::vector<T> _data = {};
    std::vector<uint32_t> generation_bits = {};
    std::stack<uint32_t, std::vector<uint32_t>> free_indices;
    uint32_t _count = 0;
    uint32_t largest_free_idx = 0;
};

#ifdef SLOTMAP_IMPLEMENTATION
template<typename T>
void slotmap<T>::alloc(uint32_t size) {
    assert(_data.size() == 0);
    _data.resize(size);
    generation_bits.resize(size);
    std::vector<uint32_t> free_inds;
    free_inds.resize(size);
    //TODO: There has to be a better way!
    for (uint32_t i = 0; i < size; i++) {
        free_inds[i] = size - i - 1;
    }
    
    free_indices = std::stack(free_inds);
}

template<typename T>
uint32_t slotmap<T>::count() {
    return _count;
}

template<typename T>
void slotmap<T>::clear() {
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
        free_inds[i] = size - i - 1;
    }
    
    free_indices = std::stack(free_inds);
}

template<typename T>
T* slotmap<T>::data() {
    return _data.data();
}

template<typename T>
T* slotmap<T>::get(uint64_t key) {
    uint32_t idx = EXTRACT_IDX(key);
    uint32_t gen = static_cast<uint32_t>(key >> 32);
    T* d = nullptr;
    if (gen == generation_bits[idx]) {
        d = &_data[idx];
    }
    return d;
}

template<typename T>
uint64_t slotmap<T>::insert(T thing) {
    uint32_t free_idx = free_indices.top();
    free_indices.pop();
    if (free_idx >= largest_free_idx) largest_free_idx += 1;
    _data[free_idx] = thing;
    generation_bits[free_idx] |= LIVE_BIT;
    uint32_t generation = generation_bits[free_idx];
    _count += 1;
    return (static_cast<uint64_t>(generation) << 32) | static_cast<uint64_t>(free_idx);
}

template<typename T>
bool slotmap<T>::is_live(uint32_t idx) {
    if (_data.size() == 0) return false;
    return generation_bits[idx] & LIVE_BIT;
}

template<typename T>
void slotmap<T>::remove(uint32_t idx) {
    if (largest_free_idx == idx + 1) {
        uint32_t n = idx;
        while ((generation_bits[n] & LIVE_BIT) == 0) {
            largest_free_idx = n;
            n -= 1;
        }
    }

    free_indices.push(idx);
    generation_bits[idx] &= ~LIVE_BIT;
    generation_bits[idx] += 1;
    _count -= 1;
}

#endif