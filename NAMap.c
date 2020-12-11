#include "NAMap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#undef NAMapGetKeys
#undef NAMapGetValues
#undef NAMapTraverseKey
#undef NAMapTraverseValue
#undef NAMapGetIterator

typedef struct _Entry {
    void *key;
    void *value;
    struct _Entry *next;
} Entry;

struct _NAMap {
    Entry *entries;
    Entry **buckets;
    Entry *freeList;
    int count;
    int size;
    NAHash keyHash;
    NADescription keyDescription;
    NADescription valueDescription;

    void *(*calloc)(size_t nmemb, size_t size);
    void (*free)(void *ptr);
};

typedef struct _NAMapIterator {
    NAIterator super;
    NAMap *map;
    int bucketIndex;
    Entry *current;
    Entry *next;
} NAMapIterator;

const int NAMapIteratorSize = sizeof(NAMapIterator);

NAMap *NAMapCreate(NAHash keyHash, NADescription keyDescription, NADescription valueDescription)
{
    return NAMapCreateWithAllocator(keyHash, keyDescription, valueDescription, calloc, free);
}

NAMap *NAMapCreateWithAllocator(NAHash keyHash, NADescription keyDescription, NADescription valueDescription,
        void *(*calloc)(size_t nmemb, size_t size), void (*free)(void *ptr))
{
    const int __initialSize = 32;
    
    NAMap *self = (NAMap *)calloc(1, sizeof(NAMap));
    self->entries = (Entry *)calloc(__initialSize, sizeof(Entry));
    self->buckets = (Entry **)calloc(__initialSize, sizeof(Entry *));
    self->size = __initialSize;
    self->count = 0;
    self->keyHash = keyHash ? keyHash : NAHashAddress;
    self->keyDescription = keyDescription ? keyDescription : NADescriptionAddress;
    self->valueDescription = valueDescription ? valueDescription : NADescriptionAddress;

    self->freeList = self->entries;
    for (int i = 0; i < self->size; ++i) {
        self->entries[i].next = i < self->size - 1 ? &self->entries[i + 1] : NULL;
    }

    self->calloc = calloc;
    self->free = free;

    return self;
}

void NAMapDestroy(NAMap *self)
{
    self->free(self->entries);
    self->free(self->buckets);
    self->free(self);
}

bool NAMapContainsKey(NAMap *self, void *key)
{
    uint32_t h = self->keyHash(key);

    Entry *entry = self->buckets[h & (self->size - 1)];
    while (entry != NULL) {
        if (h == self->keyHash(entry->key)) {
            return true;
        }
        entry = entry->next;
    }

    return false;
}

static void __NAMapAdd(NAMap *self, void *key, void *value)
{
    uint32_t h = self->keyHash(key);
    int idx = h & (self->size - 1);

    Entry *entry = self->freeList;
    self->freeList = self->freeList->next;

    entry->next = self->buckets[idx];
    self->buckets[idx] = entry;

    entry->key = key;
    entry->value = value;

    ++self->count;
}

static void __NAMapExtend(NAMap *self)
{
    Entry *oldEntries = self->entries;
    Entry **oldBuckets = self->buckets;
    int oldSize = self->size;

    self->size *= 2;
    self->entries = (Entry *)self->calloc(self->size, sizeof(Entry));
    self->buckets = (Entry **)self->calloc(self->size, sizeof(Entry *));
    self->count = 0;

    self->freeList = self->entries;
    for (int i = 0; i < self->size; ++i) {
        self->entries[i].next = i < self->size - 1 ? &self->entries[i + 1] : NULL;
    }

    for (int i = 0; i < oldSize; ++i) {
        Entry *entry = oldBuckets[i];
        while (NULL != entry) {
            __NAMapAdd(self, entry->key, entry->value);
            entry = entry->next;
        }
    }

    self->free(oldEntries);
    self->free(oldBuckets);
}

void *NAMapPut(NAMap *self, void *key, void *value)
{
    if (NAMapContainsKey(self, key)) {
        NAMapRemove(self, key);
    }

    if (self->count >= self->size) {
        __NAMapExtend(self);
    }

    __NAMapAdd(self, key, value);

    return value;
}

void *NAMapGet(NAMap *self, void *key)
{
    uint32_t h = self->keyHash(key);

    Entry *entry = self->buckets[h & (self->size - 1)];
    while (entry != NULL) {
        if (h == self->keyHash(entry->key)) {
            return entry->value;
        }
        entry = entry->next;
    }

    return NULL;
}

