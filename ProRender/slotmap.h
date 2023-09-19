#include <vector>
#include <stack>
#include <assert.h>

#define EXTRACT_IDX(key) (key & 0x00000000FFFFFFFF)

template<class T>
struct slotmap {
    void alloc(uint32_t size) {
        assert(data.size() == 0);

        data.resize(size);
        generation_bits.resize(size);

        //TODO: There has to be a better way!
        std::vector<uint32_t> free_inds;
        free_inds.resize(size);
        for (uint32_t i = 0; i < size; i++) {
            free_inds[i] = size - i - 1;
        }
        free_indices = std::stack(free_inds);
    }

    T* ptr() {
        return data.data();
    }

    T& get(uint64_t key) {
        uint32_t idx = EXTRACT_IDX(key);
        uint32_t gen = static_cast<uint32_t>(key >> 32);
        T& d;
        if (gen == generation_bits[idx]) {
            d = data[idx];
        } else {
            d = nullptr;
        }
        return d;
    }

    uint64_t insert(T thing) {
        uint32_t free_idx = free_indices.top();
        free_indices.pop();
        data[free_idx] = thing;
        uint32_t generation = generation_bits[free_idx];

        uint32_t live_idx = free_idx / 64;
        if (live_bits.size() <= live_idx) {
            live_bits.push_back(1 << (free_idx % 64));
        } else {
            live_bits[live_idx] |= 1 << (free_idx % 64);
        }

        _size += 1;

        return (static_cast<uint64_t>(generation) << 32) | static_cast<uint64_t>(free_idx);
    }

    bool is_live(uint32_t idx) {
        if (data.size() == 0) return false;

        uint32_t i = idx / 64;
        return live_bits[i] & (1 << (idx % 64));
    }

    void remove(uint64_t key) {
        uint32_t idx = EXTRACT_IDX(key);
        uint32_t gen = static_cast<uint32_t>(key >> 32);
        free_indices.push(idx);
        generation_bits[idx] += 1;

        _size -= 1;

        uint32_t live_idx = idx / 64;
        live_bits[live_idx] &= ~static_cast<uint64_t>(1 << (idx % 64));
    }

    uint32_t size() {
        return _size;
    }

private:
    std::vector<T> data = {};
    std::vector<uint32_t> generation_bits = {};
    std::vector<uint64_t> live_bits = {};
    std::stack<uint32_t, std::vector<uint32_t>> free_indices;
    uint32_t _size = 0;
};