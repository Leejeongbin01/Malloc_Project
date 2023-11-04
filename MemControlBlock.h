#pragma once

struct MemControlBlock{
    bool available;
    MemControlBlock* prev;
    MemControlBlock* next;
};