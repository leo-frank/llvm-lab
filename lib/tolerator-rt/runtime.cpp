
#include <cstdint>
#include <cstdio>
#include <cstdlib>

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
}

void TOLERATE(free)(int64_t *ptr) { printf("free at %p\n", ptr); }

void TOLERATE(local)(int64_t *ptr, int64_t size) {
  printf("local var alloc at %p, size %ld bytes\n", ptr, size);
}

void TOLERATE(store)(int64_t *ptr, int64_t val, int64_t size) {
  printf("try to write %p with value %ld, size %ld \n", ptr, val, size);
}
}
