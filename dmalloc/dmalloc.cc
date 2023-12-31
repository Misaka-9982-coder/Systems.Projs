#define DMALLOC_DISABLE 1
#define SECRET_VALUE 0xDEADBEEF
#include "dmalloc.hh"
#include <cassert>
#include <cstring>
#include <unordered_map>
#include <string>

static dmalloc_stats global_stats;
static bool dmalloc_status;
static int valid_free;
static uintptr_t malloc_address;

struct MallocInfo {
    std::string file;
    long line;
    size_t size;
};

static std::unordered_map<void*, MallocInfo> malloc_map;

/**
 * dmalloc(sz,file,line)
 *      malloc() wrapper. Dynamically allocate the requested amount `sz` of memory and 
 *      return a pointer to it 
 * 
 * @arg size_t sz : the amount of memory requested 
 * @arg const char *file : a string containing the filename from which dmalloc was called 
 * @arg long line : the line number from which dmalloc was called 
 * 
 * @return a pointer to the heap where the memory was reserved
 */
void* dmalloc(size_t sz, const char* file, long line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    // Your code here.
    if (sz > SIZE_MAX - sizeof(size_t)) {
        global_stats.nfail++;
        global_stats.fail_size += sz;
        return NULL;
    }

    size_t total_size = sz + sizeof(size_t) + sizeof(SECRET_VALUE);
    size_t* ptr = (size_t*) base_malloc(total_size);

    if (ptr) {
        *ptr = sz;

        *(uint32_t*)((char*)ptr + sz + sizeof(size_t)) = SECRET_VALUE;
        malloc_map[ptr] = {file, line, sz};

        global_stats.nactive ++ ;
        global_stats.ntotal ++ ;
        global_stats.active_size += sz;
        global_stats.total_size += sz;

        malloc_address += (uintptr_t) ptr + sizeof(size_t);

        uintptr_t block_start = (uintptr_t) ptr;
        uintptr_t block_end = block_start + total_size;
        if (!global_stats.heap_min || block_start < global_stats.heap_min) {
            global_stats.heap_min = block_start;
        }
        if (!global_stats.heap_max || block_end > global_stats.heap_max) {
            global_stats.heap_max = block_end;
        }

        dmalloc_status = 1;
        valid_free ++ ;

        return ptr + 1;
    } else {
        global_stats.nfail++;
        global_stats.fail_size += sz;
        return nullptr;
    }
}

/**
 * dfree(ptr, file, line)
 *      free() wrapper. Release the block of heap memory pointed to by `ptr`. This should 
 *      be a pointer that was previously allocated on the heap. If `ptr` is a nullptr do nothing. 
 * 
 * @arg void *ptr : a pointer to the heap 
 * @arg const char *file : a string containing the filename from which dfree was called 
 * @arg long line : the line number from which dfree was called 
 */
void dfree(void* ptr, const char* file, long line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    // Your code here.
    if (!ptr) {
        return;
    }

    if ((dmalloc_status == 0) || (ptr < (void*)global_stats.heap_min)) {
        fprintf(stderr, "MEMORY BUG???: invalid free of pointer %p, not in heap\n", ptr);
        abort();
    }

    if (valid_free <= 0) {
        fprintf(stderr,"MEMORY BUG???: invalid free of pointer %p, double free\n", ptr);
        abort();
    }

    auto iter = malloc_map.find((size_t*) ptr - 1);
    if (iter == malloc_map.end()) {
        fprintf(stderr, "MEMORY BUG: %s:%ld: invalid free of pointer %p, not allocated\n", file, line, ptr);
        for (const auto& pair : malloc_map) {
            char* start = static_cast<char*>(pair.first) + sizeof(size_t);
            char* end = start + pair.second.size;
            if (ptr >= start && ptr < end) {
                fprintf(stderr, "%s:%ld: %p is %ld bytes inside a %ld byte region allocated here\n",
                        pair.second.file.c_str(), pair.second.line, ptr,
                        static_cast<char*>(ptr) - start, pair.second.size);
                break;
            }
        }
        abort();
    }
    
    size_t* true_ptr = ((size_t*) ptr) - 1;
    size_t sz = *true_ptr;

    if (*(uint32_t*)((char*)true_ptr + sz + sizeof(size_t)) != SECRET_VALUE) {
        fprintf(stderr,"MEMORY BUG???: detected wild write during free of pointer %p\n", ptr);
        abort();
    }

    global_stats.nactive -- ;
    global_stats.active_size -= sz;
    valid_free -- ;
    base_free(true_ptr);

    auto it = malloc_map.find(true_ptr);
    if (it != malloc_map.end()) {
        malloc_map.erase(it);
    }
}

/**
 * dcalloc(nmemb, sz, file, line)
 *      calloc() wrapper. Dynamically allocate enough memory to store an array of `nmemb` 
 *      number of elements with wach element being `sz` bytes. The memory should be initialized 
 *      to zero  
 * 
 * @arg size_t nmemb : the number of items that space is requested for
 * @arg size_t sz : the size in bytes of the items that space is requested for
 * @arg const char *file : a string containing the filename from which dcalloc was called 
 * @arg long line : the line number from which dcalloc was called 
 * 
 * @return a pointer to the heap where the memory was reserved
 */
void* dcalloc(size_t nmemb, size_t sz, const char* file, long line) {
    // Your code here (to fix test014).
    if (nmemb != 0 && sz > SIZE_MAX / nmemb) {
        global_stats.nfail ++ ;
        global_stats.fail_size += sz;
        return NULL;
    }

    void* ptr = dmalloc(nmemb * sz, file, line);
    if (ptr) {
        memset(ptr, 0, nmemb * sz);
    }
    return ptr;
}

/**
 * get_statistics(stats)
 *      fill a dmalloc_stats pointer with the current memory statistics  
 * 
 * @arg dmalloc_stats *stats : a pointer to the the dmalloc_stats struct we want to fill
 */
void get_statistics(dmalloc_stats* stats) {
    // Stub: set all statistics to enormous numbers
    memset(stats, 255, sizeof(dmalloc_stats));
    // Your code here.
    memcpy(stats, &global_stats, sizeof(dmalloc_stats));
}

/**
 * print_statistics()
 *      print the current memory statistics to stdout       
 */
void print_statistics() {
    dmalloc_stats stats;
    get_statistics(&stats);

    printf("alloc count: active %10llu   total %10llu   fail %10llu\n",
           stats.nactive, stats.ntotal, stats.nfail);
    printf("alloc size:  active %10llu   total %10llu   fail %10llu\n",
           stats.active_size, stats.total_size, stats.fail_size);
}

/**  
 * print_leak_report()
 *      Print a report of all currently-active allocated blocks of dynamic
 *      memory.
 */
void print_leak_report() {
    // Your code here.
    for (const auto& pair : malloc_map) {
        const auto& info = pair.second;
        char* ptr = static_cast<char*>(pair.first);
        printf("LEAK CHECK: %s:%ld: allocated object %p with size %ld\n",
               info.file.c_str(), info.line, ptr + sizeof(size_t), info.size);
    }
}
