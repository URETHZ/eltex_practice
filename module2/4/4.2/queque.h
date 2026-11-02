#ifndef QUEQUE_H
#define QUEQUE_H

typedef struct {
    int priority;
    struct Queque_node* next;
    struct Queque_node* prev;
} Queque_node;



#endif