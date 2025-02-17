#include <atomic>
#include <ctime>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "allocator.h"
#include "object_log.h"

// Note: original system uses process IDs, which are non-zero, but the
// allocator benchmark uses thread IDs starting from zero.
#define LOCK                                                                   \
  do {                                                                         \
    auto out = 0;                                                              \
    while (!store_header_->lock_flag.compare_exchange_strong(                  \
        out, id + 1, std::memory_order_acq_rel, std::memory_order_acquire)) {  \
      out = 0;                                                                 \
      nanosleep((const struct timespec[]){{0, 0L}}, nullptr);                  \
    }                                                                          \
  } while (0);

#define UNLOCK store_header_->lock_flag.store(0, std::memory_order_release)

LightningAllocator::LightningAllocator(char *address, size_t size)
    : size_(size) {
  store_header_ = (LightningStoreHeader *)address;

  size_t object_log_size = sizeof(LogObjectEntry) * OBJECT_LOG_SIZE;
  auto disk = new UndoLogDisk(1024 * 1024 * 10, (uint8_t *)store_header_,
                              size + object_log_size);
  allocator_ = new MemAllocator(store_header_, disk);
}

void LightningAllocator::Initialize(uint64_t id) {
  LOCK;

  for (int i = 0; i < MAX_NUM_OBJECTS - 1; i++) {
    store_header_->memory_entries[i].free_list_next = i + 1;
  }
  store_header_->memory_entries[MAX_NUM_OBJECTS - 1].free_list_next = -1;

  for (int i = 0; i < MAX_NUM_OBJECTS - 1; i++) {
    store_header_->object_entries[i].free_list_next = i + 1;
  }
  store_header_->object_entries[MAX_NUM_OBJECTS - 1].free_list_next = -1;

  int num_mpk_pages = sizeof(LightningStoreHeader) / 4096 + 1;
  int64_t secure_memory_size = num_mpk_pages * 4096;

  allocator_->Init(secure_memory_size, size_ - secure_memory_size);

  for (int i = 0; i < HASHMAP_SIZE; i++) {
    store_header_->hashmap.hash_entries[i].object_list = -1;
  }

  UNLOCK;
}

void *LightningAllocator::GetRoot() {
  sm_offset root = store_header_->root.load();
  if (root == 0) {
    return nullptr;
  } else {
    return OffsetToPointer(root);
  }
}

void LightningAllocator::SetRoot(void *pointer) {
  store_header_->root.store(PointerToOffset(pointer));
}

sm_offset LightningAllocator::PointerToOffset(void *pointer) {
  return ((sm_offset)pointer - (sm_offset)store_header_);
}

void *LightningAllocator::OffsetToPointer(sm_offset offset) {
  return (void *)((sm_offset)store_header_ + offset);
}

sm_offset LightningAllocator::Malloc(uint64_t id, size_t size) {
  LOCK;
  allocator_->BeginTx();
  sm_offset pointer = allocator_->MallocShared(size);
  allocator_->CommitTx();
  UNLOCK;
  return pointer;
}

void LightningAllocator::Free(uint64_t id, sm_offset offset) {
  LOCK;
  allocator_->BeginTx();
  allocator_->FreeShared(offset);
  allocator_->CommitTx();
  UNLOCK;
}
