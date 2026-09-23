#include "linkedlist.h"
#include "sys/sys.h"

void list_init(List* list, ListMode mode) {
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
    list->head = mode;
}

/* Restore the appropriate end link after a modification. */
static void list_update_tail(List* list) {
    if (list->tail != NULL) {
        list->tail->next = list->mode == LIST_CIRCULAR ? list->head : NULL;
    }
}

/* Append an element. O(1) */
bool list_push_back(List* list, void* data) {
    if (list == NULL)
        return false;

    ListNode* node = MEM_ALLOC(sizeof(*node));
    if (node == NULL)
        return false;

    node->data = data;
    node->next = NULL;

    if (list->tail != NULL)
        list->tail->next = node;
    else
        list->head = node;

    list->tail = node;
    list->size++;

    list_update_tail(list);
    return true;
}

/* Prepend an element. O(1) */
bool list_push_front(List* list, void* data) {
    if (list == NULL)
        return false;

    ListNode* node = MEM_ALLOC(sizeof(*node));
    if (node == NULL)
        return false;

    node->data = data;
    node->next = list->head;

    list->head = node;

    if (list->tail == NULL)
        list->tail = node;

    list->size++;

    list_update_tail(list);
    return true;
}

/*
 * Remove the first element whose data pointer equals data.
 * Frees only the node, not the stored data. O(n)
 */
bool list_remove(List* list, const void* data) {
    if (list == NULL)
        return false;

    ListNode* previous = NULL;
    ListNode* node     = list->head;

    for (size_t i = 0; i < list->size; ++i) {
        if (node->data == data) {
            if (previous != NULL)
                previous->next = node->next;
            else
                list->head = node->next;

            if (node == list->tail)
                list->tail = previous;

            list->size--;

            if (list->size == 0) {
                list->head = NULL;
                list->tail = NULL;
            }

            list_update_tail(list);
            MEM_FREE(node);
            return true;
        }

        previous = node;
        node     = node->next;
    }

    return false;
}

/*
 * Remove the first node and return its data. O(1)
 * The caller is responsible for the returned data.
 */
bool list_pop_front(List* list, void** outData) {
    if (list == NULL || outData == NULL)
        return false;

    *outData = NULL;

    if (list->head == NULL)
        return false;

    ListNode* node = list->head;
    *outData       = node->data;

    list->head = node->next;
    list->size--;

    if (list->size == 0) {
        list->head = NULL;
        list->tail = NULL;
    }

    list_update_tail(list);
    MEM_FREE(node);
    return true;
}

/*
 * Visit each element once, including in circular mode.
 * Return false from visit to stop early.
 *
 * The callback may modify stored data, but must not add/remove
 * nodes, clear the list, or otherwise modify its structure.
 */
void list_foreach(const List* list, ListVisitor visit, void* context) {
    if (list == NULL || visit == NULL)
        return;

    const ListNode* node = list->head;

    for (size_t i = 0; i < list->size; ++i) {
        if (!visit(node->data, context))
            break;

        node = node->next;
    }
}

/*
 * Remove all nodes, preserving the list mode.
 * Pass NULL to retain data, or a destructor to release it.
 * The destructor must not modify this list.
 */
void list_clear(List* list, ListDestructor destroyData) {
    if (list == NULL)
        return;

    void* data;

    while (list_pop_front(list, &data)) {
        if (destroyData != NULL)
            destroyData(data);
    }
}

/*
 * Remove every matching node.
 * Returns the number of removed nodes.
 *
 * Frees nodes only; stored data is not freed.
 * The predicate must not modify the list structure.
 */
size_t list_remove_predicate(List* list, ListPredicate predicate,
                             void* context) {
    if (list == NULL || predicate == NULL)
        return 0;

    ListNode* previous = NULL;
    ListNode* node     = list->head;

    size_t remaining = list->size;
    size_t removed   = 0;

    /* Visit each original node once, including in circular mode. */
    while (remaining > 0) {
        ListNode* next = node->next;
        remaining--;

        if (predicate(node->data, context)) {
            if (previous != NULL)
                previous->next = next;
            else
                list->head = next;

            if (node == list->tail)
                list->tail = previous;

            list->size--;

            if (list->size == 0) {
                list->head = NULL;
                list->tail = NULL;
            }

            list_update_tail(list);
            free(node);
            removed++;

            if (list->size == 0)
                break;
        } else {
            previous = node;
        }

        node = next;
    }

    return removed;
}