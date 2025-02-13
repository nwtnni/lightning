#include "malloc.h"
#include "memory.h"
class LightningAllocator {
private:
  LightningStoreHeader *store_header_;
  MemAllocator *allocator_;
  size_t size_;

public:
  LightningAllocator(const char *path, size_t size);

  void Initialize(uint64_t id);
  void *GetRoot();
  void SetRoot(void *pointer);
  sm_offset PointerToOffset(void *pointer);
  void *OffsetToPointer(sm_offset offset);
  sm_offset Malloc(uint64_t id, size_t size);
  void Free(uint64_t id, sm_offset offset);
};
