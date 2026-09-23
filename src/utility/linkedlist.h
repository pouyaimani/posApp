#ifndef LINKED_LIST_H_
#define LINKED_LIST_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

typedef bool (*ListVisitor)(void* data, void* context);
typedef void (*ListDestructor)(void* data);
typedef bool (*ListPredicate)(void* data, void* context);

#define LIST_INIT(listMode) {NULL, NULL, 0, (listMode)}

typedef enum { LIST_LINEAR, LIST_CIRCULAR } ListMode;

typedef struct ListNode {
    void*            data;
    struct ListNode* next;
} ListNode;

typedef struct {
    ListNode* head;
    ListNode* tail;
    size_t    size;
    ListMode  mode;
} List;

void list_init(List* list, ListMode mode);

/* Append an element. O(1) */
bool list_push_back(List* list, void* data);

/* Prepend an element. O(1) */
bool list_push_front(List* list, void* data);

/*
 * Remove the first element whose data pointer equals data.
 * Frees only the node, not the stored data. O(n)
 */
bool list_remove(List* list, const void* data);

/*
 * Remove every matching node.
 * Returns the number of removed nodes.
 *
 * Frees nodes only; stored data is not freed.
 * The predicate must not modify the list structure.
 */
size_t list_remove_predicate(List* list, ListPredicate predicate,
                             void* context);

/*
 * Remove the first node and return its data. O(1)
 * The caller is responsible for the returned data.
 */
bool list_pop_front(List* list, void** outData);

/*
 * Visit each element once, including in circular mode.
 * Return false from visit to stop early.
 *
 * The callback may modify stored data, but must not add/remove
 * nodes, clear the list, or otherwise modify its structure.
 */
void list_foreach(const List* list, ListVisitor visit, void* context);

/*
 * Remove all nodes, preserving the list mode.
 * Pass NULL to retain data, or a destructor to release it.
 * The destructor must not modify this list.
 */
void list_clear(List* list, ListDestructor destroyData);

#endif