int NAMapCount(NAMap *self)
{
    return self->count;
}

void **NAMapGetKeys(NAMap *self, void *buffer)
{
    void **keys = buffer;
    int idx = 0;

    for (int i = 0; i < self->size; ++i) {
        Entry *entry = self->buckets[i];
        while (entry != NULL) {
            keys[idx++] = entry->key;
            entry = entry->next;
        }
    }

    return keys;
}

void **NAMapGetValues(NAMap *self, void *buffer)
{
    void **values = buffer;
    int idx = 0;

    for (int i = 0; i < self->size; ++i) {
        Entry *entry = self->buckets[i];
        while (entry != NULL) {
            values[idx++] = entry->value;
            entry = entry->next;
        }
    }

    return values;
}

void *NAMapRemove(NAMap *self, void *key)
{
    uint32_t h = self->keyHash(key);
    int idx = h & (self->size - 1);

    Entry *entry = self->buckets[idx];
    Entry *prev = NULL;
    while (entry != NULL) {
        if (h == self->keyHash(entry->key)) {
            if (prev) {
                prev->next = entry->next;
            }
            else {
                self->buckets[idx] = self->buckets[idx]->next;
            }

            entry->next = self->freeList;
            self->freeList = entry;
            --self->count;

            return entry->value;
        }

        prev = entry;
        entry = entry->next;
    }

    return NULL;
}

void NAMapRemoveAll(NAMap *self)
{
    self->count = 0;
    self->freeList = self->entries;
    for (int i = 0; i < self->size; ++i) {
        self->entries[i].next = i < self->size - 1 ? &self->entries[i + 1] : NULL;
        self->buckets[i] = NULL;
    }
}

void NAMapTraverseKey(NAMap *self, void (*function)(void *))
{
    for (int i = 0; i < self->size; ++i) {
        Entry *entry = self->buckets[i];
        while (entry != NULL) {
            function(entry->key);
            entry = entry->next;
        }
    }
}

void NAMapTraverseValue(NAMap *self, void (*function)(void *))
{
    for (int i = 0; i < self->size; ++i) {
        Entry *entry = self->buckets[i];
        while (entry != NULL) {
            function(entry->value);
            entry = entry->next;
        }
    }
}

static bool NAMapIteratorHasNext(NAIterator *_iterator)
{
    NAMapIterator *iterator = (NAMapIterator *)_iterator;
    return NULL != iterator->next;
}

static void *NAMapIteratorNext(NAIterator *_iterator)
{
    NAMapIterator *iterator = (NAMapIterator *)_iterator;
    iterator->current = iterator->next;

    iterator->next = iterator->next->next;
    if (!iterator->next) {
        for (iterator->bucketIndex += 1; iterator->bucketIndex < iterator->map->size; ++iterator->bucketIndex) {
            iterator->next = iterator->map->buckets[iterator->bucketIndex];
            if (iterator->next) {
                break;
            }
        }
    }

    return iterator->current;
}

static void NAMapIteratorRemove(NAIterator *_iterator)
{
    NAMapIterator *iterator = (NAMapIterator *)_iterator;
    if (iterator->current) {
        NAMapRemove(iterator->map, iterator->current->key);
    }
}

NAIterator *NAMapGetIterator(NAMap *self, void *buffer)
{
    NAMapIterator *iterator = buffer;
    iterator->super.hasNext = NAMapIteratorHasNext;
    iterator->super.next = NAMapIteratorNext;
    iterator->super.remove = NAMapIteratorRemove;
    iterator->map = self;
    iterator->current = NULL;

    for (iterator->bucketIndex = 0; iterator->bucketIndex < self->size; ++iterator->bucketIndex) {
        iterator->next = self->buckets[iterator->bucketIndex];
        if (iterator->next) {
            break;
        }
    }

    return (NAIterator *)iterator;
}

void NAMapDescription(void *_self, FILE *stream)
{
    NAMap *self = _self;
    fprintf(stream, "<NAMap 0x%X\n", (uint32_t)self);

    for (int i = 0; i < self->size; ++i) {
        Entry *entry = self->buckets[i];
        int chain = 0;
        while (entry != NULL) {
            fprintf(stream, "  [%d:%d] ", i, chain);
            self->keyDescription(entry->key, stream);
            fprintf(stream, ": ");
            self->valueDescription(entry->value, stream);
            fprintf(stream, "\n");
            entry = entry->next;
            ++chain;
        }
    }

    fprintf(stream, ">\n");
}
