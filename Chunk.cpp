#include "MemControlBlock.h"
#include <iostream>
#include <assert.h>
#include <vector>


struct Chunk{
    void Init(std::size_t blockSize, unsigned char blocks);
    void * Allocate(std::size_t blockSize);
    void Deallocate(void*p,std::size_t blockSize);
    unsigned char*pData;
    unsigned char firstAvailableBlock;
    unsigned char blockAvailable;
};


void Chunk::Init(std::size_t blockSize,unsigned char blocks){
    pData=new unsigned char[blockSize*blocks]; // 처음 공간 생성
    firstAvailableBlock=0; // 사용 가능한 블록의 첫 인덱스
    blockAvailable=blocks;  // 블록 개수

    unsigned char*p=pData;
    for(unsigned char i=0; i!=blocks; p+=blockSize){
        *p=++i;
    }
    // 처음 공간 생성한 주소에서, 생성한 블록 개수만큼 
    // 주소를 업데이트해줌으로써, 연결시켜준다.
}

void*Chunk::Allocate(std::size_t blockSize){
    if(!blockAvailable){
        return 0;
    }

    unsigned char*pResult=pData+(firstAvailableBlock*blockSize);
    // 새로 할당하는 위치
    // 만약, first가 2, size가 4라면 : 위의 식만큼 계산된 위치에서 한 bit가 할당
    
    firstAvailableBlock=*pResult;
    blockAvailable--;
    return pResult;
}

void Chunk::Deallocate(void*p,std::size_t blockSize){
    assert(p>=pData); 
    unsigned char* toRelease=static_cast<unsigned char*>(p);
    assert((toRelease-pData)%blockSize==0);
    *toRelease=firstAvailableBlock;
    firstAvailableBlock=static_cast<unsigned char>((toRelease-pData)/blockSize);
    assert(firstAvailableBlock==(toRelease-pData)/blockSize);
    blockAvailable++;
}


class  FixedAllocator{
    private:
        std::size_t blockSize; // 고정된 블록 크기의 저장
        unsigned char numBlocks; // 각 청크에 있는 블록의 수 저장
        typedef std::vector<Chunk> Chunks; // 청크들을 관리
        Chunks chunks;
        Chunk* allocChunk; // 사용 가능한 공간을 확인
        Chunk* deallocChunk;
        void* Allocate();
        void Deallocate();
};


void*FixedAllocator::Allocate(){
    if(allocChunk==0||allocChunk->blockAvailable==0){
        Chunks::iterator i=chunks.begin();
        for(;; ++i){
            if(i==chunks.end()){
                chunks.reserve(chunks.size()+1);
                Chunk newChunk;
                newChunk.Init(blockSize,numBlocks);
                chunks.push_back(newChunk);
                allocChunk=&chunks.back();
                deallocChunk=&chunks.back();
                break;
            }
            if(i->blockAvailable>0){ // 반복중인 동안, 공간이 있을 경우
                allocChunk=&*i;
                break;
            }
        }
    }

    assert(allocChunk!=0);
    assert(allocChunk->blockAvailable>0);
    return allocChunk->Allocate(blockSize);
}

void FixedAllocator::Deallocate(){
    if(deallocChunk->blockAvailable!=0) // 이용이 가능한데, 해제하려고 하면 오류, 수정
    {
        Chunks::iterator i=chunks.begin(); // 시작부터 이용된 마지막 부분
        for(;; ++i){
            if(i->blockAvailable!=0){
                // 사용중인 블록의 마지막 위치를 찾기위함
                deallocChunk=&*i;
            }
        }
    }

    assert(deallocChunk==0);
    assert(deallocChunk->blockAvailable==0);
    // 삭제하려는 위치인데, 할당이 안되어 있으면 오류이므로 오류 체크
    deallocChunk->Deallocate(deallocChunk,blockSize);

    allocChunk=&chunks.back();
    deallocChunk=&chunks.back();
}