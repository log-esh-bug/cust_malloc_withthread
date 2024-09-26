#include<stdio.h>
#include<stdlib.h>
#include<time.h>
#include<unistd.h>
#include<string.h>
#include<pthread.h>


#define MEM_BLOCK_SIZE (1024*1024)
#define total_size (1024*1024*1024)
#define MAX_BLOCKS total_size/MEM_BLOCK_SIZE
#define H_size (sizeof(int))
#define CHUNK_MIN_SIZE 16
#define SIZE_PADDER 8 //Must be even
int FinishCode=0;

#define ARRAY_SIZE 1000000
#define tempMaxChunkSize 100
#define tempString "The truth is, there are lots of equally iconic Dickinson poems, so consider this a stand-in for them all."

// pthread_mutex_t FreePtrMutex= PTHREAD_MUTEX_INITIALIZER;
// pthread_mutex_t printMemoryMutex=PTHREAD_MUTEX_INITIALIZER;
// pthread_mutex_t printFreeMutex=PTHREAD_MUTEX_INITIALIZER;

typedef struct block{
    struct block* nextBlock;
    void* data;
    void* currptr;
}Block;

typedef struct mc{
    Block* blockList;
    int usedblocks;
    void* freePtr;
    pthread_mutex_t freePtrMutex;
}MemoryContext;

MemoryContext* createMemoryContext(){

    MemoryContext* mc=(MemoryContext*)malloc(sizeof(MemoryContext));
    if (mc == NULL) {
        printf("Cannot create Memory context!\n");
        return NULL;
    }
    mc->blockList = NULL;
    mc->usedblocks = 0;
    mc->freePtr=NULL;
    pthread_mutex_init(&(mc->freePtrMutex),NULL);
    return mc;

}

void setHeader(void* ptr,int size,int isFree){
    if(ptr==NULL)return;
    int h = size + isFree;
    *((int*)ptr-1)=h;
}

int isFree(void* ptr){
    return (*((int*)ptr-1)) & 1;
}

int extractsize(void* ptr){
    int a = *((int*)ptr);
    if(a&1) return a-1;
    return a;
}

int getSize(void* ptr){
    return extractsize(ptr-H_size);
}

int pad_size(int size){
    if(size<CHUNK_MIN_SIZE)return CHUNK_MIN_SIZE;
    if(size % SIZE_PADDER ==0)return size;
    return size +(SIZE_PADDER - size % SIZE_PADDER);
}

void removeFree(void* ptr,MemoryContext* mc){

    pthread_mutex_lock(&(mc->freePtrMutex));

    void* prev =*(void**)ptr;
    void* next=*((void**)ptr+1);
    *(void**)ptr=NULL;
    *((void**)ptr+1)=NULL;

    if(prev!=NULL && next!=NULL){
        *((void**)prev+1)=next;
        *(void**)next=prev;
    }
    else if(prev==NULL){
        if(next)
            *(void**)next=NULL;
        mc->freePtr=next;
    }
    else{
        *((void**)prev+1)=NULL;
    }
    
    pthread_mutex_unlock(&(mc->freePtrMutex));
    
}

void addFree(void* ptr,MemoryContext* mc){

    pthread_mutex_lock(&(mc->freePtrMutex));

    if(mc->freePtr==NULL ) {
        *(void**)ptr = NULL;
        *((void**)ptr+1) = NULL;
        mc->freePtr = ptr;
    }
    else{
        *(void**)ptr=NULL;
        *((void**)ptr+1)=mc->freePtr;
        *((void**)mc->freePtr)=ptr;
        mc->freePtr=ptr;
    } 

    pthread_mutex_unlock(&(mc->freePtrMutex));

}

void mergeFree(MemoryContext* mc){
    void* first=mc->freePtr;
    if(first==NULL)return;
    void* nextPtr=NULL;
    int totSize=0;
    while(1){
        totSize=getSize(first);
        nextPtr=first+getSize(first)+H_size;
        while(isFree(nextPtr)){
            removeFree(nextPtr,mc);
            totSize+=(getSize(nextPtr)+H_size);
            nextPtr=nextPtr+sizeof(nextPtr)+H_size;
        }
        if(totSize!=getSize(first)){
            setHeader(first,totSize,1);
        }
        totSize=0;
        first = *((void**) first+1);
        totSize=0;
        if(first==NULL)break;
    }
}

