#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <pthread.h>

//Defines and macros
#define SYSTEM_MALLOC 1
#define STRUCT_SIZE 24 
#define MULTIPLIER 10
#define ALIGN_SIZE 8
#define ALIGN(size) (((size) + (ALIGN_SIZE-1)) & ~(ALIGN_SIZE-1))

typedef struct chunkStatus
{
    int size;
    int available;
    struct chunkStatus* next;
    struct chunkStatus* prev;
    char end[1]; 		
} chunkStatus;

chunkStatus* head = NULL;
chunkStatus* lastVisited = NULL;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
void* brkPoint0 = NULL;

// findChunk: 사용자 요청 크기와 일치하거나, 더 큰 블록을 찾는 역할
// 첫 번째 블록을 가르키는 ptr과 size를 받아서, 요청에 맞는 블록을 찾음

chunkStatus* findChunk(chunkStatus* headptr, unsigned int size){
    chunkStatus* ptr = headptr; // 리스트를 탐색하는데 사용됨

    while (ptr != NULL)    {
        if (ptr->size >= (size + STRUCT_SIZE) && ptr->available == 1){
            // 요청 size+ 구조체 사이즈 (기본적으로 필요한 사이즈) 보다 크고
            // 이용가능하다면
            return ptr;
        }
        lastVisited = ptr;
        ptr = ptr->next;
    }
    return ptr;
}

// 큰 블록을 2개로 분할한다.
// 첫번째 블록은 사용자가 요청한 크기이며, 두번째 블록은 나머지 크기임
// ptr이 가르키는 블록을 나누고, 크기가 요청한 크기임
void splitChunk(chunkStatus* ptr, unsigned int size){
    chunkStatus* newChunk; // 나누어진 2번째 블록

    newChunk = ptr->end + size; // 두번째 블록의 시작 위치가 계산됨
    newChunk->size = ptr->size - size - STRUCT_SIZE; 
    newChunk->available = 1;
    newChunk->next = ptr->next;
    newChunk->prev = ptr; // 분할했으므로 이전은 기존의 ptr로 설정해야함

    if ((newChunk->next) != NULL){
        (newChunk->next)->prev = newChunk; // 연결리스트의 성질
    }

    ptr->size = size; // 요청한 크기의 사이즈
    ptr->available = 0; // 사용중인 상태
    ptr->next = newChunk;
}

// 힙에서 사용 가능한 메모리 양을 증가시키고, 힙의 breakptr를 변경한다.
// 가르키는 블록 이후에 메모리를 할당하여 늘린다. (그래서 lastvisit이 저장된것)
// 연결리스트를 수정해야한다.
chunkStatus* increaseAllocation(chunkStatus* lastVisitedPtr, unsigned int size){
    brkPoint0 = sbrk(0); // 현재 힙의 break포인트를 가져온다. (힙의 끝)
    chunkStatus* curBreak = brkPoint0; // 마지막 위치를 가르킴

    if (sbrk(MULTIPLIER * (size + STRUCT_SIZE)) == (void*)-1){
        // 힙의 브레이크 포인트를 늘리는데, 사용자가 요청한  크기*10만큼 늘린다.
        // 브레이크 포인트를 변경할 수 없는 경우, NULL을 반환
        return NULL;
    }

    curBreak->size = (MULTIPLIER * (size + STRUCT_SIZE)) - STRUCT_SIZE;
    // 새로운 브레이크 포인트의 크기를 계산, 증가된 메모리 양에서 구조체 사이즈를 뺌
    curBreak->available = 0; 
    // 이용중
    curBreak->next = NULL;
    // 현재 리스트의 끝
    curBreak->prev = lastVisitedPtr;
    lastVisitedPtr->next = curBreak;

    if (curBreak->size > size) {
        splitChunk(curBreak, size);
    }

    return curBreak;
}

// 해제된 블록을 이전 블록과 병합하는 역할
// freed가 가리키는 블록을 이전 블록과 병합
void mergeChunkPrev(chunkStatus* freed)
{
    chunkStatus* prev;
    prev = freed->prev;

    if (prev != NULL && prev->available == 1)
    { // prev가 NULL이 아니고, 이전 블록이 사용 가능한 상태인 경우
        prev->size = prev->size + freed->size + STRUCT_SIZE; // 합체
        prev->next = freed->next; //freed와 합체
        if ((freed->next) != NULL) {
            (freed->next)->prev = prev; // 해제된 블록의 다음 블록의 이전 블록을 이전블록으로 설정
        }
    }
}

