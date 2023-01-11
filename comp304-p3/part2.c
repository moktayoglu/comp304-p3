/**
 * virtmem.c 
 */

#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#define TLB_SIZE 16
#define PAGES 1024
#define PAGE_MASK 0b11111111110000000000

#define PAGE_SIZE 1024
#define OFFSET_BITS 10
#define OFFSET_MASK 0b00000000001111111111

#define FRAMES 256

#define MEMORY_SIZE PAGES * PAGE_SIZE
#define PHYSIC_MEMORY_SIZE FRAMES * PAGE_SIZE
// Max number of characters per line of input file to read.f
#define BUFFER_SIZE 10

struct tlbentry {
  unsigned char logical;
  unsigned char physical;
};


// TLB is kept track of as a circular array, with the oldest element being overwritten once the TLB is full.
struct tlbentry tlb[TLB_SIZE];
// number of inserts into TLB that have been completed. Use as tlbindex % TLB_SIZE for the index of the next TLB line to use.
int tlbindex = 0;

// pagetable[logical_page] is the physical page number for logical page. Value is -1 if that logical page isn't yet in the table.
int pagetable[PAGES];

// updated with new size
signed char main_memory[PHYSIC_MEMORY_SIZE]; 

// LRU clock keepers
int LRU_CLOCK_COUNTER[FRAMES];
int clock = 0;

// Pointer to memory mapped backing file
signed char *backing;

int max(int a, int b)
{
  if (a > b)
    return a;
  return b;
}

/* Returns the physical address from TLB or -1 if not present. */
int search_tlb(unsigned char logical_page) {
    /* TODO */
    struct tlbentry curr_entry;
    for(int i = 0; i<TLB_SIZE; i++){
      curr_entry = tlb[i];

      if(curr_entry.logical == logical_page){
        return curr_entry.physical;
      }

    }
    return -1;
    
}

/* Adds the specified mapping to the TLB, replacing the oldest mapping (FIFO replacement). */
void add_to_tlb(unsigned char logical, unsigned char physical) {
    /* TODO */
   tlb[tlbindex].logical = logical;
   tlb[tlbindex].physical = physical;
   
   tlbindex += 1;
   
   //For circular replacement of first come, first out 
   tlbindex = tlbindex % TLB_SIZE;
}



int main(int argc, const char *argv[])
{
  if ((argc != 5) || (strcmp(argv[3], "-p") != 0)){
    fprintf(stderr, "Usage ./virtmem backingstore input -p [0-1]\n");
    exit(1);
  }
  
  const char *backing_filename = argv[1]; 
  int backing_fd = open(backing_filename, O_RDONLY);
  backing = mmap(0, MEMORY_SIZE, PROT_READ, MAP_PRIVATE, backing_fd, 0); 
  
  const char *input_filename = argv[2];
  FILE *input_fp = fopen(input_filename, "r");

  //LRU or FIFO
  int isLRU = atoi(argv[4]);
  
  // Fill page table entries with -1 for initially empty table.
  int i;
  for (i = 0; i < PAGES; i++) {
    pagetable[i] = -1;
  }
  
  for (i = 0; i < FRAMES; i++) {
    LRU_CLOCK_COUNTER[i] = 999999999;
  }
  
  
  // Character buffer for reading lines of input file.
  char buffer[BUFFER_SIZE];
  
  // Data we need to keep track of to compute stats at end.
  int total_addresses = 0;
  int tlb_hits = 0;
  int page_faults = 0;
  int curr_fifo_frame = 0;
  int curr_lru_frame = 0;
  
  // Number of the next unallocated physical page in main memory
  unsigned char free_page = 0;
  
  while (fgets(buffer, BUFFER_SIZE, input_fp) != NULL) {
    total_addresses++;
    clock++;
    int logical_address = atoi(buffer);

    /* TODO 
    / Calculate the page offset and logical page number from logical_address */
    int offset = (logical_address & OFFSET_MASK);
    int logical_page = (logical_address & PAGE_MASK) >> OFFSET_BITS ;
    ///////
    
    int physical_page = search_tlb(logical_page);
    // TLB hit
    if (physical_page != -1) {
      tlb_hits++;
      //update each accession the clock
      LRU_CLOCK_COUNTER[physical_page] = clock;
      
    } else {
      physical_page = pagetable[logical_page];
      
      // Page fault
      if (physical_page == -1)
      {
	
        page_faults++; 
       
	if (!isLRU){//FIFO
		
        	physical_page = curr_fifo_frame; 
        	curr_fifo_frame++;
		curr_fifo_frame = curr_fifo_frame % FRAMES;

        	

	}else{ //LRU
	
		if(free_page < FRAMES) {
              		physical_page = free_page;
              		free_page++;  
            	}else{ 
			int min = 999999;
			int lru_frame = -1;
			for (int i=0; i<FRAMES; i++){ 
				if (LRU_CLOCK_COUNTER[i] < min){
					min = LRU_CLOCK_COUNTER[i];
					lru_frame = i;
				}
			}
			
			physical_page = lru_frame;
		}
		
	}
	
	for(int i=0; i<PAGES; i++) {
        	//remove from table if current frame was full
          	if(pagetable[i] == physical_page) {  
                	pagetable[i] = -1;
          	}
        }

        pagetable[logical_page] = physical_page;
        //update clock in each entry
        
      	LRU_CLOCK_COUNTER[physical_page] = clock;
        
        //copy backing into memory, with equivalent page and frame range
        memcpy(main_memory + physical_page * PAGE_SIZE, backing +  logical_page * PAGE_SIZE, PAGE_SIZE);
        
      }

      add_to_tlb(logical_page, physical_page);
    }
    int physical_address = (physical_page << OFFSET_BITS) | offset;
    signed char value = main_memory[physical_page * PAGE_SIZE + offset];
    
    printf("Virtual address: %d Physical address: %d Value: %d\n", logical_address, physical_address, value);
    //printf("clock: %d frame: %d \n", LRU_CLOCK_COUNTER[physical_page], physical_page);

  }
  
  printf("Number of Translated Addresses = %d\n", total_addresses);
  printf("Page Faults = %d\n", page_faults);
  printf("Page Fault Rate = %.3f\n", page_faults / (1. * total_addresses));
  printf("TLB Hits = %d\n", tlb_hits);
  printf("TLB Hit Rate = %.3f\n", tlb_hits / (1. * total_addresses));
  
  return 0;
}

