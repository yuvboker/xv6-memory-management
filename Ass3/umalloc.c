#include "types.h"
#include "stat.h"
#include "user.h"
#include "param.h"

// Memory allocator by Kernighan and Ritchie,
// The C programming Language, 2nd ed.  Section 8.7.

typedef long Align;

union header {
  struct {
    union header *ptr;
    uint size;
  } s;
  Align x;
};

typedef union header Header;

static Header base;
static Header *freep;

void
free(void *ap)
{

  Header *bp, *p;
  pmalloc_check(ap,3);
  bp = (Header*)ap - 1;
  //printf(1,"free pointer: %d\n", bp );
  for(p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
    if(p >= p->s.ptr && (bp > p || bp < p->s.ptr))
      break;
  if(bp + bp->s.size == p->s.ptr){
    bp->s.size += p->s.ptr->s.size;
    bp->s.ptr = p->s.ptr->s.ptr;
  } else
    bp->s.ptr = p->s.ptr;
  if(p + p->s.size == bp){
    p->s.size += bp->s.size;
    p->s.ptr = bp->s.ptr;
  } else
    p->s.ptr = bp;
  freep = p;
}

int
pfree(void *ap)
{
  //printf(1, "p_pfree: %d\n",ap );
  if(pmalloc_check(ap,2) == 1){
    free(ap);
    pmalloc_check(ap,3);
    return 1;
  }
  //printf(1,"pfree failed\n");
  return -1;

}
static Header*
morecore(uint nu)
{
  char *p;
  Header *hp;

  if(nu < 4096)
    nu = 4096;
  p = sbrk(nu * sizeof(Header));
  if(p == (char*)-1)
    return 0;
  hp = (Header*)p;
  hp->s.size = nu;
  free((void*)(hp + 1));
  return freep;
}

void*
malloc(uint nbytes)
{
  pmalloc_check(0,4);
  //printf(1,"malloc was called \n");
  Header *p, *prevp;
  uint nunits;
  nunits = (nbytes + sizeof(Header) - 1)/sizeof(Header) + 1;
  if((prevp = freep) == 0){
    base.s.ptr = freep = prevp = &base;
    base.s.size = 0;
  }
  for(p = prevp->s.ptr; ; prevp = p, p = p->s.ptr){

    if(p->s.size >= nunits){

      if(p->s.size == nunits)
        prevp->s.ptr = p->s.ptr;
      else {
        
        p->s.size -= nunits;
        p += p->s.size;
        //printf(1, "new p : %d\n",p);
        p->s.size = nunits;
      }
      freep = prevp;
      //printf(1, "prevp->s.ptr : %d\n",prevp->s.ptr);
      //printf(1,"returned address using malloc: %d\n",(void*)(p+1) );
      return (void*)(p + 1);
    }
    if(p == freep)
      if((p = morecore(nunits)) == 0)
        return 0;
  }
}

int
protect_page(void* addr){
  Header *p = (Header*)addr;
  //printf(1, "trying to protect: %d\n",p );
  int result = pmalloc_check((void*)p,1);
  if(result == -1)
    return -1;
  return 1;
}
void*
pmalloc(void)
{

  //printf(1,"pmalloc was called \n");
  pmalloc_check(0,4);
  pmalloc_check(0,4);
  Header *p, *prevp;
  int align;// if p = 12402. align = 4096*(p/4096+1)-p = 4096*4-12402 = 3982.
  int nualigned;
  if ((prevp = freep) == 0) {
    base.s.ptr = freep = prevp = &base;
    base.s.size = 0;
  }
  for (p = prevp->s.ptr; ; prevp = p, p = p->s.ptr) {
    //printf(1,"p %d\n",p );
   	align = 4096 * ((int)p/4096 + 1) - (int)p; // align = 4096*(3+1) - 12498 = 16384 - 12498 = 3886
   	nualigned = (align + sizeof(Header) - 1) / sizeof(Header)-1;
  	int desired_block = p->s.size -nualigned;

  	if(desired_block >= 513){
        if((int)p % 8){
        int align_Header = 8 - (int)prevp->s.ptr%8;
        Header* p_fixed = (Header*)((void*)p + align_Header);
        p_fixed->s.ptr = p->s.ptr;
        p_fixed->s.size = p->s.size;
        prevp->s.ptr = p_fixed;
        p = p_fixed;

      }
  	    //printf(1, "p_start: %d\n",p );
  	    //printf(1, "align: %d\n",align );
  	    //printf(1, "nualigned: %d\n",nualigned );
  	    //printf(1, "p.size: %d\n",p->s.size );
      	if(desired_block == 513){
      		//printf(1, "reached here!\n");
      		    prevp->s.ptr= p->s.ptr;
              p += nualigned;
              p->s.size = 513;
              if((int)(p+1)%4096)
               printf(1,"something is fucked up1!!!! \n");
          }
          else{
          	Header* continuation =  p + nualigned + 513;
          	continuation->s.size = p->s.size - 513 - nualigned;
          	//printf(1,"continuation->s.size: %d\n",continuation->s.size);	
      		 continuation->s.ptr = p->s.ptr;
      		 prevp->s.ptr = continuation;       
  	        p += nualigned;
  	        //printf(1,"p after alignment: %d\n",p);
  	        p->s.size = 513;
            if((int)(p+1)%4096)
               printf(1,"something is fucked up2!!!! \n");
          }

          freep = prevp;
          void* addr = (void*)(p+1);
          //printf(1, "p_new: %d\n",p+1 );
          pmalloc_check(addr,0);
          //printf(1,"returned address by pmalloc: %d\n",addr);
          return addr;
  	}
      if (p == freep)
        if ((p = morecore(513)) == 0)
          return 0;
      	

  }
}

