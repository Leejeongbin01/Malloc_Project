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


int mm_init() { // 처음 heap을 시작할 때, 0부터 시작
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void*)-1) {
        return -1; //4*4 바이트만큼 늘려서 old,mem brk로 늘림
    }
    PUT(heap_listp, 0); // 블록 생성시 넣는 패딩을 한 워드크기만큼 생성. heap 위치 맨 앞에 넣음
    PUT(heap_listp + (1 * WSIZE), pack(DSIZE, 1)); // prologue 헤더 생성. 할당을 하고, 8만큼 준다.>>4바이트 늘어난 시점부터 팩에서 나온 사이즈를 줄거다. 
    PUT(heap_listp + (2 * WSIZE), pack(DSIZE, 1)); // prologue 푸터 생성.
    PUT(heap_listp + (3 * WSIZE), pack(0, 1)); //epilogue block header를 만든다. 뒤로 밀리는형태
    heap_listp += (2 * WSIZE); //prologue header와 footer 사이로 포인터로 옮긴다.

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) { //extended heap을 통해 시작할 때 한번 heap을 늘려준다.
        return -1;
    }

    return 0;
}

static void* extend_heap(size_t words) {
    // 새 가용 블록으로 힙 확장, 12승만큼 워드블록을 만듦
    char* bp;
    size_t size; // 이 함수에 넣을 size를 하나 만들어줌

    // alignment 유지를 위해 짝수 개수의 words를 할당
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
     
    if ((long)(bp = mem_sbrk(size)) == -1) { //sbrk로 size를 늘려서 long형으로 반환 (한 번 쫙 미리 늘림)
        return NULL; //반환되면 bp는 현재 만들어진 블록의 끝에 가있음
    } // 사이즈를 늘릴 때마다 old brk는 과거의 mem_brk위치로 감.

    // free block 헤더와 푸터를 init하고 epilogue 헤더를 init
    PUT(HDRP(bp), PACK(size, 0)); // free block헤더 생성. reegular block의 총합의 첫번째 부분. 현재 bp위치의 한칸 앞에 헤더를 생성
    PUT(FTRP(bp), PACK(size, 0)); // free block footer/ regular block 총합의 마지막 부분
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); //블록을 추가했으므로, epilogue header를 새롭게 위치 조정
    // bp를 헤더에서 읽은 사이즈만큼 이동하고, 앞으로 한칸 이동. 그 위치가 결국 늘린 블록 끝에서 한칸 간거라 거기가 epilogue header 위치.

    // 만약 prev block이 free였다면, coalesce해라
    return calesce(bp); // 함수 재사용을 위해 리턴값으로 선언
}

static void* coalesce(void* bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 그전 블록으로 가서 그 블록의 가용여부를 확인
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 그 뒷 블록의 가용여부 확인
    size_t size = GET_SIZE(HDRP(bp)); // 지금 블록의 사이즈 확인

    if (prev_alloc && next_alloc) { // 전과 앞이 모두 할당되어있다. 현재 블록의 상태는 할당에서 가용으로 변경
        return bp; // 이미 free에서 가용 (추후)
    }
    else if (prev_alloc && !next_alloc) { // 이전 블록은 할당, 다음 블록은 가용/ 현재 블록과 다음 블록 통합
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); // 다음 블록의 헤더를 보고,그 크기만큼 지금 블록의 크기와 더해줌
        PUT(HDRP(bp), PACK(size, 0)); // 헤더 갱신, 더 큰 크기로 put
        PUT(FTRP(bp), PACK(size, 0)); //푸터 갱신
    }
    else if (!prev_alloc && next_alloc) { // 이전 블록은 가용상태, 다음 블록은 할당/ 이전과 현재블록을 통합
        size += GET_SIZE(HDRP(PREV_BLKP(bp))); 
        PUT(FTRP(bp), PACK(size, 0)); // 푸터에 먼저 조정하려는 크기로 상태 변경
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // 현재 헤더에서 그 앞블록의 헤더로 위치 후 조정한 size를 넣음
        bp = PREV_BLKP(bp); // bp를 앞블록의 헤더로 이동
    }
    else { // 이전과 다음 블록 모두 가용상태, 3개 모두 하나로 통합
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp))); // 이전 블록 헤더, 다음 블럭 푸터까지 사이즈 늘리기
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); //헤더부터 앞으로가서 사이즈 넣고
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0)); // 푸터를 뒤로가서 사이즈 넣는다.
        bp = PREV_BLKP(bp); //헤더는 그 전 블록으로 이동
    }

    return bp;
}

l