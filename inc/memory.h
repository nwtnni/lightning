#ifndef MEMORY_H
#define MEMORY_H

#include <atomic>
#include <semaphore.h>

#define MINIMAL_BLOCK_SIZE_LOG 3
#define MAXIMUM_BLOCK_SIZE_LOG 39
#define HASHMAP_SIZE 65536

#define MAX_NUM_OBJECTS 30000000

typedef int64_t sm_offset;

struct MemoryEntry {
  int64_t free_list_next;
  int64_t in_use;
  sm_offset offset;
  size_t size;
  int64_t prev;
  int64_t next;
  int64_t buddy[40];
};

struct MemoryHeader {
  int64_t index;
};

struct FreeList {
  int64_t free_list_head[40];
};

struct ObjectEntry {
  // 0
  uint64_t object_id;
  // 8
  sm_offset offset;
  // 16
  size_t size;
  // 24
  int64_t ref_count;
  // 32
  int64_t sealed;
  // 40
  int64_t prev;
  // 48
  int64_t next;
  // 56
  int64_t num_waiters;
  // 64
  int64_t free_list_next;
  // 72
  sem_t sem;
} __attribute__((aligned(8)));

struct HashEntry {
  int64_t object_list;
};

struct HashMap {
  HashEntry hash_entries[HASHMAP_SIZE];
};

struct LightningStoreHeader {
  std::atomic<int> lock_flag;
  std::atomic<sm_offset> root;
  MemoryEntry memory_entries[MAX_NUM_OBJECTS];
  ObjectEntry object_entries[MAX_NUM_OBJECTS];
  int64_t memory_entry_free_list;
  int64_t object_entry_free_list;
  FreeList free_list;
  HashMap hashmap;
};

#endif // MEMORY_H
