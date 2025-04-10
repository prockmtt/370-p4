/*
 * EECS 370, University of Michigan
 * Project 4: LC-2K Cache Simulator
 * Instructions are found in the project spec.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAX_CACHE_SIZE 256
#define MAX_BLOCK_SIZE 256

// **Note** this is a preprocessor macro. This is not the same as a function.
// Powers of 2 have exactly one 1 and the rest 0's, and 0 isn't a power of 2.
#define is_power_of_2(val) (val && !(val & (val - 1)))


/*
 * Accesses 1 word of memory.
 * addr is a 16-bit LC2K word address.
 * write_flag is 0 for reads and 1 for writes.
 * write_data is a word, and is only valid if write_flag is 1.
 * If write flag is 1, mem_access does: state.mem[addr] = write_data.
 * The return of mem_access is state.mem[addr].
 */
extern int mem_access(int addr, int write_flag, int write_data);

/*
 * Returns the number of times mem_access has been called.
 */
extern int get_num_mem_accesses(void);

//Use this when calling printAction. Do not modify the enumerated type below.
enum actionType {
    cacheToProcessor,
    processorToCache,
    memoryToCache,
    cacheToMemory,
    cacheToNowhere
};

/* You may add or remove variables from these structs */
typedef struct blockStruct {
    int data[MAX_BLOCK_SIZE];
    int dirty;
    int lruLabel;
    int tag;
    int valid;
} blockStruct;

typedef struct cacheStruct {
    blockStruct blocks[MAX_CACHE_SIZE];
    int blockSize;
    int numSets;
    int blocksPerSet;
    // add any variables for end-of-run stats
    int numHits;
    int numMisses;
    int numDirty;
    int numWritebacks;
} cacheStruct;

/* Global Cache variable */
cacheStruct cache;

void printAction(int, int, enum actionType);
void printCache(void);

/*
 * Set up the cache with given command line parameters. This is
 * called once in main(). You must implement this function.
 */
void cache_init(int blockSize, int numSets, int blocksPerSet) {
    if (blockSize <= 0 || numSets <= 0 || blocksPerSet <= 0) {
        printf("error: input parameters must be positive numbers\n");
        exit(1);
    }
    if (blocksPerSet * numSets > MAX_CACHE_SIZE) {
        printf("error: cache must be no larger than %d blocks\n", MAX_CACHE_SIZE);
        exit(1);
    }
    if (blockSize > MAX_BLOCK_SIZE) {
        printf("error: blocks must be no larger than %d words\n", MAX_BLOCK_SIZE);
        exit(1);
    }
    if (!is_power_of_2(blockSize)) {
        printf("warning: blockSize %d is not a power of 2\n", blockSize);
    }
    if (!is_power_of_2(numSets)) {
        printf("warning: numSets %d is not a power of 2\n", numSets);
    }
    printf("Simulating a cache with %d total lines; each line has %d words\n",
        numSets * blocksPerSet, blockSize);
    printf("Each set in the cache contains %d lines; there are %d sets\n",
        blocksPerSet, numSets);

    // TODO check how I init cache
    cache.blockSize = blockSize;
    cache.blocksPerSet = blocksPerSet;
    cache.numSets = numSets;
    cache.numHits = 0;
    cache.numMisses = 0;
    cache.numDirty = 0;
    cache.numWritebacks = 0;
    for (int idx = 0; idx < MAX_CACHE_SIZE; ++idx) {
        cache.blocks[idx].dirty = 0;
        cache.blocks[idx].lruLabel = 0;
        cache.blocks[idx].valid = 0;
        cache.blocks[idx].tag = 0;
    }

    return;
}

/*
 * Access the cache. This is the main part of the project,
 * and should call printAction as is appropriate.
 * It should only call mem_access when absolutely necessary.
 * addr is a 16-bit LC2K word address.
 * write_flag is 0 for reads (fetch/lw) and 1 for writes (sw).
 * write_data is a word, and is only valid if write_flag is 1.
 * The return of mem_access is undefined if write_flag is 1.
 * Thus the return of cache_access is undefined if write_flag is 1.
 */
