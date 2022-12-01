#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <sys/mman.h>
#include <math.h>
#include "os.h"



/*
virtual address 64 bits:

|63  (7)  57|56     (45)       12|11  (12)  0|
 ---------------------------------------------
| sign ext  |  virtual address#  |   offset  |   
---------------------------------------------

only 57 first bits are used for translation.

PTB (page table) structure:

page size : 4KB (4096 B)
PTB node's are pages, i.e 4KB size.

PTE (page table entry) 64 bits:

|63         (52)           12|11  (11)  1| 0 |
 ---------------------------------------------
|          page/frame#       |  (unused) | v |  
---------------------------------------------

 since page size is 4 KB=4096 B and PTE is 64 b = 8 B , 
 every node has 4096/8=512=2^9 sons,
 so wi will need 9 bits for each node.
 
 vpn is 45 bits, so we will have 45/9=5 layers, 
 and the ppn is stored in the last layer.
         
*/


/*
function to creat/destroy vitrual memory mapping in the Page Table.
*/

void page_table_update(uint64_t pt, uint64_t vpn, uint64_t ppn){

	// r = 511 (10) = 111111111 (2) = 0x1ff (16).
	// used to obtain 9 bits slices fron the 45 bits of virtual-address# .
	uint64_t r = pow(2,9)-1; 
	
	// layer is initialized to the root of the Page Table.
	uint64_t* layer = phys_to_virt(pt<<12); 
	int i; 
	uint64_t entry;
	for (i=4; i>=0; i--){
		entry = (vpn&(r<<(9*i)))>>(9*i); // layer entry  (bits [45-k, 37-k] in the vpn for layer k=0,1,2,3,4)
		if (i==0){ // we arrived to the last layer, where the ppn should be stored.
			if (ppn==NO_MAPPING)
				layer[entry]= 0x0; // destroy this entry 
			else
				layer[entry]= ((ppn<<12)|0x1); // update this entry to store the ppn with valid bit =1.
			return;
		}
		
		if (!(layer[entry]&0x1)){ // wer'e not in the last layer and the layer is not valid (valid bit=0) 
			if (ppn==NO_MAPPING)
				return; // nothing to update/delete
			layer[entry]= ((alloc_page_frame()<<12)|0x1); // create new page for this entry with valid bit=1.
		}
		//countinue to next layer 
		layer = phys_to_virt(layer[entry]);
	}
}
/*
returns the ppn that vpn is mapped to, or NO_MAPPING if no mapping exist. 
*/
uint64_t page_table_query(uint64_t pt, uint64_t vpn){
	uint64_t* layer = phys_to_virt(pt<<12);
	uint64_t r=pow(2,9)-1;
	int i;
	uint64_t entry;
	for (i=4; i>=0; i--){
		entry = (vpn&(r<<(9*i)))>>(9*i);
		if (!(layer[entry]&0x1)) //the entry is not valid
				return NO_MAPPING;
		if (i==0){ // this is the last layer
			return layer[entry]>>12; // return the ppn stored in the the last entry 63-12 bits.
		}
		layer = phys_to_virt(layer[entry]); //countinue to next layer
	}
	return NO_MAPPING;
}


