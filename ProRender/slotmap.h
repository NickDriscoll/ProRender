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
    std::stack<uint32_t, std::vector<uint32_t>> free_indices;
    uint32_t _count = 0;
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
    _data[free_idx] = thing;
    generation_bits[free_idx] |= LIVE_BIT;
    uint32_t generation = generation_bits[free_idx];
    _count += 1;
    return (static_cast<uint64_t>(generation) << 32) | static_cast<uint64_t>(free_idx);
}

template<typename T>
bool slotmap<T>::is_live(uint64_t idx) {
    if (_data.size() == 0) return false;
    return generation_bits[idx] & LIVE_BIT;
}

template<typename T>
void slotmap<T>::remove(uint32_t idx) {
    free_indices.push(idx);
    generation_bits[idx] &= ~LIVE_BIT;
    generation_bits[idx] += 1;
    _count -= 1;
}

#endif