int cache_access(int addr, int write_flag, int write_data) {
    // TODO: fix, debug
    // tag, index, block offset based on blockSize, numSets
    // addr is a 16-bit LC2K word address
    int blockOffsetSize = log2(cache.blockSize);
    int setBitsSize = log2(cache.numSets);
    // extract tag bits from address
    int tag = addr >> (blockOffsetSize + setBitsSize);
    // extract set index, block offset from address-- 
    // TODO adjust how I use blockOffset and setIdx in rest of code!!!
    int mask = 0 << 16;
    for (int i = 0; i < blockOffsetSize; ++i) {
        mask |= (0b1 << i);
    }
    // printf("mask for block offset: 0x%08X\n", mask);
    int blockOffset = addr & mask;
    // printf("block offset: 0x%08X\n", blockOffset);
    for (int i = 0; i < setBitsSize; ++i) {
        mask |= (0b1 << (blockOffsetSize + i));
    }
    // printf("mask for set idx: 0x%08X\n", mask);
    int setIndex = addr & mask;
    setIndex >>= blockOffsetSize;
    // printf("set idx: 0x%08X\n", setIndex);
    // debug printouts
    // printf("write_flag: %d write_data: %d\n", write_flag, write_data);
    // printf("address: 0x%08x\n", addr);
    // printf("tag: 0x%08X blockOffset: %d setIndex: %d\n", 
    //        tag, blockOffset, setIndex);

    int numLines = cache.numSets * cache.blocksPerSet;
    // check if tag is a hit in cache
    bool foundInCache = false;
    int idxFound = 0;
    // TODO fix how I find this-- possibly using setIndex and tag wrong
    for (int idx = 0; idx < numLines; ++idx) {
        int currSet = idx % cache.numSets;
        if (currSet == setIndex && cache.blocks[idx].valid && cache.blocks[idx].tag == tag) {
            // hit!
            // printf("idx %d hit in cache\n", idx);
            cache.numHits += 1;
            foundInCache = true;
            idxFound = idx;
            break;
        }
    }
    if (!foundInCache) {
        cache.numMisses += 1;
        // printf("addr 0x%08X not found in cache\n", addr);
        // cache miss case; pull from mem to cache before R/W
        // first check if cache is full-- we know where to search in cache based 
        // on set size and the set that our input would be put in. assume full
        int idxToStore = 0;
        bool full = true;
        for (int idx = 0; idx < numLines; ++idx) {
            int currSet = idx % cache.numSets;
            if (currSet == setIndex && (!cache.blocks[idx].valid)) {
                full = false;
                idxToStore = idx;
                // printf("next empty idx in cache: %d\n", idxToStore);
                break;
            }
        }
        if (full) {
            // printf("eviction case\n");
            // eviction case
            // find LRU
            int lruIdx = 0;
            int maxLru = 0;
            for (int idx = 0; idx < numLines; ++idx) {
                if ((idx % cache.numSets) == setIndex && 
                    cache.blocks[idx].valid &&
                    cache.blocks[idx].lruLabel > maxLru) {
                    maxLru = cache.blocks[idx].lruLabel;
                    lruIdx = idx;
                }
            }
            // printf("lru index in cache: %d\n", lruIdx);
            // set place to put data correctly?
            idxToStore = lruIdx;
            if (cache.blocks[lruIdx].dirty) {
                // printf("block dirty\n");
                // have to write back before evicting and replacing data
                // TODO check how I calc this (should be tag and set idx of data
                // being evicted)
                int start = cache.blocks[idxToStore].tag << (blockOffsetSize + setBitsSize);
                start |= setIndex << blockOffsetSize;
                printAction(start, cache.blockSize, cacheToMemory);
                // TODO check how I write back for each word in block
                // writing to mem
                int currWrite = 1;
                for (int idx = 0; idx < cache.blockSize; ++idx) {
                    int currData = cache.blocks[lruIdx].data[idx];
                    mem_access(start, currWrite, currData);
                    start += 1;
                }
                cache.numWritebacks += 1;
                // printCache();
            }
            else {
                // removing from cache; cache to nowhere case
                int start = cache.blocks[idxToStore].tag << (blockOffsetSize + setBitsSize);
                start |= setIndex << blockOffsetSize;
                printAction(start, cache.blockSize, cacheToNowhere);
                // printCache();
            }
            cache.blocks[idxToStore].dirty = 0;
            cache.numDirty -= 1;
        }
        // atp, we've cleared/found idx to put data from mem
        cache.blocks[idxToStore].valid = 1;
        cache.blocks[idxToStore].tag = tag;
        // fill in new empty spot
        // TODO check calcs
        int end = addr + blockOffset;
        if (blockOffset == 0) {
            end = cache.blockSize;
            // printf("if blockOffset is 0 end = %d\n", end);
        }
        else if (blockOffset == cache.blockSize - 1) {
            end = addr + 1;
        }
        int start = end - cache.blockSize;
        // printf("start = end - block size = %d\n", start);
        printAction(start, cache.blockSize, memoryToCache);
        // we need to load in as many words from mem that fit in cache line
        int startAddress = cache.blocks[idxToStore].tag << (blockOffsetSize + setBitsSize);
        // make sure we start loading from right spot
        startAddress += start;
        // reading from mem
        int currWrite = 0;
        for (int idx = 0; idx < cache.blockSize; ++idx) {
            cache.blocks[idxToStore].data[idx] = mem_access(startAddress, currWrite, write_data);
            startAddress += 1;
        }
        // printCache();
        // update idxFound to reflect where data has now been placed
        idxFound = idxToStore;
    }
    // cache hit OR after loading from mem to cache
    // TODO test how I update LRU
    cache.blocks[idxFound].lruLabel = 0;
    for (int idx = 0; idx < numLines; ++idx) {
        if (idx != idxFound) {
            cache.blocks[idx].lruLabel += 1;
        }
    }
    if (write_flag) {
        // write word to cache
        int size = 1;
        // use size of 1 word and address
        printAction(addr, size, processorToCache);
        cache.blocks[idxFound].dirty = 1;
        cache.numDirty += 1;
        cache.blocks[idxFound].valid = 1;
        // NOTE: check what I should return here for "default val"
        cache.blocks[idxFound].data[blockOffset] = mem_access(addr, write_flag, write_data);
        // printCache();
        return 0;
    }
    else {
        // read word from cache-- TODO make sure size correct and idxFound is used 
        // correctly here 
        int size = 1;
        printAction(addr, size, cacheToProcessor);
        // NOTE: need to make sure I'm returning right val here
        // TODO make sure reading in from right word in block
        return mem_access(addr, write_flag, write_data);
    }
    return 0;
}


