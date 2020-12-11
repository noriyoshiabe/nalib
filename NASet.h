#pragma once

#include "NAHash.h"
#include "NADescription.h"
#include "NAIterator.h"

#include <stdbool.h>
#include <alloca.h>

typedef struct _NASet NASet;

extern NASet *NASetCreate(NAHash hash, NADescription description);
extern void NASetDestroy(NASet *self);
extern bool NASetContains(NASet *self, void *value);
extern bool NASetAdd(NASet *self, void *value);
extern void *NASetGet(NASet *self, void *value);
extern int NASetCount(NASet *self);
extern bool NASetRemove(NASet *self, void *value);
extern void NASetRemoveAll(NASet *self);
extern void NASetTraverse(NASet *self, void (*function)(void *));
extern NAIterator *NASetGetIterator(NASet *self, void *buffer);
extern void NASetDescription(void *self, FILE *stream);

extern const int NASetIteratorSize;

#define NASetTraverse(self, function) NASetTraverse(self, (void *)function)
#define NASetGetIterator(self) NASetGetIterator(self, alloca(NASetIteratorSize))
