//-----------------------------------------------------------------------------
// List.c
// Implementation file for List ADT
//
// Name:       Quan Do
// ID:         11009023
//-----------------------------------------------------------------------------

#include "List.h"

#include <stdio.h>
#include <stdlib.h>

// structs --------------------------------------------------------------------
// private NodeObj type "Nodeobj refers to this struct w/ data/next/prev"
typedef struct NodeObj {
    int data;
    struct NodeObj* next;
    struct NodeObj* prev;
} NodeObj;

// private Node type "node refers to a Nodeobj pointer"
typedef NodeObj* Node;

// private ListObj type
typedef struct ListObj {
    int length;
    // int index;
    Node front;
    Node back;
    Node cursor;
} ListObj;

// Constructors-Destructors ---------------------------------------------------
// newNode()
// Returns reference to new Node object. Initializes next and data fields.
// Private.
Node newNode(int data) {
    Node N = malloc(sizeof(NodeObj));
    N->data = data;
    N->next = NULL;
    N->prev = NULL;
    return N;
}

// freeNode()
// Frees heap memory pointed to by *pN, sets *pN to NULL.
// Private.
void freeNode(Node* pN) {
    if (pN != NULL && *pN != NULL) {
        free(*pN);
        *pN = NULL;
    }
}

// newList()
// Creates and returns a new empty List.
List newList(void) {
    List L = malloc(sizeof(ListObj));
    L->length = 0;
    // L->index = -1;
    L->front = NULL;
    L->back = NULL;
    L->cursor = NULL;
    return (L);
}

// freeList()
// Frees all heap memory associated with *pL, and sets *pL to NULL.
void freeList(List* pL) {
    if (pL != NULL && *pL != NULL) {
        while (length(*pL) > 0) deleteFront(*pL);
        free(*pL);
        *pL = NULL;
    }
}

// Access functions -----------------------------------------------------------
// length()
// Returns the number of elements in L.
int length(List L) {
    if (L == NULL) {
        printf("List Error: calling length() on NULL List reference\n");
        EXIT_FAILURE;
    }
    return (L->length);
}

// index()
// Returns index of cursor element if defined, -1 otherwise.
// int index(List L) {
//     if (L == NULL) {
//         printf("List Error: calling index() on NULL List reference\n");
//         EXIT_FAILURE;
//     }
//     if (L->cursor != NULL)
//         return (L->index);
//     else
//         return (-1);
// }

// front()
// Returns front element of L. Pre: length()>0
int front(List L) {
    if (L == NULL) {
        printf("List Error: calling front() on NULL List reference\n");
        EXIT_FAILURE;
    }
    if (length(L) == 0) {
        printf("List Error: calling front() on on an empty List \n");
        EXIT_FAILURE;
    }
    return (L->front->data);
}

// back()
// Returns back element of L. Pre: length()>0
int back(List L) {
    if (L == NULL) {
        printf("List Error: calling back() on NULL List reference\n");
        EXIT_FAILURE;
    }
    if (length(L) == 0) {
        printf("List Error: calling back() on on an empty List \n");
        EXIT_FAILURE;
    }
    return (L->back->data);
}

// get()
// Returns cursor element of L. Pre: length()>0, index()>=0
int get(List L) {
    if (L == NULL) {
        printf("List Error: calling get() on NULL List reference\n");
        EXIT_FAILURE;
    }
    if (length(L) == 0) {
        printf("List Error: calling get() on on an empty List \n");
        EXIT_FAILURE;
    }
    // if (L->index < 0) {
    //     printf("List Error: calling get() with undefined cursor\n");
    //     EXIT_FAILURE;
    // }
    return (L->cursor->data);
}

// equals()
// Returns true (1) iff Lists A and B are in same state, and returns false (0)
// otherwise.
int equals(List A, List B) {
    if (A == NULL || B == NULL) {
        printf("List Error: calling equals() on NULL List reference\n");
        EXIT_FAILURE;
    }

    int equal = 0;
    Node N = A->front;
    Node M = B->front;
    // early exit conditions: unequal length or mismatched cursors
    equal = (A->length == B->length);
    // loop to compare otherwise
    while (equal && N != NULL) {
        equal = (N->data == M->data);
        N = N->next;
        M = M->next;
    }
    return equal;
}

