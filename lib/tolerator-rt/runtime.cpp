
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

std::vector<int64_t> mallocAddrs;
struct validLocalPair {
  int64_t id; // function ID
  int64_t ptr;
  int64_t size;
};
std::vector<validLocalPair> validLocals;

enum AnalysisType { LOGGING = 1, IGNORING, DEFAULTING, BYPASSING };
enum STATE { INVALID, VALID };
char Mode;

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
    switch (Mode) {
    case LOGGING:
    case IGNORING: {
      fprintf(stderr, "FOUND: Division by zero\n");
      exit(-1);
    }
    default: {
      fprintf(stderr, "Not implemented\n");
      exit(-1);
    }
    }
  }
}

void TOLERATE(malloc)(int64_t *ptr, int64_t size) {
  mallocAddrs.push_back((int64_t)ptr);
}

int TOLERATE(free)(int64_t *ptr) {
  auto iter = std::find(begin(mallocAddrs), end(mallocAddrs), (int64_t)ptr);
  if (iter == std::end(mallocAddrs)) {
    switch (Mode) {
    case LOGGING: {
      fprintf(stderr, "FOUND: Invalid free of memory\n");
      exit(-1);
    }
    case IGNORING: {
      fprintf(stderr, "FOUND: Invalid free of memory\n");
      return INVALID;
    }
    default: {
      fprintf(stderr, "Not implemented\n");
      exit(-1);
    }
    }
  } else {
    mallocAddrs.erase(iter);
    return VALID;
  }
}

void TOLERATE(local)(int64_t id, int64_t *ptr, int64_t size) {
  validLocals.push_back(validLocalPair{id, (int64_t)ptr, size});
}

int TOLERATE(store)(int64_t id, int64_t *ptr, int64_t val, int64_t size) {
  /* FIXME: Bug here. forget GLOBAL STORE. */
  for (auto i = validLocals.begin(); i < validLocals.end(); i++) {
    if (i->id == id || i->id == -1) {
      if (((int64_t)ptr < i->ptr + i->size) && ((int64_t)ptr >= i->ptr)) {
        return VALID;
      }
    }
  }
  switch (Mode) {
  case LOGGING: {
    fprintf(stderr, "FOUND: Invalid write to memory\n");
    exit(-1);
  }
  case IGNORING: {
    fprintf(stderr, "FOUND: Invalid write to memory\n");
    return INVALID;
  }
  default: {
    fprintf(stderr, "Not implemented\n");
    exit(-1);
  }
  }
}

void TOLERATE(load)(int64_t id, int64_t *ptr, int64_t size) {
  for (auto i = validLocals.begin(); i < validLocals.end(); i++) {
    if (i->id == id || i->id == -1) {
      if (((int64_t)ptr < i->ptr + i->size) && ((int64_t)ptr >= i->ptr)) {
        return;
      }
    }
  }

  switch (Mode) {
  case LOGGING: {
    fprintf(stderr, "FOUND: Invalid read from memory\n");
    exit(-1);
  }
  case IGNORING: {
    fprintf(stderr, "FOUND: Invalid read from memory\n");
  }
  default: {
    fprintf(stderr, "Not implemented\n");
    exit(-1);
  }
  }
}

void TOLERATE(clear)(int64_t id) {
  for (auto i = validLocals.begin(); i < validLocals.end(); i++) {
    if (i->id == id) {
      validLocals.erase(i);
    }
  }
}

void TOLERATE(setMode)(char id) { Mode = id; }
}