/*
 * print end of run statistics like in the spec. **This is not required**,
 * but is very helpful in debugging.
 * This should be called once a halt is reached.
 * DO NOT delete this function, or else it won't compile.
 * DO NOT print $$$ in this function
 */
void printStats(void) {
    printf("End of run statistics:\n");
    printf("hits %d, misses %d, writebacks %d\n", cache.numHits, cache.numMisses, cache.numWritebacks);
    printf("%d dirty cache blocks left\n", cache.numDirty);
    return;
}

/*
 * Log the specifics of each cache action.
 *
 *DO NOT modify the content below.
 * address is the starting word address of the range of data being transferred.
 * size is the size of the range of data being transferred.
 * type specifies the source and destination of the data being transferred.
 *  -    cacheToProcessor: reading data from the cache to the processor
 *  -    processorToCache: writing data from the processor to the cache
 *  -    memoryToCache: reading data from the memory to the cache
 *  -    cacheToMemory: evicting cache data and writing it to the memory
 *  -    cacheToNowhere: evicting cache data and throwing it away
 */
void printAction(int address, int size, enum actionType type) {
    printf("$$$ transferring word [%d-%d] ", address, address + size - 1);

    if (type == cacheToProcessor) {
        printf("from the cache to the processor\n");
    }
    else if (type == processorToCache) {
        printf("from the processor to the cache\n");
    }
    else if (type == memoryToCache) {
        printf("from the memory to the cache\n");
    }
    else if (type == cacheToMemory) {
        printf("from the cache to the memory\n");
    }
    else if (type == cacheToNowhere) {
        printf("from the cache to nowhere\n");
    }
    else {
        printf("Error: unrecognized action\n");
        exit(1);
    }

}

/*
 * Prints the cache based on the configurations of the struct
 * This is for debugging only and is not graded, so you may
 * modify it, but that is not recommended.
 */
void printCache(void) {
    int blockIdx;
    int decimalDigitsForWaysInSet = (cache.blocksPerSet == 1) ? 1 : (int)ceil(log10((double)cache.blocksPerSet));
    printf("\ncache:\n");
    for (int set = 0; set < cache.numSets; ++set) {
        printf("\tset %i:\n", set);
        for (int block = 0; block < cache.blocksPerSet; ++block) {
            blockIdx = set * cache.blocksPerSet + block;
            if(cache.blocks[set * cache.blocksPerSet + block].valid) {
                printf("\t\t[ %0*i ] : ( V:T | D:%c | LRU:%-*i | T:%i )\n\t\t%*s{",
                    decimalDigitsForWaysInSet, block,
                    (cache.blocks[blockIdx].dirty) ? 'T' : 'F',
                    decimalDigitsForWaysInSet, cache.blocks[blockIdx].lruLabel,
                    cache.blocks[blockIdx].tag,
                    7+decimalDigitsForWaysInSet, "");
                for (int index = 0; index < cache.blockSize; ++index) {
                    printf(" 0x%08X", cache.blocks[blockIdx].data[index]);
                }
                printf(" }\n");
            }
            else {
                printf("\t\t[ %0*i ] : ( V:F )\n\t\t%*s{     }\n", decimalDigitsForWaysInSet, block, 7+decimalDigitsForWaysInSet, "");
            }
        }
    }
    printf("end cache\n");
}