// Manipulation procedures ----------------------------------------------------
// clear()
// Resets L to its original empty state.
void clear(List L) {
    while (L->length > 0)
        // will eventually set index to -1
        deleteFront(L);

    L->front = NULL;
    L->back = NULL;
    L->cursor = NULL;
}
/*
// moveFront()
// If L is non-empty, sets cursor under the front element,
// otherwise does nothing.
void moveFront(List L) {
    if (L == NULL) {
        printf("List Error: calling moveFront() on NULL List reference\n");
        EXIT_FAILURE;
    }
    if (length(L) == 0) {
        printf("List Error: calling moveFront() on on an empty List \n");
        EXIT_FAILURE;
    }
    L->cursor = L->front;
    L->index = 0;
}

// moveBack()
// If L is non-empty, sets cursor under the back element,
// otherwise does nothing.
void moveBack(List L) {
    if (L == NULL) {
        printf("List Error: calling moveback() on NULL List reference\n");
        EXIT_FAILURE;
    }
    if (length(L) == 0) {
        printf("List Error: calling moveBack() on on an empty List \n");
        EXIT_FAILURE;
    }
    L->cursor = L->back;
    L->index = L->length - 1;
}

// ()movePrev
// If cursor is defined and not at front, move cursor one
// step toward the front of L; if cursor is defined and at
// front, cursor becomes undefined; if cursor is undefined
// do nothing
void movePrev(List L) {
    if (L == NULL) {
        printf("List Error: calling movePrev() on NULL List reference\n");
        EXIT_FAILURE;
    }

    if (L->cursor != NULL && L->cursor != L->front) {
        L->cursor = L->cursor->prev;
        L->index--;
    } else if (L->cursor == L->front) {
        L->cursor = NULL;
        L->index = -1;
    }
}

// moveNext()
// If cursor is defined and not at back, move cursor one
// step toward the back of L; if cursor is defined and at
// back, cursor becomes undefined; if cursor is undefined
// do nothing
void moveNext(List L) {
    if (L == NULL) {
        printf("List Error: calling moveNext() on NULL List reference\n");
        EXIT_FAILURE;
    }

    if (L->cursor != NULL && L->cursor != L->back) {
        L->cursor = L->cursor->next;
        L->index++;
    } else if (L->cursor == L->back) {
        L->cursor = NULL;
        L->index = -1;
    }
}
*/
// prepend()
// Insert new element into L. If L is non-empty,
// insertion takes place before front element.
void prepend(List L, int data) {
    if (L == NULL) {
        printf("List Error: calling prepend() on NULL List reference\n");
        EXIT_FAILURE;
    }

    Node temp = newNode(data);
    if (length(L) == 0)
        L->front = L->back = temp;
    else {
        L->front->prev = temp;
        temp->next = L->front;
        L->front = temp;
        temp->prev = NULL;
    }
    // increment index IFF cursor exists
    // if (L->cursor != NULL) L->index++;
    L->length++;
}

// append()
// Insert new element into L. If L is non-empty,
// insertion takes place after back element.
void append(List L, int data) {
    if (L == NULL) {
        printf("List Error: calling append() on NULL List reference\n");
        EXIT_FAILURE;
    }

    Node temp = newNode(data);
    if (length(L) == 0)
        L->front = L->back = temp;
    else {
        L->back->next = temp;
        temp->prev = L->back;
        L->back = temp;
        temp->next = NULL;
    }
    L->length++;
}

// insertBefore()
// Insert new element before cursor.
// Pre: length()>0, index()>=0
void insertBefore(List L, int data) {
    if (L == NULL) {
        printf("List Error: calling insertBefore() on NULL List reference\n");
        EXIT_FAILURE;
    }
    if (length(L) == 0) {
        printf("List Error: calling insertBefore() on on an empty List \n");
        EXIT_FAILURE;
    }
    // if (L->index < 0) {
    //     printf(
    //         "List Error: calling insertBefore() with an undefined
    //         cursor\n\n");
    //     EXIT_FAILURE;
    // }

    if (L->cursor == L->front)
        prepend(L, data);
    else {
        Node temp = newNode(data);
        L->cursor->prev->next = temp;
        temp->next = L->cursor;
        temp->prev = L->cursor->prev;
        L->cursor->prev = temp;

        L->length++;
        // if (L->cursor != NULL) L->index++;
    }
}

// insertAfter()
// Insert new element after cursor.
// Pre: length()>0, index()>=0
void insertAfter(List L, int data) {
    if (L == NULL) {
        printf("List Error: calling insertBefore() on NULL List reference\n");
        EXIT_FAILURE;
    }
    if (length(L) == 0) {
        printf("List Error: calling insertBefore() on on an empty List \n");
        EXIT_FAILURE;
    }
    // if (L->index < 0) {
    //     printf("List Error: calling insertAfter() with an undefined
    //     cursor\n"); EXIT_FAILURE;
    // }

    if (L->cursor == L->back)
        append(L, data);
    else {
        Node temp = newNode(data);
        L->cursor->next->prev = temp;
        temp->prev = L->cursor;
        temp->next = L->cursor->next;
        L->cursor->next = temp;

        L->length++;
    }
}