void* periodicMerging(void* args){
    //printf("merge\n");
    MemoryContext* mc = (MemoryContext*)args;
    while(1){
        // pthread_mutex_lock(&FreePtrMutex);
        mergeFree(mc);
        // pthread_mutex_unlock(&FreePtrMutex);
        
        if(FinishCode){
            break;
        }
        usleep(200);
    }
}

void* memoryAlloc(MemoryContext* mc,int size){
    
    //printf("alloc\n");

    size = pad_size(size);
    
    void* ptr=mc->freePtr;
    while(ptr){
        int nodeSize=getSize(ptr);
        if(nodeSize >= size){
            removeFree(ptr,mc);
            if (nodeSize >= size + H_size + CHUNK_MIN_SIZE) {
                void* newPtr=(void*)ptr+size + H_size;
                int newSize=nodeSize-size-H_size;
                setHeader(newPtr,newSize,1);
                setHeader(ptr,size,0);
                addFree(newPtr,mc);
            }
            else{
                setHeader(ptr,nodeSize,0);
            }
            return ptr;
        }
        ptr = *((void**) ptr+1);
    }

    Block* block=mc->blockList;
    while(block){
        ptr = block->currptr + H_size;
        if(ptr + size <= (block->data + MEM_BLOCK_SIZE)){
            setHeader(ptr,size,0);
            block->currptr = ptr+size ;
            // *(void**)(block->currptr+H_size) = NULL;
            return ptr;
        }
        block=block->nextBlock;
    }

    if(mc->usedblocks >= MAX_BLOCKS){
        return NULL;
    }
    block=(Block*)malloc(sizeof(Block));
    if(block==NULL){
        return NULL;
    }
    block->data=malloc(MEM_BLOCK_SIZE);

    if(block->data == NULL){
        free(block);
        //printf("Unable to allocate a new Bock!\n");
        return NULL;
    }


    block->currptr=block->data;

    block->nextBlock=mc->blockList ;
    mc->blockList=block;
    mc->usedblocks++;
    void* p=block->currptr+H_size;
    setHeader(p,size,0);
    block->currptr = p+ size ;
    return p;
}



void memFree(void* ptr,MemoryContext* mc){

    //printf("free\n");
    
    int size= getSize(ptr);

    memset(ptr,'0',size);
    addFree(ptr,mc);
    setHeader(ptr,size,1);  
}

void* memoryRealloc(void* ptr,int size,MemoryContext* mc){

    //printf("realloc\n");

    if(isFree(ptr)){
        printf("Memory is not allocated.\n");
        return NULL;
    }


    int nodeSize=getSize(ptr);

    size=pad_size(size);

    if(size == nodeSize){
        return ptr;
    }

    void* nextPtr = (void*)(ptr + nodeSize + H_size);
    void* newNext=NULL;
    int newNextSize;

    if(isFree(nextPtr)){
        removeFree(nextPtr,mc);
        if(size < nodeSize){
            setHeader(ptr,size,0);
            int extraSpace = nodeSize - size;
            newNext = ptr + size + H_size;
            newNextSize = getSize(nextPtr) + extraSpace;
            setHeader(newNext,newNextSize,1);
            addFree(newNext,mc);
            return ptr;
        }
        else if(size > nodeSize){
            int neededSize = size - nodeSize;
            if(neededSize <= getSize(nextPtr)){
                newNextSize = getSize(ptr) + getSize(nextPtr) - size;
                newNext = ptr + size + H_size;
                setHeader(ptr,size,0);
                setHeader(newNext,newNextSize,1);
                addFree(newNext,mc);
                return ptr;
            }
        }
    }

    else if(size + H_size + CHUNK_MIN_SIZE < nodeSize ){
        
        setHeader(ptr,size,0);

        newNext = (void*)(ptr + size + H_size);
        newNextSize =nodeSize-(size + H_size);

        setHeader(newNext,newNextSize,1);
        addFree(newNext,mc);
        return ptr;
    }
    else if(size < nodeSize){
        return ptr;
    }

    memFree(ptr,mc);
    return memoryAlloc(mc,size);
}

