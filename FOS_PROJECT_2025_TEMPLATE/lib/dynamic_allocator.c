#include <inc/assert.h>
#include <inc/string.h>
#include "../inc/dynamic_allocator.h"

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//
//==================================
//==================================
// [1] GET PAGE VA:
//==================================

__inline__ uint32 to_page_va(struct PageInfoElement *ptrPageInfo)
{
	if (ptrPageInfo < &pageBlockInfoArr[0] || ptrPageInfo >= &pageBlockInfoArr[DYN_ALLOC_MAX_SIZE/PAGE_SIZE])
			panic("to_page_va called with invalid pageInfoPtr");
	//Get start VA of the page from the corresponding Page Info pointer
	int idxInPageInfoArr = (ptrPageInfo - pageBlockInfoArr);
	return dynAllocStart + (idxInPageInfoArr << PGSHIFT);
}

//==================================
// [2] GET PAGE INFO OF PAGE VA:
//==================================
__inline__ struct PageInfoElement * to_page_info(uint32 va)
{
	int idxInPageInfoArr = (va - dynAllocStart) >> PGSHIFT;
	if (idxInPageInfoArr < 0 || idxInPageInfoArr >= DYN_ALLOC_MAX_SIZE/PAGE_SIZE)
		panic("to_page_info called with invalid pa");
	return &pageBlockInfoArr[idxInPageInfoArr];
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//==================================
// [1] INITIALIZE DYNAMIC ALLOCATOR:
//==================================
bool is_initialized = 0;

void initialize_dynamic_allocator(uint32 daStart, uint32 daEnd)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert(daEnd <= daStart + DYN_ALLOC_MAX_SIZE);
		is_initialized = 1;
	}
	//==================================================================================
	//==================================================================================
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #1 initialize_dynamic_allocator
	//Your code is here
	//Comment the following line
	//panic("initialize_dynamic_allocator() Not implemented yet");
	dynAllocStart = daStart;
	dynAllocEnd   = daEnd;


	for (int i = 0; i < (LOG2_MAX_SIZE - LOG2_MIN_SIZE + 1); i++) {
		LIST_INIT(&freeBlockLists[i]);
	}

	LIST_INIT(&freePagesList);


	uint32 NumOfPages = (daEnd - daStart) / PAGE_SIZE;
	for (uint32 i = 0; i < NumOfPages; i++) {
		pageBlockInfoArr[i].block_size = 0;
		pageBlockInfoArr[i].num_of_free_blocks = 0;

	}

	for (uint32 i = 0; i < NumOfPages; i++) {
		LIST_INSERT_TAIL(&freePagesList, &pageBlockInfoArr[i]);
	}
}


//===========================
// [2] GET BLOCK SIZE:
//===========================

__inline__ uint32 get_block_size(void *va)
{
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #2 get_block_size
	//Your code is here
	//Comment the following line

	struct PageInfoElement *p_element = to_page_info((uint32) va) ;

	return (uint32) p_element->block_size;
}