// deleteFront()
// Delete the front element. Pre: length()>0
void deleteFront(List L) {
    if (length(L) == 0) {
        printf("List Error: calling deleteFront() on on an empty List \n");
        EXIT_FAILURE;
    }
    if (L == NULL) {
        printf("List Error: calling deleteFront() on NULL List reference\n");
        EXIT_FAILURE;
    }

    Node temp = L->front;
    if (temp == L->cursor) {
        L->cursor = NULL;
        // L->index = -1;
    }

    if (L->length > 1) {
        L->front = L->front->next;
        L->front->prev = NULL;
    } else  // one element list
        L->front = L->back = NULL;

    // if (L->cursor != NULL) L->index--;

    L->length--;
    freeNode(&temp);
}

// deleteBack()
// Delete the back element. Pre: length()>0
void deleteBack(List L) {
    if (length(L) == 0) {
        printf("List Error: calling deleteBack() on on an empty List \n");
        EXIT_FAILURE;
    }
    if (L == NULL) {
        printf("List Error: calling deleteBack() on NULL List reference\n");
        EXIT_FAILURE;
    }

    Node temp = L->back;
    if (temp == L->cursor) {
        L->cursor = NULL;
        // L->index = -1;
    }

    if (L->length > 1) {
        L->back = L->back->prev;
        L->back->next = NULL;
    } else
        L->front = L->back = NULL;

    L->length--;
    freeNode(&temp);
}

// delete()
// Delete cursor element, making cursor undefined.
// Pre: length()>0, index()>=0
void delete (List L) {
    if (length(L) == 0) {
        printf("List Error: calling delete() on on an empty List \n");
        EXIT_FAILURE;
    }
    // if (L->index < 0) {
    //     printf("List Error: calling delete() with an undefined cursor\n");
    //     EXIT_FAILURE;
    // }
    // DELETION
    Node temp = L->cursor;
    // cursor = front/back || 1 element list
    if (temp == L->front)
        deleteFront(L);
    else if (temp == L->back)
        deleteBack(L);
    else {
        // Cursor is within the middle
        temp->prev->next = temp->next;
        temp->next->prev = temp->prev;
        L->cursor = NULL;
        // L->index = -1;
        L->length--;
        freeNode(&temp);
    }
}

// Other operations -----------------------------------------------------------

// printList()
// Prints to the file pointed to by out, a
// string representation of L consisting
// of a space separated sequence of integers,
// with front on left.
void printList(FILE* out, List L) {
    if (length(L) == 0) {
        printf("List Error: calling printList() on on an empty List \n");
        EXIT_FAILURE;
    }
    if (L == NULL) {
        printf("List Error: calling printList() on NULL List reference\n\n");
        EXIT_FAILURE;
    }

    Node temp = NULL;
    for (temp = L->front; temp != NULL; temp = temp->next)
        fprintf(out, "%d ", temp->data);
}

/*

// copyList()
// Returns a new List representing the same integer
// sequence as L. The cursor in the new list is undefined,
// regardless of the state of the cursor in L. The state
// of L is unchanged.
List copyList(List L) {
    if (length(L) == 0) {
        printf("List Error: calling copyList() on on an empty List \n");
        EXIT_FAILURE;
    }
    if (L == NULL) {
        printf("List Error: calling copyList() on NULL List reference\n");
        EXIT_FAILURE;
    }

    // create a new list
    List newL = newList();
    // copy elements from L
    Node temp = NULL;
    for (temp = L->front; temp != NULL; temp = temp->next)
        append(newL, temp->data);
    return (newL);
}

// concatList
// Returns a new List which is the concatenation of
// A and B. The cursor in the new List is undefined,
// regardless of the states of the cursors in A and B.
// The states of A and B are unchanged.
List concatList(List A, List B) {
    if (A == NULL || B == NULL) {
        printf("List Error: calling concatList() on NULL List reference\n");
        EXIT_FAILURE;
    }

    List newL = newList();
    Node temp = NULL;
    for (temp = A->front; temp != NULL; temp = temp->next)
        append(newL, temp->data);
    for (temp = B->front; temp != NULL; temp = temp->next)
        append(newL, temp->data);
    return newL;
}
*/
