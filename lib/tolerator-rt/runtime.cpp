
#include <cstdint>
#include <cstdio>
#include <cstdlib>
<<<<<<< HEAD
#include <vector>

std::vector<int64_t> mallocAddrs;
struct validLocalPair {
  int64_t id; // function ID
  int64_t ptr;
  int64_t size;
};
std::vector<validLocalPair> validLocals;
=======
>>>>>>> parent of c4f54fd... finish Task1 - Invalid Free Detect

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
<<<<<<< HEAD
  mallocAddrs.push_back((int64_t)ptr);
=======
  printf("malloc at %p size %ld\n", ptr, size);
>>>>>>> parent of c4f54fd... finish Task1 - Invalid Free Detect
}

void TOLERATE(free)(int64_t *ptr) { printf("free at %p\n", ptr); }

void TOLERATE(local)(int64_t id, int64_t *ptr, int64_t size) {
  validLocals.push_back(validLocalPair{id, (int64_t)ptr, size});
}

void TOLERATE(store)(int64_t id, int64_t *ptr, int64_t val, int64_t size) {
  for (auto i = validLocals.begin(); i < validLocals.end(); i++) {
    if (i->id == id || i->id == -1) {
      if (((int64_t)ptr < i->ptr + i->size) && ((int64_t)ptr >= i->ptr)) {
        return;
      }
    }
  }
  fprintf(stderr, "FOUND: Invalid write of memory\n");
  exit(-1);
}

void TOLERATE(load)(int64_t id, int64_t *ptr, int64_t size) {
  for (auto i = validLocals.begin(); i < validLocals.end(); i++) {
    if (i->id == id || i->id == -1) {
      if (((int64_t)ptr < i->ptr + i->size) && ((int64_t)ptr >= i->ptr)) {
        return;
      }
    }
  }
  fprintf(stderr, "FOUND: Invalid read of memory\n");
  exit(-1);
}

void TOLERATE(clear)(int64_t id) {
  for (auto i = validLocals.begin(); i < validLocals.end(); i++) {
    if (i->id == id) {
      validLocals.erase(i);
    }
  }
}
}
