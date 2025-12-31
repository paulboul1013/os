#include "mem.h"

void memory_copy(uint8_t *source,uint8_t *dest,int nbytes){
    int i;
    for(i=0;i<nbytes;i++){
        *(dest+i)=*(source+i);
    }
}

void memory_set(uint8_t *dest,uint8_t val,uint32_t len){
    uint8_t *temp=(uint8_t *)dest;
    for (; len!=0;len--) {
        *temp++=val;
    }
}

//should be cpmputed at link time,but a hardcoded
//value is fine for now，remember kernel start at 0x10000
uint32_t free_mem_addr=0x10000;

//implementation is a pointer to some free memory which keeps growing
uint32_t kmalloc(size_t size,int align,uint32_t *phys_addr){
    //pages are aligned to 4k , or 0x1000
    if (align==1 && (free_mem_addr & 0xFFFFF000)){
        free_mem_addr&=0xFFFFF000;
        free_mem_addr+=0x1000;
    }

    //save also the physical address
    if (phys_addr) *phys_addr=free_mem_addr;

    uint32_t ret=free_mem_addr;
    free_mem_addr+=size; //remember to increment the pointer
    return ret;
}