//===========================
// 3) ALLOCATE BLOCK:
//===========================
void* alloc_block(uint32 size)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert(size <= DYN_ALLOC_MAX_BLOCK_SIZE);
	}
	//==================================================================================
	//==================================================================================
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #3 alloc_block
	//Your code is here
	//Comment the following line

	// **************************************
	// when indexing issue in freeblocklists (by log2 instead of accessing directly by size)
	// return the start va
	// **************************************

	if(size == 0)
		return NULL ;

	if(DYN_ALLOC_MIN_BLOCK_SIZE > size)
		size = DYN_ALLOC_MIN_BLOCK_SIZE ;

	uint32 rounded_up_size = DYN_ALLOC_MIN_BLOCK_SIZE;
	while (rounded_up_size < size)
		rounded_up_size *= 2;

	//cprintf("size = %u, rounded_up_size = %u\n", size, rounded_up_size);


	// for(int i = 8 ; i < 2048 ; i*=2)
	// {
	// 	cprintf("%d" , freeBlockLists[i].size);
	// }

	uint32 outer_index = __builtin_ctz(rounded_up_size) ;
	outer_index -= 3 ;

	// uint32 outer_index = 0 ;

	// int temp_size = rounded_up_size ;

	// while(temp_size > 1)
	// {
	// 	temp_size /= 2;
	// 	outer_index++ ;
	// }

	// outer_index -= 3;

	int num_of_free_blocks = LIST_SIZE(&freeBlockLists[outer_index]);

	if(num_of_free_blocks == 0)
	{
		int num_of_free_pages = LIST_SIZE(&freePagesList);

		if(num_of_free_pages > 0)
		{
			struct PageInfoElement *pg = LIST_FIRST(&freePagesList);
			LIST_REMOVE(&freePagesList,pg) ;

			uint32 va = to_page_va(pg) ;

			int ret = get_page((void*)va);

			if(ret != 0)
				panic("Failed to allocate page");

			pg->block_size = rounded_up_size ;
			pg->num_of_free_blocks = (PAGE_SIZE / pg->block_size);

			uint32 index2 = (va - dynAllocStart) / PAGE_SIZE ;

			pageBlockInfoArr[index2].num_of_free_blocks = pg->num_of_free_blocks;
			pageBlockInfoArr[index2].block_size = pg->block_size;

			// add to free block list

			for(uint32 block = 0; block < PAGE_SIZE ; block += rounded_up_size)
			{
				struct BlockElement *new_block = (struct BlockElement*)(va + block);
				LIST_INSERT_TAIL(&freeBlockLists[outer_index], new_block);
			}

			struct BlockElement *blk = LIST_FIRST(&freeBlockLists[outer_index]);
			LIST_REMOVE(&freeBlockLists[outer_index], blk) ;

			pageBlockInfoArr[index2].num_of_free_blocks -= 1;

			//va = (uint32) blk ;

			return (void*)va;
		}
		else
		{
			for(int i = rounded_up_size * 2; i <= DYN_ALLOC_MAX_BLOCK_SIZE ; i *= 2)
			{
				uint32 index1 = __builtin_ctz(i) ;

				index1 -= 3;

				// uint32 index1 = 0 ;

				// int temp_size = rounded_up_size ;

				// while(temp_size > 1)
				// {
				// 	temp_size /= 2;
				// 	index1++ ;
				// }

				// index1 -= 3;

				int num_of_next_free_blocks = LIST_SIZE(&freeBlockLists[index1]);

				if(num_of_next_free_blocks == 0)
				{
					continue;
				}
				struct BlockElement *blk = LIST_FIRST(&freeBlockLists[index1]);
				LIST_REMOVE(&freeBlockLists[index1], blk) ;

				void* va = (void*) blk ;

				uint32 index2 = (uint32)(va - dynAllocStart) / PAGE_SIZE ;
				// if(pageBlockInfoArr[index2].num_of_free_blocks == NULL || pageBlockInfoArr[index2].block_size == NULL)
				// {
				// 	pageBlockInfoArr[index2].block_size = rounded_up_size ;
				// 	pageBlockInfoArr[index2].num_of_free_blocks = PAGE_SIZE / rounded_up_size - 1;
				// }
				pageBlockInfoArr[index2].num_of_free_blocks -= 1;

				// struct PageInfoElement* page = to_page_info(va) ;

				return va ;
			}
			// panic if the next size doesn't have free blocks from the next size
			panic("No Suitable Block Found.");
		}
	}
	else
	{
		uint32 index1 = __builtin_ctz(rounded_up_size) ;

		index1 -= 3;

		// uint32 index1 = 0 ;

		// int temp_size = rounded_up_size ;

		// while(temp_size > 1)
		// {
		// 	temp_size /= 2;
		// 	index1++ ;
		// }

		// index1 -= 3;

		struct BlockElement *blk = LIST_FIRST(&freeBlockLists[index1]);
		// cprintf("%x", (uint32)blk) ;
		LIST_REMOVE(&freeBlockLists[index1], blk) ;

		void* va = (void*) blk ;

		//page->num_of_free_blocks -= 1;

		uint32 index2 = (uint32)(va - dynAllocStart) / PAGE_SIZE;

		// if(pageBlockInfoArr[index2].num_of_free_blocks == NULL || pageBlockInfoArr[index2].block_size == NULL)
		// {
		// 	pageBlockInfoArr[index2].block_size = rounded_up_size ;


		// 	pageBlockInfoArr[index2].num_of_free_blocks = PAGE_SIZE / rounded_up_size - 1;
		// }
		pageBlockInfoArr[index2].num_of_free_blocks -= 1;

		// struct PageInfoElement* page = to_page_info(va) ;

		return va ;

	}

	//TODO: [PROJECT'25.BONUS#1] DYNAMIC ALLOCATOR - block if no free block
}

