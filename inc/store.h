#ifndef STORE_H
#define STORE_H

#include <mutex>
#include <string>
#include <sys/types.h>
#include <unordered_set>

#include "config.h"
#include "malloc.h"

class LightningStore {
public:
  LightningStore(const std::string &unix_socket, int size);
  LightningStore(const char *path, int size);
  void Run();

  void *GetRoot();
  void SetRoot(void *pointer);
  void *OffsetToPointer(sm_offset offset);
  sm_offset PointerToOffset(void *pointer);
  sm_offset Malloc(uint64_t id, size_t size);
  void Free(uint64_t id, sm_offset offset);

private:
  LightningStore(const char *path, const std::string &unix_socket, int size);
  void monitor();
  void listener();

  void recover(uint8_t *sm, uint8_t *log, uint8_t *object_log, pid_t pid);

  std::string unix_socket_;
  int size_;
  int store_fd_;
  LightningStoreHeader *store_header_;

  std::mutex client_lock_;
  std::unordered_set<pid_t> clients_;
  MemAllocator *allocator_;

  int64_t find_object(uint64_t object_id);
  int release_object(uint64_t object_id);

  int64_t alloc_object_entry();
  void dealloc_object_entry(int64_t object_index);
  int create_object(uint64_t object_id, sm_offset *offset, size_t size);
  int get_object(uint64_t object_id, sm_offset *ptr, size_t *size);
  int seal_object(uint64_t object_id);
  int delete_object(uint64_t object_id);
};

#endif // STORE_H
