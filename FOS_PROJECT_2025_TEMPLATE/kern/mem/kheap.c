#include "kheap.h"

#include <inc/memlayout.h>
#include <inc/dynamic_allocator.h>
#include <kern/conc/sleeplock.h>
#include <kern/proc/user_environment.h>
#include <kern/mem/memory_manager.h>
#include "../conc/kspinlock.h"

struct kspinlock lock;

struct stratVA_and_size {
	//uint32 va ;
	int num_of_pages_after_me;
	int num_of_pages_before_me;
};


struct stratVA_and_size list_of_startVA_and_size[(KERNEL_HEAP_MAX - KERNEL_HEAP_START)/PAGE_SIZE];


struct from_pa_to_va {
	uint32 va ;
};



struct from_pa_to_va pa_to_va_list [1048576];


// old logic
// int entries_num = 0 ;

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//==============================================
// [1] INITIALIZE KERNEL HEAP:
//==============================================
//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #0 kheap_init [GIVEN]
//Remember to initialize locks (if any)

void kheap_init()
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		initialize_dynamic_allocator(KERNEL_HEAP_START, KERNEL_HEAP_START + DYN_ALLOC_MAX_SIZE);
		set_kheap_strategy(KHP_PLACE_CUSTOMFIT);

		kheapPageAllocStart = dynAllocEnd + PAGE_SIZE;
		kheapPageAllocBreak = kheapPageAllocStart;

		init_kspinlock(&lock , "bla_bla");

		for (uint32 i = 0; i < 1048576; ++i)
			pa_to_va_list[i].va = 0;
	}
	//==================================================================================
	//==================================================================================
}

//==============================================
// [2] GET A PAGE FROM THE KERNEL FOR DA:
//==============================================
int get_page(void* va)
{
	int ret = alloc_page(ptr_page_directory, ROUNDDOWN((uint32)va, PAGE_SIZE), PERM_WRITEABLE, 1);
	if (ret < 0)
		panic("get_page() in kern: failed to allocate page from the kernel");

	uint32 pa = (uint32)kheap_physical_address((uint32)va) ;
	int index = (int)pa / PAGE_SIZE ;
	pa_to_va_list[index].va = (uint32)va;
	return 0;
}