//===========================
// [4] FREE BLOCK:
//===========================
void free_block(void *va)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert((uint32)va >= dynAllocStart && (uint32)va < dynAllocEnd);
	}
	//==================================================================================
	//==================================================================================

	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #4 free_block
	//Your code is here
	//Comment the following line

	int index_for_page_arr = (uint32) (va - dynAllocStart) / PAGE_SIZE ;

	//int offset = (uint32) va % PAGE_SIZE ;

	//uint32 block_size = pageBlockInfoArr[index_for_page_arr].block_size ;

	uint32 block_size = get_block_size(va) ;

	//int block_num = (offset / block_size);


	int index_for_free_blocks_lists = __builtin_ctz(block_size) ;

	index_for_free_blocks_lists -= 3;

	//cprintf("%u",freeBlockLists[index_for_free_blocks_lists].size);

	LIST_INSERT_TAIL(&freeBlockLists[index_for_free_blocks_lists],(struct BlockElement *)(va)) ;

	// freeBlockLists[index_for_free_blocks_lists].size += 1 ;

	//cprintf("%u",freeBlockLists[index_for_free_blocks_lists].size);

	pageBlockInfoArr[index_for_page_arr].num_of_free_blocks++ ;

	if(pageBlockInfoArr[index_for_page_arr].num_of_free_blocks == (PAGE_SIZE / block_size))
	{

		// uint32* page_table ;
		// struct FrameInfo *fr =  get_frame_info(ptr_page_directory,(uint32)va,&page_table);

		// free_frame(fr);

		// unmap_frame(ptr_page_directory,(uint32) va);


		// for(uint32 i = 0 ; i < PAGE_SIZE ; i += block_size)
		// {
		// 	struct BlockElement *blk = (struct BlockElement *)((uint32)va + i);
		// 	struct BlockElement *iterator ;
		// 	// cprintf("%x", (uint32)blk) ;
		// 	LIST_FOREACH(iterator,&freeBlockLists[index_for_free_blocks_lists])
		// 	{
		// 		if(ROUNDDOWN(va,PAGE_SIZE) <= blk && blk < ROUNDDOWN(iterator,PAGE_SIZE) + PAGE_SIZE)
		// 		{
		// 			LIST_REMOVE(&freeBlockLists[index_for_free_blocks_lists],blk);
		// 		}
		// 	}
		// }

		uint32 rounded_down_va = ROUNDDOWN((uint32)va,PAGE_SIZE);

		struct BlockElement *iterator ;

		LIST_FOREACH(iterator,&freeBlockLists[index_for_free_blocks_lists]){
			if(rounded_down_va <= (uint32)iterator && (uint32)iterator < rounded_down_va + PAGE_SIZE)
			{
				LIST_REMOVE(&freeBlockLists[index_for_free_blocks_lists], iterator) ;
			}
		}

		return_page((void*)rounded_down_va) ;

		pageBlockInfoArr[index_for_page_arr].block_size = 0;
		pageBlockInfoArr[index_for_page_arr].num_of_free_blocks = 0;

		struct PageInfoElement *pg = &pageBlockInfoArr[index_for_page_arr];
		LIST_INSERT_TAIL(&freePagesList,pg);


	}

	// freeBlockLists[block_size]. ;
}

// ================================================================================== //
// ============================== BONUS FUNCTIONS =================================== //
// ================================================================================== //

//===========================
// [1] REALLOCATE BLOCK:
//===========================
void *realloc_block(void* va, uint32 new_size)
{
	//TODO: [PROJECT'25.BONUS#2] KERNEL REALLOC - realloc_block
	//Your code is here
	//Comment the following line
	panic("realloc_block() Not implemented yet");
}
