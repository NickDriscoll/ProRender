#include <vector>
#include <stack>
#include <assert.h>

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
            free_inds[i] = i;
        }
        free_indices = std::stack(free_inds);
    }

    T& get(uint64_t key) {
        uint32_t idx = key & 0x00000000FFFFFFFF;
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

        if (live_bits.size() <= (free_idx / 64)) {
            
        }

        return (static_cast<uint64_t>(generation) << 32) | static_cast<uint64_t>(free_idx);
    }

    void remove(uint64_t key) {
        uint32_t idx = key & 0x00000000FFFFFFFF;
        uint32_t gen = key >> 32;
        free_indices.push(idx);
        generation_bits[idx] += 1;
    }

    uint32_t size() {
        return data.size();
    }

private:
    std::vector<T> data = {};
    std::vector<uint32_t> generation_bits = {};
    std::vector<uint64_t> live_bits = {};
    std::stack<uint32_t, std::vector<uint32_t>> free_indices;
};