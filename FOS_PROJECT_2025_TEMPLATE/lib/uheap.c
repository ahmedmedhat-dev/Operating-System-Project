#include <inc/lib.h>

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

struct takenOrNot_and_size {
	bool taken;
	int num_of_pages;
	uint32 va;
};

struct takenOrNot_and_size list_of_takenOrNot_and_size[(USER_HEAP_MAX - USER_HEAP_START)/PAGE_SIZE];


//==============================================
// [1] INITIALIZE USER HEAP:
//==============================================
int __firstTimeFlag = 1;
void uheap_init()
{
	if(__firstTimeFlag)
	{
		initialize_dynamic_allocator(USER_HEAP_START, USER_HEAP_START + DYN_ALLOC_MAX_SIZE);
		uheapPlaceStrategy = sys_get_uheap_strategy();
		uheapPageAllocStart = dynAllocEnd + PAGE_SIZE;
		uheapPageAllocBreak = uheapPageAllocStart;

		__firstTimeFlag = 0;
	}

}

//==============================================
// [2] GET A PAGE FROM THE KERNEL FOR DA:
//==============================================
int get_page(void* va)
{
	int ret = __sys_allocate_page(ROUNDDOWN(va, PAGE_SIZE), PERM_USER|PERM_WRITEABLE|PERM_UHPAGE);
	if (ret < 0)
		panic("get_page() in user: failed to allocate page from the kernel");
	return 0;
}

