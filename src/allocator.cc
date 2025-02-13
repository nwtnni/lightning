#include <atomic>
#include <ctime>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "allocator.h"
#include "object_log.h"

#define LOCK                                                                   \
  do {                                                                         \
    auto out = 0;                                                              \
    while (!store_header_->lock_flag.compare_exchange_strong(                  \
        out, id, std::memory_order_acq_rel, std::memory_order_acquire)) {      \
      out = 0;                                                                 \
      nanosleep((const struct timespec[]){{0, 0L}}, nullptr);                  \
    }                                                                          \
  } while (0);

#define UNLOCK store_header_->lock_flag.store(0, std::memory_order_release)

LightningAllocator::LightningAllocator(const char *path, size_t size)
    : size_(size) {
  auto fd = shm_open(path, O_CREAT | O_RDWR, 0666);
  int status = ftruncate(fd, size);

  if (status < 0) {
    perror("cannot ftruncate");
    exit(-1);
  }
  store_header_ = (LightningStoreHeader *)mmap(
      nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (store_header_ == (LightningStoreHeader *)-1) {
    perror("mmap failed");
    exit(-1);
  }

  auto pid = getpid();
  auto pid_str = "object-log-" + std::to_string(pid);
  size_t object_log_size = sizeof(LogObjectEntry) * OBJECT_LOG_SIZE;
  auto object_log_fd = shm_open(pid_str.c_str(), O_CREAT | O_RDWR, 0666);

  status = ftruncate(object_log_fd, object_log_size);
  if (status < 0) {
    perror("cannot ftruncate");
    exit(-1);
  }

  auto object_log_base_ =
      (uint8_t *)mmap(nullptr, object_log_size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, object_log_fd, 0);
  if (object_log_base_ == (uint8_t *)-1) {
    perror("mmap failed");
    exit(-1);
  }

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
