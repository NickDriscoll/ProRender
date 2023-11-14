#pragma once

#include <vector>
#include <stack>
#include <assert.h>

#define EXTRACT_IDX(key) (key & 0xFFFFFFFF)
#define EXTRACT_GENERATION(key) (key >> 32)

#define SLOTMAP_IMPLEMENTATION  //TODO: Figure out how to remove this

template<class T>
struct slotmap {

    void alloc(uint32_t size);
    uint32_t count();
    T* data();
    T* get(uint64_t key);
    uint64_t insert(T thing);
    bool is_live(uint64_t idx);
    void remove(uint32_t idx);

private:
    std::vector<T> _data = {};
    std::vector<uint32_t> generation_bits = {};
    std::vector<uint64_t> live_bits = {};           //Nth least significant bit indicates if slot N is live TODO: should just use top gen bit for this
    std::stack<uint32_t, std::vector<uint32_t>> free_indices;
    uint32_t _count = 0;
};

#ifdef SLOTMAP_IMPLEMENTATION
template<class T>
void slotmap<T>::alloc(uint32_t size) {
    assert(_data.size() == 0);
    _data.resize(size);
    generation_bits.resize(size);
    live_bits.resize(size);
    std::vector<uint32_t> free_inds;
    free_inds.resize(size);
    //TODO: There has to be a better way!
    for (uint32_t i = 0; i < size; i++) {
        free_inds[i] = size - i - 1;
    }
    for (uint32_t i = 0; i < size; i++) {
        live_bits[i] = 0;
    }
    
    free_indices = std::stack(free_inds);
}

template<class T>
uint32_t slotmap<T>::count() {
    return _count;
}

template<class T>
T* slotmap<T>::data() {
    return _data.data();
}

template<class T>
T* slotmap<T>::get(uint64_t key) {
    uint32_t idx = EXTRACT_IDX(key);
    uint32_t gen = static_cast<uint32_t>(EXTRACT_GENERATION(key));
    T* d = nullptr;
    if (gen == generation_bits[idx]) {
        d = &_data[idx];
    }
    return d;
}

template<class T>
uint64_t slotmap<T>::insert(T thing) {
    uint32_t free_idx = free_indices.top();
    free_indices.pop();
    _data[free_idx] = thing;
    uint32_t generation = generation_bits[free_idx];
    uint32_t live_idx = free_idx / 64;
    if (live_bits.size() <= live_idx) {
        live_bits.push_back((uint64_t)1 << ((uint64_t)free_idx % 64));
    } else {
        live_bits[live_idx] |= (uint64_t)1 << ((uint64_t)free_idx % 64);
    }
    _count += 1;
    return (static_cast<uint64_t>(generation) << 32) | static_cast<uint64_t>(free_idx);
}

template<class T>
bool slotmap<T>::is_live(uint64_t idx) {
    if (_data.size() == 0) return false;
    uint64_t i = idx / 64;
    return live_bits[i] & ((uint64_t)1 << ((uint64_t)idx % 64));
}

template<class T>
void slotmap<T>::remove(uint32_t idx) {
    free_indices.push(idx);
    generation_bits[idx] += 1;
    _count -= 1;
    uint32_t live_idx = idx / 64;
    live_bits[live_idx] &= ~((uint64_t)1 << ((uint64_t)idx % 64));
}

#endif