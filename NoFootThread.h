#ifndef MY_MEM_H
#define MY_MEM_H

typedef struct block{
    struct block* nextBlock;
    void* data;
    void* currptr;
}Block;

typedef struct mc{
    Block* blockList;
    int usedblocks;
    void* freePtr;
}MemoryContext;


MemoryContext* createMemoryContext();
void* memoryAlloc(MemoryContext* mc,int size);
void memFree(void* ptr,MemoryContext* mc);
void* memoryRealloc(void* ptr,int size,MemoryContext* mc);
void printMemory(MemoryContext* mc);
void destroyMemoryContext(MemoryContext* mc);
void setHeader(void* ptr,int size,int isFree);
int isFree(void* ptr);
int getSize(void* ptr);
int pad_size(int size);
int extractsize(void* ptr);
void printFree(MemoryContext* mc);
void removeFree(void* ptr,MemoryContext* mc);
void mergeFree(MemoryContext* mc);
void user(MemoryContext* mc,int n);

#endif