// 해제된 청크를 그 다음 청크와 병합하는 것
// 로직은 전과 같다.
void mergeChunkNext(chunkStatus* freed){
    chunkStatus* next;
    next = freed->next;

    if (next != NULL && next->available == 1){
        freed->size = freed->size + STRUCT_SIZE + next->size;
        freed->next = next->next;
        if ((next->next) != NULL) {
            (next->next)->prev = freed;
        }
    }
}

// headptr이 연결 리스트를 탐색하고, 각 노드의 정보를 화면에 출력
void printList(chunkStatus* headptr){
    int i = 0;
    chunkStatus* p = headptr;

    while (p != NULL){
        printf("[%d] p: %d\n", i, p);
        printf("[%d] p->size: %d\n", i, p->size);
        printf("[%d] p->available: %d\n", i, p->available);
        printf("[%d] p->prev: %d\n", i, p->prev);
        printf("[%d] p->next: %d\n", i, p->next);
        printf("__________________________________________________\n");
        i++;
        p = p->next;
    }
}

// 메모리 힙에서 요청된 크기의 메모리를 할당하는 역할
// 워드 경계에 시작하고 끝나도록 패딩을 설정하여 할당한다.
void* mymalloc(unsigned int _size)
{
    void* brkPoint1; // 힙의 현재 브레이크 포인트를 가리키는 포인터
    unsigned int size = ALIGN(_size); // 워드 경계에 맞게 정렬된 크기로 설정
    int memoryNeed = MULTIPLIER * (size + STRUCT_SIZE); // 요청된 크기를 처리하기 위해 필요한 메모리 양
    chunkStatus* ptr; // 새로운 청크를 할당하기 위함
    chunkStatus* newChunk; 

    pthread_mutex_lock(&lock); // 다중 스레드 환경에서의 안전한 실행 보장
    brkPoint0 = sbrk(0); // 현재 힙의 브레이크 포인트를 가리키는 포인터로 초기화


    if (head == NULL){
        // 프로그램이 처음 실행되는 경우, 처음으로 메모리 할당을 수행
        if (sbrk(memoryNeed) == (void*)-1){
            // 힙에 추가 메모리를 할당, 에러 체크
            pthread_mutex_unlock(&lock);
            return NULL;
        }

        brkPoint1 = sbrk(0);
        head = brkPoint0;
        head->size = memoryNeed - STRUCT_SIZE;
        head->available = 0;
        head->next = NULL;
        head->prev = NULL;

        ptr = head;

        if (MULTIPLIER > 1) // 첫번째 할당시 청크를 분할할 필요가 있는지 검사
            splitChunk(ptr, size);

        pthread_mutex_unlock(&lock); // 다른 스레드가 메모리 할당을 수행할 수 있도록
        return ptr->end;
    }
    else{
        chunkStatus* freeChunk = NULL; // 할당 가능한 청크를 가리킬 포인터를 초기화한다.
        freeChunk = findChunk(head, size); // 요청한 크기에 맞는 할당 가능한 청크를 검색

        if (freeChunk == NULL){ // 추가 메모리를 할당하지 못할 경우
            freeChunk = increaseAllocation(lastVisited, size);	//Extend the heap
            if (freeChunk == NULL){
                pthread_mutex_unlock(&lock);
                return NULL;
            }
            pthread_mutex_unlock(&lock);
            return freeChunk->end;
        }

        else{ // 요청한 크기에 맞는 할당 가능한 청크를 찾은 경우
            if (freeChunk->size > size) {
                splitChunk(freeChunk, size);
            }
        }
        pthread_mutex_unlock(&lock);
        return freeChunk->end;
    }
}

unsigned int myfree(void* ptr) {
    pthread_mutex_lock(&lock); // 다중 스레드 환경에서 안전한 실행 보장

    chunkStatus* toFree; // 해제할 메모리 블록의 첫 번째 바이트를 가리키는 포인터로 초기화
    toFree = ptr - STRUCT_SIZE; //구조체 크기를 뺀 위치로 설정 

    if (toFree >= head && toFree <= brkPoint0){ // 메모리가 유효한 범위 내에 있는지
        toFree->available = 1;
        mergeChunkNext(toFree); // 병합
        mergeChunkPrev(toFree);
        pthread_mutex_unlock(&lock); // 다른 스레드가 해제를 수행할 수 있도록
        return 0;

    }
    else{
        pthread_mutex_unlock(&lock);
        return 1; // 메모리 해제가 실패한 경우
    }
}
