
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

std::vector<int64_t> mallocAddrs;

extern "C" {

// This macro allows us to prefix strings so that they are less likely to
// conflict with existing symbol names in the examined programs.
// e.g. TOLERATE(entry) yields ToLeRaToR_entry
#define TOLERATE(X) ToLeRaToR_##X

void TOLERATE(helloworld)() {
  printf("==============================\n"
         "\tHello, World!\n"
         "==============================\n");
}

void TOLERATE(goodbyeworld)() {
  printf("==============================\n"
         "\tGoodbye, World!\n"
         "==============================\n");
}

void TOLERATE(div)(int32_t op) {
  if (op == 0) {
    fprintf(stderr, "FOUND: Division by zero\n");
    exit(-1);
  }
}

void TOLERATE(malloc)(int64_t *ptr, int64_t size) {
  printf("malloc at %p size %ld\n", ptr, size);
  mallocAddrs.push_back((int64_t)ptr);
}

void TOLERATE(free)(int64_t *ptr) {
  auto iter = std::find(begin(mallocAddrs), end(mallocAddrs), (int64_t)ptr);
  if (iter == std::end(mallocAddrs)) {
    fprintf(stderr, "FOUND: Invalid free of memory\n");
    exit(-1);
  } else {
    mallocAddrs.erase(iter);
  }
}

void TOLERATE(local)(int64_t *ptr, int64_t size) {
  printf("local var alloc at %p, size %ld bytes\n", ptr, size);
}

void TOLERATE(store)(int64_t *ptr, int64_t val, int64_t size) {
  printf("try to write %p with value %ld, size %ld \n", ptr, val, size);
}

void TOLERATE(load)(int64_t *ptr, int64_t size) {
  printf("load from %p, size %ld bytes\n", ptr, size);
}
}
