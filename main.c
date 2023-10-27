#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#define WSIZE 4 // word, hear, footer 사이즈를 byte로
#define DSIZE 8 // double word size를 byte로
#define CHUNKSIZE (1<<12) // heap을 늘릴 때, 

#define MAX(x,y) ((x)>(y)?x:y) // x,y중 큰 값을 가진다.

#define PACK(size,alloc) ((size)|(alloc)) // 가용여부와 사이즈를 합치면 온전한 주소가 나온다.
// size를 pack하고 개별 word안의 bit를 할당, 헤더에 사용

// address p위치에 words를 read,write한다.
#define GET(p)(*(unsigned int*)p) //p를 참조, 주소와 값을 알 수 있다. 다른 블록을 가리키거나 이동한다.
#define PUT(p,val) (*(unsigned int*)(p)=(int)(val)) //블록의 주소를 담는다. 위치를 담아야, 헤더나 푸터를 읽어서 이동가능하다.

// address p위치로 부터 size를 읽고 field를 할당
#define GET_SIZE(p) (GET(p) & ~0x7) // 11111000 비트 연산을 하면, 헤더에서 블록 size만 가져옴 (가용여부는 x)
#define GET_ALLOC(p) ((GET(p)&0x1) //0000 0001 헤더에서 가용여부만 가져옴

// ptr bp, 헤더와 푸터의 주소를 계산
#define HDRP(bp) ((char*)bp - WSIZE) //bp가 어디있던 상관없이 WSIZE 앞에 위치한다.
#define FTRP(bp) ((char*)bp + GET_SIZE(HDRP(bp))-DSIZE) // 헤더의 끝 지점부터 GETSZE만큼 더한 후, 8바이트를 빼는게 푸터의 시작위치가 된다.

// 이전 블록과 다음 블록의 주소를 계산
#define NEXT_BLKP(bp) ((char*)bp) + GET_SIZE(((char*)(bp)-WSIZE))) // 그 다음 블록의 bp위치로 이동한다. 해당 블록의 크기만큼 이동, 블록의 헤더 뒤로 이동
#define PREV_BLKP(bp) ((char*)bp) -GET_SIZE(((char*)(bp)-DSIZE)) // 그 전 블록의 bp위치로 이동 (이전 블록의 footer로 이동하면, 그 전 블록의 사이즈를알 수 있음
static char* heap_listp; // 처음에쓸 큰 가용블록 힙을 만들어줌


int mm_init(){ // 처음 heap을 시작할 때, 0부터 시작
    if((heap_listp=mem_sbrk(4*WSIZE))==(void*)-1){
        return -1; //4*4 바이트만큼 늘려서 old,mem brk로 늘림
    }
    PUT(heap_listp,0); // 블록 생성시 넣는 패딩을 한 워드크기만큼 생성. heap 위치 맨 앞에 넣음
    PUT(heap_listp+(1*WSIZE),pack(DSIZE,1)); // prologue 헤더 생성. 할당을 하고, 8만큼 준다.>>4바이트 늘어난 시점부터 팩에서 나온 사이즈를 줄거다. 
    PUT(heap_listp+(2*WSIZE),pack(DSIZE,1)); // prologue 푸터 생성.
    PUT(heap_listp+(3*WSIZE),pack(0,1)); //epilogue block header를 만든다. 뒤로 밀리는형태
    heap_listp+=(2*WSIZE); //prologue header와 footer 사이로 포인터로 옮긴다.

    if(extend_heap(CHUNKSIZE/WSIZE)==NULL){ //extended heap을 통해 시작할 때 한번 heap을 늘려준다.
        return -1;
    }

    return 0;
}