//==============================================
// [3] RETURN A PAGE FROM THE DA TO KERNEL:
//==============================================
void return_page(void* va)
{
	int ret = __sys_unmap_frame(ROUNDDOWN((uint32)va, PAGE_SIZE));
	if (ret < 0)
		panic("return_page() in user: failed to return a page to the kernel");
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//=================================
// [1] ALLOCATE SPACE IN USER HEAP:
//=================================
void* malloc(uint32 size)
{
#if USE_KHEAP
    uheap_init();
    if (size == 0) return NULL;



    void* block_address;
    if(size <= DYN_ALLOC_MAX_BLOCK_SIZE)
    {
        block_address = alloc_block(size);
        return block_address;
    }
    else
    {
        unsigned int rounded_up_size = ROUNDUP(size,PAGE_SIZE);
        int required_num_of_pages = (int)(rounded_up_size / PAGE_SIZE);
        int counter = 0;
        int max_free = -1;
        uint32 start_address = 0;
        uint32 max_address = 0;

		//int start_index = (uheapPageAllocStart - USER_HEAP_START) / PAGE_SIZE , end_index = (uheapPageAllocBreak - USER_HEAP_START) / PAGE_SIZE ;

		//cprintf("array size = %d , required #pages = %d, size = %d , rounded up size = %d\n",ARRAY_LENGTH,required_num_of_pages, size,rounded_up_size) ;

        for(uint32 i = uheapPageAllocStart ; i < uheapPageAllocBreak ; i += PAGE_SIZE)
        {
			//cprintf("i = %d\n",i ) ;
			int index = (i - USER_HEAP_START) / PAGE_SIZE;

            if(list_of_takenOrNot_and_size[index].taken == 0)
            {
                counter++;
                if(counter == 1)
                {
					//cprintf("Counter = %d\n",counter);
                    start_address = i ;
                }

				// continue;
            }
			else
			{
				if(counter == required_num_of_pages)
				{
					for(uint32 j = start_address; j < start_address + rounded_up_size; j += PAGE_SIZE)
					{
						int index1 = (j - USER_HEAP_START) / PAGE_SIZE;
						list_of_takenOrNot_and_size[index1].taken = 1;
						list_of_takenOrNot_and_size[index1].num_of_pages = required_num_of_pages;
						list_of_takenOrNot_and_size[index1].va = start_address;
					}
					sys_allocate_user_mem(start_address, rounded_up_size);
					return (void*)start_address;
				}

				if(counter > max_free)
				{
					//cprintf("Counter = %d\n",counter);
					max_free = counter;
					//cprintf("max addrress 1 = %x\n",max_address);
					max_address = start_address;
					//cprintf("max addrress 2 = %x\n",max_address);

				}

				counter = 0;
				start_address = 0;
			}

        }

		// WORST FIT
        if(max_free >= required_num_of_pages)
        {
			// cprintf("worst fit --------------\n") ;
			// cprintf("max addrress = %x\n",max_address) ;

			int index ;

            for(uint32 i = max_address ; i < max_address + rounded_up_size; i += PAGE_SIZE)
            {
                index = (i - USER_HEAP_START) / PAGE_SIZE;
                list_of_takenOrNot_and_size[index].taken = 1;
                list_of_takenOrNot_and_size[index].num_of_pages = required_num_of_pages;
				list_of_takenOrNot_and_size[index].va = max_address;

				//max_address += PAGE_SIZE ;
            }
            sys_allocate_user_mem(max_address, rounded_up_size);

            return (void*)max_address;
        }
		// ****
        else if((uheapPageAllocBreak + rounded_up_size) < USER_HEAP_MAX && (uheapPageAllocBreak + rounded_up_size) > USER_HEAP_START)
        {
			//cprintf("uheapPageAllocBreak before update = %x \n",uheapPageAllocBreak) ;

            start_address = uheapPageAllocBreak;

            for(uint32 i = uheapPageAllocBreak; i < uheapPageAllocBreak + rounded_up_size; i += PAGE_SIZE)
            {
                int index = (i - USER_HEAP_START) / PAGE_SIZE;
                list_of_takenOrNot_and_size[index].taken = 1;
                list_of_takenOrNot_and_size[index].num_of_pages = required_num_of_pages;
				list_of_takenOrNot_and_size[index].va = start_address;
            }

            uheapPageAllocBreak += rounded_up_size;

			//cprintf("uheapPageAllocBreak after update = %x \n",uheapPageAllocBreak) ;

            sys_allocate_user_mem(start_address, rounded_up_size);

            return (void*)start_address;
        }
        else
        {
            return NULL;
        }
    }
#endif
}


//=================================
// [2] FREE SPACE FROM USER HEAP:
//=================================
void free(void* virtual_address)
{
#if USE_KHEAP
    //TODO: [PROJECT'25.IM#2] USER HEAP - #3 free
    virtual_address = ROUNDDOWN(virtual_address, PAGE_SIZE);
    //cprintf("virtual address : %x\n", virtual_address);

    if((uint32)virtual_address >= dynAllocStart && (uint32)virtual_address <= dynAllocEnd)
    {
        free_block(virtual_address);
        return;
    }
    else if((uint32)virtual_address >= uheapPageAllocStart && (uint32)virtual_address < uheapPageAllocBreak)
    {
        int found_index = -1;
        uint32 size = 0;
        uint32 va = 0;

        for(int i = 0; i < (int)((USER_HEAP_MAX - USER_HEAP_START)/PAGE_SIZE); i++)
        {
            if(list_of_takenOrNot_and_size[i].va == (uint32)virtual_address &&
               list_of_takenOrNot_and_size[i].taken == 1)
            {
                size = (uint32)(list_of_takenOrNot_and_size[i].num_of_pages * PAGE_SIZE);
                va = (uint32)(list_of_takenOrNot_and_size[i].va);
                found_index = i;
                break;
            }
        }
        //cprintf("va = %d\n",va);
        if(found_index == -1)
        {
			panic("Invalid Virtual Address!!");
        }

        sys_free_user_mem(va, size);

        int num_pages = list_of_takenOrNot_and_size[found_index].num_of_pages;
        for(int i = 0; i < num_pages; i++)
        {
            list_of_takenOrNot_and_size[found_index + i].taken = 0;
            list_of_takenOrNot_and_size[found_index + i].num_of_pages = 0;
            list_of_takenOrNot_and_size[found_index + i].va = 0;
        }

        uint32 end_of_alloc = va + size;
        //cprintf("break before = %d\n",uheapPageAllocBreak);
        if(end_of_alloc == uheapPageAllocBreak)
        {
            uheapPageAllocBreak = va;

            while(uheapPageAllocBreak > uheapPageAllocStart)
            {
                uint32 check_va = uheapPageAllocBreak - PAGE_SIZE;
                int check_index = (check_va - USER_HEAP_START) / PAGE_SIZE;

                if(list_of_takenOrNot_and_size[check_index].taken == 1)
                {
                    break;
                }

                uheapPageAllocBreak -= PAGE_SIZE;
            }
        }
        //cprintf("break after = %d\n",uheapPageAllocBreak);
    }
    else
    {
        panic("Invalid Virtual Address!!");
    }
#endif
}
//=================================
// [3] ALLOCATE SHARED VARIABLE:
//=================================

void* smalloc(char *sharedVarName, uint32 size, uint8 isWritable)
{
#if USE_KHEAP
    uheap_init();


    if (sharedVarName == NULL || *sharedVarName == '\0' || size == 0)
    {
        return NULL;
    }

    uint32 rounded_up_size = ROUNDUP(size, PAGE_SIZE);
    int required_num_of_pages = (int)(rounded_up_size / PAGE_SIZE);


    int exact_fit_index = -1;
    int worst_fit_index = -1;
    int worst_fit_size = 0;


    for(uint32 va = uheapPageAllocStart; va < uheapPageAllocBreak; va += PAGE_SIZE)
    {
        int index = (va - USER_HEAP_START) / PAGE_SIZE;

        if(list_of_takenOrNot_and_size[index].taken == 1)
        {
            continue;
        }


        int free_count = 0;
        uint32 free_start_va = va;
        int free_start_index = index;

        while(va < uheapPageAllocBreak)
        {
            index = (va - USER_HEAP_START) / PAGE_SIZE;
            if(list_of_takenOrNot_and_size[index].taken == 1)
            {
                break;
            }
            free_count++;
            va += PAGE_SIZE;
        }


        if(free_count == required_num_of_pages)
        {
            exact_fit_index = free_start_index;
            break;
        }


        if(free_count >= required_num_of_pages && free_count > worst_fit_size)
        {
            worst_fit_size = free_count;
            worst_fit_index = free_start_index;
        }

        va -= PAGE_SIZE;
    }

    uint32 alloc_va = 0;

    if(exact_fit_index != -1)
    {
        alloc_va = USER_HEAP_START + (exact_fit_index * PAGE_SIZE);
    }

    else if(worst_fit_index != -1)
    {
        alloc_va = USER_HEAP_START + (worst_fit_index * PAGE_SIZE);
    }

    else
    {
        if(uheapPageAllocBreak + rounded_up_size > USER_HEAP_MAX)
            return NULL;

        alloc_va = uheapPageAllocBreak;
        uheapPageAllocBreak += rounded_up_size;
    }


    int ret = sys_create_shared_object(sharedVarName, size, isWritable, (void*)alloc_va);

    if(ret < 0)
    {

        if(alloc_va + rounded_up_size == uheapPageAllocBreak)
        {
            uheapPageAllocBreak = alloc_va;
        }
        return NULL;
    }


    int start_index = (alloc_va - USER_HEAP_START) / PAGE_SIZE;
    for(int i = 0; i < required_num_of_pages; i++)
    {
        list_of_takenOrNot_and_size[start_index + i].taken = 1;
        list_of_takenOrNot_and_size[start_index + i].num_of_pages = required_num_of_pages;
        list_of_takenOrNot_and_size[start_index + i].va = alloc_va;
    }

    return (void*)alloc_va;
#endif
}


//========================================
// [4] SHARE ON ALLOCATED SHARED VARIABLE:
//========================================
void* sget(int32 ownerEnvID, char *sharedVarName)
{
#if USE_KHEAP
    uheap_init();


    if (sharedVarName == NULL || *sharedVarName == '\0')
    {
        return NULL;
    }


    int size = sys_size_of_shared_object(ownerEnvID, sharedVarName);
    if(size <= 0)
    {
        return NULL;
    }

    uint32 rounded_up_size = ROUNDUP(size, PAGE_SIZE);
    int required_num_of_pages = (int)(rounded_up_size / PAGE_SIZE);


    int exact_fit_index = -1;
    int worst_fit_index = -1;
    int worst_fit_size = 0;


    for(uint32 va = uheapPageAllocStart; va < uheapPageAllocBreak; va += PAGE_SIZE)
    {
        int index = (va - USER_HEAP_START) / PAGE_SIZE;

        if(list_of_takenOrNot_and_size[index].taken == 1)
        {
            continue;
        }


        int free_count = 0;
        uint32 free_start_va = va;
        int free_start_index = index;

        while(va < uheapPageAllocBreak)
        {
            index = (va - USER_HEAP_START) / PAGE_SIZE;
            if(list_of_takenOrNot_and_size[index].taken == 1)
            {
                break;
            }
            free_count++;
            va += PAGE_SIZE;
        }


        if(free_count == required_num_of_pages)
        {
            exact_fit_index = free_start_index;
            break;
        }


        if(free_count >= required_num_of_pages && free_count > worst_fit_size)
        {
            worst_fit_size = free_count;
            worst_fit_index = free_start_index;
        }

        va -= PAGE_SIZE;
    }

    uint32 alloc_va = 0;


    if(exact_fit_index != -1)
    {
        alloc_va = USER_HEAP_START + (exact_fit_index * PAGE_SIZE);
    }

    else if(worst_fit_index != -1)
    {
        alloc_va = USER_HEAP_START + (worst_fit_index * PAGE_SIZE);
    }

    else
    {
        if(uheapPageAllocBreak + rounded_up_size > USER_HEAP_MAX)
            return NULL;

        alloc_va = uheapPageAllocBreak;
        uheapPageAllocBreak += rounded_up_size;
    }

    int ret = sys_get_shared_object(ownerEnvID, sharedVarName, (void*)alloc_va);

    if(ret < 0)
    {

        if(alloc_va + rounded_up_size == uheapPageAllocBreak)
        {
            uheapPageAllocBreak = alloc_va;
        }
        return NULL;
    }

    int start_index = (alloc_va - USER_HEAP_START) / PAGE_SIZE;
    for(int i = 0; i < required_num_of_pages; i++)
    {
        list_of_takenOrNot_and_size[start_index + i].taken = 1;
        list_of_takenOrNot_and_size[start_index + i].num_of_pages = required_num_of_pages;
        list_of_takenOrNot_and_size[start_index + i].va = alloc_va;
    }

    return (void*)alloc_va;
#endif

}


//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//


//=================================
// REALLOC USER SPACE:
//=================================
//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to malloc().
//	A call with new_size = zero is equivalent to free().

//  Hint: you may need to use the sys_move_user_mem(...)
//		which switches to the kernel mode, calls move_user_mem(...)
//		in "kern/mem/chunk_operations.c", then switch back to the user mode here
//	the move_user_mem() function is empty, make sure to implement it.
void *realloc(void *virtual_address, uint32 new_size)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	//==============================================================
	panic("realloc() is not implemented yet...!!");
}


//=================================
// FREE SHARED VARIABLE:
//=================================
//	This function frees the shared variable at the given virtual_address
//	To do this, we need to switch to the kernel, free the pages AND "EMPTY" PAGE TABLES
//	from main memory then switch back to the user again.
//
//	use sys_delete_shared_object(...); which switches to the kernel mode,
//	calls delete_shared_object(...) in "shared_memory_manager.c", then switch back to the user mode here
//	the delete_shared_object() function is empty, make sure to implement it.
void sfree(void* virtual_address)
{
	//TODO: [PROJECT'25.BONUS#5] EXIT #2 - sfree
	//Your code is here
	//Comment the following line
	panic("sfree() is not implemented yet...!!");

	//	1) you should find the ID of the shared variable at the given address
	//	2) you need to call sys_freeSharedObject()
}


//==================================================================================//
//========================== MODIFICATION FUNCTIONS ================================//
//==================================================================================//