//==============================================
// [3] RETURN A PAGE FROM THE DA TO KERNEL:
//==============================================
void return_page(void* va)
{
	unmap_frame(ptr_page_directory, ROUNDDOWN((uint32)va, PAGE_SIZE));
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
//===================================
// [1] ALLOCATE SPACE IN KERNEL HEAP:
//===================================


void* kmalloc(unsigned int size)
{
	//TODO: [PROJECT'25.GM#1] KERNEL HEAP - #2 kfree
	//Your code is here
	//Comment the following line
	//panic("kmalloc() is not implemented yet...!!");

	// ************************** //
	// ************************** //
#if USE_KHEAP
	void* block_address ;
	if(size <= DYN_ALLOC_MAX_BLOCK_SIZE)
	{
		block_address = alloc_block(size);
		if(block_address != NULL){
			int index ;
			uint32 va;
			va = ROUNDDOWN((uint32)block_address, PAGE_SIZE) ;
			index = kheap_physical_address((uint32)va) / PAGE_SIZE;
			pa_to_va_list[index].va = va;
			return block_address;
		}
		else{
			return NULL;
		}
	}
	else
	{
		unsigned int rounded_up_size = ROUNDUP(size,PAGE_SIZE);
		int required_num_of_pages = (rounded_up_size / PAGE_SIZE) , counter = 0 ,  max_free = -1 ;

		//cprintf("111111111111111111111111111111111111111\n");

		uint32 start_address = 0 , max_address = 0 ;

		//cprintf("2222222222222222222222222222222222222222\n");


		for(uint32 i = kheapPageAllocStart; i < kheapPageAllocBreak ; i += PAGE_SIZE)
		{
			uint32* page_table ;
			struct FrameInfo *fr =  get_frame_info(ptr_page_directory,i,&page_table);

			//cprintf("3333333333333333333333333333333333333\n");

			if(fr == NULL)
			{
				counter++;
				if(counter == 1)
				{
					start_address = i ;
				}
				// Worst Fit
				else if(counter > max_free)
				{
					max_free = counter ;
					max_address = start_address;
				}
			}
			else
			{
				// Exact Fit
				if(required_num_of_pages == counter)
				{
					acquire_kspinlock(&lock);

					int index = (start_address - KERNEL_HEAP_START) / PAGE_SIZE;


					for(int i = 1 ; i <= required_num_of_pages ; i++)
					{
						list_of_startVA_and_size[index].num_of_pages_after_me = required_num_of_pages - i;
						list_of_startVA_and_size[index].num_of_pages_before_me = i - 1;
						index++;
					}

					release_kspinlock(&lock);
					//entries_num++;


					//cprintf("444444444444444444444444444444444444\n");


					for(uint32 i = start_address; i < rounded_up_size + start_address ; i+=PAGE_SIZE){
						// uint32* page_table ;
						// struct FrameInfo *fr =  get_frame_info(ptr_page_directory,i,&page_table);
						// int ret = allocate_frame(&fr);
						// if(ret != 0)
						// {
						// 	cprintf("allocate_frame failed !! free_frames=%u\n", sys_calculate_free_frames());
						// 	return NULL;
						// }
						//cprintf("5555555555555555555555555555555555\n");
						//map_frame(ptr_page_directory, fr, i, PERM_WRITEABLE | PERM_PRESENT);
						//int pa_index = kheap_physical_address(i) / PAGE_SIZE ;
					    //pa_to_va_list[pa_index].va = i ;

						get_page((void*)i) ;
					}

					//cprintf("66666666666666666666666666666666666666\n");

					// ************************** //
					//release_kspinlock(&lock);
					// ************************** //

					return (void*)start_address ;
				}
				counter = 0;
				start_address = 0;
			}
		}
		if(max_free >= required_num_of_pages)
		{
			acquire_kspinlock(&lock);
			int index = (max_address - KERNEL_HEAP_START) / PAGE_SIZE;

			for(int i = 1 ; i <= required_num_of_pages ; i++)
			{
				list_of_startVA_and_size[index].num_of_pages_after_me = required_num_of_pages - i;
				list_of_startVA_and_size[index].num_of_pages_before_me = i - 1;
				index++;
			}
			release_kspinlock(&lock);
			// entries_num++;


			for(uint32 i = max_address ; i < max_address + rounded_up_size ; i+= PAGE_SIZE)
			{
				//cprintf("777777777777777777777777777777777777777777777\n");
				// uint32* page_table ;
				// struct FrameInfo *fr =  get_frame_info(ptr_page_directory,i,&page_table);
				// int ret = allocate_frame(&fr);
				// if(ret != 0)
				// {
				// 	cprintf("allocate_frame failed !! free_frames=%u\n", sys_calculate_free_frames());
				// 	return NULL;
				// }
				// //cprintf("9999999999999999999999999999999999999999999999999\n");

				// map_frame(ptr_page_directory, fr, i, PERM_WRITEABLE | PERM_PRESENT );

				get_page((void*)i) ;
			}


			int pa_index = kheap_physical_address(max_address) / PAGE_SIZE ;

			//pa_to_va_list[pa_index].va = max_address ;

			//cprintf("00000000000000000000000000000000000000000000000000000\n");


			// ************************** //
			// release_kspinlock(&lock);
			// ************************** //

			return (void*)max_address;
		}
		else if((kheapPageAllocBreak + rounded_up_size) <= KERNEL_HEAP_MAX && (kheapPageAllocBreak + rounded_up_size) > KERNEL_HEAP_START){

			start_address = kheapPageAllocBreak;

			for(uint32 i = kheapPageAllocBreak ; i < kheapPageAllocBreak + rounded_up_size ; i+= PAGE_SIZE)
			{
				// //cprintf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n");
				// uint32* page_table;
				// struct FrameInfo *fr =  get_frame_info(ptr_page_directory,i,&page_table);
				// int ret = allocate_frame(&fr);
				// //cprintf("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\n");

				// if(ret != 0)
				// {
				// 	cprintf("allocate_frame failed !! free_frames=%u\n", sys_calculate_free_frames());
				// 	return NULL;
				// }
				// //cprintf("cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc\n");

				// map_frame(ptr_page_directory, fr, i, PERM_WRITEABLE);
				// //cprintf("ddddddddddddddddddddddddddddddddddddddd\n");

				get_page((void*)i) ;

			}

			acquire_kspinlock(&lock) ;

			int index = (start_address - KERNEL_HEAP_START) / PAGE_SIZE;

			    int pa_index = kheap_physical_address(start_address) / PAGE_SIZE ;
			    //pa_to_va_list[pa_index].va = start_address ;



			for(int i = 1 ; i <= required_num_of_pages ; i++)
			{
				list_of_startVA_and_size[index].num_of_pages_after_me = required_num_of_pages - i;
				list_of_startVA_and_size[index].num_of_pages_before_me = i - 1;
				index++ ;
			}

			// entries_num++;

			kheapPageAllocBreak += rounded_up_size;

			release_kspinlock(&lock);

			return (void*)start_address ;
		}
		else{
			//cprintf("ggggggggggggggggggggggggggggggggggggggggggggggg\n");

			return NULL;
		}
	}
#endif

}

//=================================
// [2] FREE SPACE FROM KERNEL HEAP:
//=================================
void kfree(void* virtual_address)
{
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #2 kfree
	//Your code is here
	//Comment the following line
	//panic("kfree() is not implemented yet...!!");
#if USE_KHEAP

	acquire_kspinlock(&lock) ;

	virtual_address = ROUNDDOWN(virtual_address, PAGE_SIZE) ;


	if((uint32)virtual_address >= KERNEL_HEAP_START && (uint32)virtual_address <= dynAllocEnd)
	{
		int pa_index = kheap_physical_address((uint32)virtual_address) / PAGE_SIZE ;
		pa_to_va_list[pa_index].va = 0 ;

		free_block(virtual_address);
		release_kspinlock(&lock) ;
		return;
	}
	else if((uint32)virtual_address >= kheapPageAllocStart && (uint32)virtual_address < kheapPageAllocBreak)
	{
		//cprintf("%x\n",virtual_address);
		//virtual_address = (void*)ROUNDDOWN((uint32)virtual_address,PAGE_SIZE);

		//cprintf("%x\n",virtual_address);

		uint32 index = (uint32)((uint32)virtual_address - KERNEL_HEAP_START) / PAGE_SIZE ;

		//int pa_index = kheap_physical_address(virtual_address) / PAGE_SIZE ;
		//pa_to_va_list[pa_index].va = 0 ;

		//cprintf("%x\n",index);

		uint32 end_of_alloc = (uint32)virtual_address + (list_of_startVA_and_size[index].num_of_pages_after_me + list_of_startVA_and_size[index].num_of_pages_before_me + 1)* PAGE_SIZE;
		uint32 start_of_alloc = (uint32)virtual_address - (list_of_startVA_and_size[index].num_of_pages_before_me * PAGE_SIZE);

		if(end_of_alloc == kheapPageAllocBreak)
		{
			kheapPageAllocBreak = start_of_alloc;


			while(kheapPageAllocBreak > kheapPageAllocStart)
            {
                uint32 check_va = kheapPageAllocBreak - PAGE_SIZE;
                uint32* page_table;
                struct FrameInfo *fr = get_frame_info(ptr_page_directory, check_va, &page_table);

                if(fr == NULL)
                {
                    kheapPageAllocBreak -= PAGE_SIZE;
                }
                else
                {
                    break;
                }
            }
		}
		if(list_of_startVA_and_size[index].num_of_pages_after_me == 0 && list_of_startVA_and_size[index].num_of_pages_before_me == 0)
		{
			uint32* page_table ;
			struct FrameInfo *fr = get_frame_info(ptr_page_directory,(uint32)virtual_address,&page_table);

			int pa_index = kheap_physical_address((uint32)virtual_address) / PAGE_SIZE ;
			pa_to_va_list[pa_index].va = 0;

			free_frame(fr);

			unmap_frame(ptr_page_directory,(uint32) virtual_address);


			release_kspinlock(&lock);
			return ;
		}
		void* addr1 = virtual_address ;
		for(int i = 0 ; i <= list_of_startVA_and_size[index].num_of_pages_after_me ; i++)
		{
			uint32* page_table ;
			struct FrameInfo *fr =  get_frame_info(ptr_page_directory,(uint32)addr1,&page_table);

			int pa_idx = kheap_physical_address((uint32)addr1) / PAGE_SIZE;
			pa_to_va_list[pa_idx].va = 0;

			free_frame(fr);

			unmap_frame(ptr_page_directory, (uint32)addr1);

			addr1 += PAGE_SIZE;

		}
		for(int i = 0 ; i <= list_of_startVA_and_size[index].num_of_pages_after_me ;i++)
		{
			list_of_startVA_and_size[index+i].num_of_pages_after_me = 0 ;
			list_of_startVA_and_size[index+i].num_of_pages_before_me = 0 ;
		}

		void* addr2 = virtual_address - PAGE_SIZE;
		for(int i = list_of_startVA_and_size[index].num_of_pages_before_me ; i > 0 ;i--){
			uint32* page_table ;
			struct FrameInfo *fr =  get_frame_info(ptr_page_directory,(uint32)addr2,&page_table);


			int pa_idx = kheap_physical_address((uint32)addr2) / PAGE_SIZE;
			pa_to_va_list[pa_idx].va = 0;

			free_frame(fr);

			unmap_frame(ptr_page_directory,(uint32) addr2);


			addr2 -= PAGE_SIZE;

		}
		for(int i = list_of_startVA_and_size[index].num_of_pages_before_me ; i > 0 ;i--)
		{
			list_of_startVA_and_size[index-i].num_of_pages_after_me = 0 ;
			list_of_startVA_and_size[index-i].num_of_pages_before_me = 0 ;
		}
		release_kspinlock(&lock);
		return ;
	}
	else
	{
		release_kspinlock(&lock);
		panic("Invalid Virtual Address !!");
	}

	//release_kspinlock(&lock) ;
#endif


}

//=================================
// [3] FIND VA OF GIVEN PA:
//=================================
unsigned int kheap_virtual_address(unsigned int physical_address)
{
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #3 kheap_virtual_address
	//Your code is here
	//Comment the following line
#if USE_KHEAP

	uint32 index = (uint32)(ROUNDDOWN(physical_address,PAGE_SIZE) / PAGE_SIZE) ;
	//uint32 index = (uint32)physical_address / PAGE_SIZE ;
	uint32 va = (uint32)pa_to_va_list[index].va ;
	if(va == 0)
		return 0 ;

	va += PGOFF((uint32)physical_address) ;

	return va;
#endif

	/*EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED */
}

//=================================
// [4] FIND PA OF GIVEN VA:
//=================================
unsigned int kheap_physical_address(unsigned int virtual_address)
{
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #4 kheap_physical_address
	//Your code is here
	//Comment the following line
	// panic("kheap_physical_address() is not implemented yet...!!");
#if USE_KHEAP
	uint32* page_table ;
	struct FrameInfo *fr = get_frame_info(ptr_page_directory,(uint32)virtual_address,&page_table);

	if(fr == NULL)
	{
		return 0 ;
	}

	unsigned int pa = to_physical_address(fr) ;
	pa += PGOFF(virtual_address);

	return pa;
#endif

	/*EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED */
}

//=================================================================================//
//============================== BONUS FUNCTION ===================================//
//=================================================================================//
// krealloc():

//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to kmalloc().
//	A call with new_size = zero is equivalent to kfree().

extern __inline__ uint32 get_block_size(void *va);

void *krealloc(void *virtual_address, uint32 new_size)
{
	//TODO: [PROJECT'25.BONUS#2] KERNEL REALLOC - krealloc
	//Your code is here
	//Comment the following line
	panic("krealloc() is not implemented yet...!!");
}
