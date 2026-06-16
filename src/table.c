#include <stdlib.h>
#include <string.h>

#include "table.h"
#include "memory.h"
#include "object.h"
#include "gc.h"
#include "vm.h"

#define TABLE_MAX_LOAD 0.75

void table_init(Table* table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void table_free(Table* table) {
    FREE_ARRAY(TableEntry, table->entries, table->capacity);
    table_init(table);
}

static TableEntry* find_entry(TableEntry* entries, int capacity,
                              ObjString* key) {
    uint32_t index = key->hash % capacity;
    TableEntry* tombstone = NULL;

    for (;;) {
        TableEntry* entry = &entries[index];
        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) {
                return tombstone != NULL ? tombstone : entry;
            } else {
                if (tombstone == NULL) tombstone = entry;
            }
        } else if (entry->key == key) {
            return entry;
        }
        index = (index + 1) % capacity;
    }
}

bool table_get(Table* table, ObjString* key, Value* value) {
    if (table->count == 0) return false;

    TableEntry* entry = find_entry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    *value = entry->value;
    return true;
}

static void adjust_capacity(Table* table, int capacity) {
    TableEntry* entries = ALLOCATE(TableEntry, capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        TableEntry* entry = &table->entries[i];
        if (entry->key == NULL) continue;

        TableEntry* dest = find_entry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    FREE_ARRAY(TableEntry, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}

bool table_set(Table* table, ObjString* key, Value value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = MJ_GROW_CAPACITY(table->capacity);
        adjust_capacity(table, capacity);
    }

    TableEntry* entry = find_entry(table->entries, table->capacity, key);
    bool is_new = entry->key == NULL;
    if (is_new && IS_NIL(entry->value)) table->count++;

    entry->key = key;
    entry->value = value;
    return is_new;
}

bool table_delete(Table* table, ObjString* key) {
    if (table->count == 0) return false;

    TableEntry* entry = find_entry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    entry->key = NULL;
    entry->value = BOOL_VAL(true);
    return true;
}

void table_add_all(Table* from, Table* to) {
    for (int i = 0; i < from->capacity; i++) {
        TableEntry* entry = &from->entries[i];
        if (entry->key != NULL) {
            table_set(to, entry->key, entry->value);
        }
    }
}

ObjString* table_find_string(Table* table, const char* chars,
                             int length, uint32_t hash) {
    if (table->count == 0) return NULL;

    uint32_t index = hash % table->capacity;
    for (;;) {
        TableEntry* entry = &table->entries[index];
        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) return NULL;
        } else if (entry->key->length == length &&
                   entry->key->hash == hash &&
                   memcmp(entry->key->chars, chars, length) == 0) {
            return entry->key;
        }
        index = (index + 1) % table->capacity;
    }
}

void table_remove_white(Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        TableEntry* entry = &table->entries[i];
        if (entry->key != NULL && !entry->key->obj.is_marked) {
            table_delete(table, entry->key);
        }
    }
}