void destroyMemoryContext(MemoryContext* mc) {

    //printf("destroy\n");
    if (mc == NULL) {
        printf("Memory context is not created!\n");
        return;
    }
    Block* block = mc->blockList;
    while (block!=NULL) {
        Block* temp=block;
        block=block->nextBlock;
        free(temp->data);
        free(temp);
    }
    pthread_mutex_destroy(&(mc->freePtrMutex));
    free(mc);
}

void printMemory(MemoryContext* mc){
    
    // pthread_mutex_lock(&printMemoryMutex);

    Block* block=mc->blockList;
    int b=1;
    printf("----------------------------------------------------------\n");
    printf("%7s%18s%7s%8s\n","Block","Pointer","Size","isFree");
    printf("----------------------------------------------------------\n");
    while (block)
    {
        void* ptr=block->data + H_size;
        while(ptr <= block->currptr){
            // totSize+=getSize(ptr);
            if(getSize(ptr)<0)continue;
            printf("%7d%18p%7d%8d\n",b,ptr,getSize(ptr),isFree(ptr));
            ptr= ptr + getSize(ptr)+H_size;
        }
        block=block->nextBlock;

        b=b+1;
        if(block)printf("\n");
    }
    printf("----------------------------------------------------------\n");

    // pthread_mutex_unlock(&printMemoryMutex);

}

void printFree(MemoryContext* mc){

    if(pthread_mutex_trylock(&(mc->freePtrMutex))!=0){
        printf("unable to print Free!\n");
        return;
    }

    void* ptr=mc->freePtr;
    if(!ptr){
        printf("Nothing Free!\n");return ;
    }
    printf("-------------------\n");
    printf("Free Memory List:\n");
    printf("-------------------\n");

    while(1){
        printf("%p \n",ptr);
        ptr = *((void**) ptr+1);
        if(ptr==NULL)break;
    }
    printf("-------------------\n");

    pthread_mutex_unlock(&(mc->freePtrMutex));
}

void user(MemoryContext* mc, int arraySize) {

    // printf("%d\n",getpid());
    if(mc == NULL) {
        printf("Unable to create memory context!\n");
        return;
    }

    srand((unsigned int)time(NULL));

    pthread_t periodicThread;
    if(pthread_create(&periodicThread,NULL,*periodicMerging,(void*)mc)!=0){
        printf("Error creating the thred!\n");
        return ;
    }

    void** ptrs = (void**)malloc(arraySize * sizeof(char*));
    if (ptrs == NULL) {
        printf("Unable to allocate pointer array of size %d!\n", arraySize);
        return;
    }

    for (int i = 0; i < arraySize; i++) {
        int tempSize = rand() % tempMaxChunkSize + 1;
        ptrs[i] = (char*)memoryAlloc(mc, tempSize);
        if (ptrs[i] != NULL) {
            strncpy(ptrs[i], tempString, tempSize - 1);
            // ptrs[i][tempSize - 1] = '\0'; 
        }
    }


    for (int i = 0; i < arraySize/10 ; i++) {
        int ind = rand() % arraySize;
        if (ptrs[ind] != NULL) {
            memFree(ptrs[ind], mc);
            ptrs[ind] = NULL;
        }
    }


    for (int i = 0; i < arraySize/1000; i++) {
        int ind = rand() % arraySize;
        if (ptrs[ind] != NULL && !isFree(ptrs[ind])) {
            int tempSize = rand() % tempMaxChunkSize + 1;
            void* temp =memoryRealloc(ptrs[ind],tempSize,mc);
            if(temp!=NULL){
                ptrs[ind]=temp;
            }
        }
    }

    FinishCode = 1;
    pthread_join(periodicThread, NULL);

    // sleep(20);

    free(ptrs); 
    ptrs=NULL;
}
