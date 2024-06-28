static const float PI = acos(-1);   //Most accurate Pi possible

//idx should be in units of T
template<typename T>
T raw_block_load(uint64_t base_addr, uint blocksize, uint idx) {
    uint items_per_block = 64 / sizeof(T);
    uint64_t addr = base_addr + blocksize * (idx / items_per_block);
    addr += sizeof(T) * (idx % items_per_block);
    return vk::RawBufferLoad<T>(addr);
}