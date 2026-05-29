#include <inc/memlayout.h>
#include "shared_memory_manager.h"

#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/queue.h>
#include <inc/environment_definitions.h>

#include <kern/proc/user_environment.h>
#include <kern/trap/syscall.h>
#include "kheap.h"
#include "memory_manager.h"

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//===========================
// [1] INITIALIZE SHARES:
//===========================
//Initialize the list and the corresponding lock
void sharing_init()
{
#if USE_KHEAP
	LIST_INIT(&AllShares.shares_list) ;
	init_kspinlock(&AllShares.shareslock, "shares lock");
	//init_sleeplock(&AllShares.sharessleeplock, "shares sleep lock");
#else
	panic("not handled when KERN HEAP is disabled");
#endif
}

//=========================
// [2] Find Share Object:
//=========================
//Search for the given shared object in the "shares_list"
//Return:
//	a) if found: ptr to Share object
//	b) else: NULL
struct Share* find_share(int32 ownerID, char* name)
{
#if USE_KHEAP
	struct Share * ret = NULL;
	bool wasHeld = holding_kspinlock(&(AllShares.shareslock));
	if (!wasHeld)
	{
		acquire_kspinlock(&(AllShares.shareslock));
	}
	{
		struct Share * shr ;
		LIST_FOREACH(shr, &(AllShares.shares_list))
		{
			//cprintf("shared var name = %s compared with %s\n", name, shr->name);
			if(shr->ownerID == ownerID && strcmp(name, shr->name)==0)
			{
				//cprintf("%s found\n", name);
				ret = shr;
				break;
			}
		}
	}
	if (!wasHeld)
	{
		release_kspinlock(&(AllShares.shareslock));
	}
	return ret;
#endif
}

//==============================
// [3] Get Size of Share Object:
//==============================
int size_of_shared_object(int32 ownerID, char* shareName)
{
	// This function should return the size of the given shared object
	// RETURN:
	//	a) If found, return size of shared object
	//	b) Else, return E_SHARED_MEM_NOT_EXISTS
	//
	struct Share* ptr_share = find_share(ownerID, shareName);
	if (ptr_share == NULL)
		return E_SHARED_MEM_NOT_EXISTS;
	else
		return ptr_share->size;

	return 0;
}
//===========================================================


//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//=====================================
// [1] Alloc & Initialize Share Object:
//=====================================
//Allocates a new shared object and initialize its member
//It dynamically creates the "framesStorage"
//Return: allocatedObject (pointer to struct Share) passed by reference
//=====================================
// [1] Alloc & Initialize Share Object:
//=====================================
struct Share* alloc_share(int32 ownerID, char* shareName, uint32 size, uint8 isWritable)
{
#if USE_KHEAP
	struct Share* new_share = NULL;
	uint32 num_pages;


	if (shareName == NULL || *shareName == '\0') {
		return NULL;
	}

	if (size == 0) {
		return NULL;
	}


	new_share = (struct Share*)kmalloc(sizeof(struct Share));
	if (new_share == NULL) {
		return NULL;
	}

	memset(new_share, 0, sizeof(struct Share));

	new_share->ownerID = ownerID;


	int name_len = strlen(shareName);
	if (name_len > 63) {
		name_len = 63;
	}
	strncpy(new_share->name, shareName, name_len);
	new_share->name[name_len] = '\0';

	new_share->size = size;
	new_share->isWritable = isWritable;
	new_share->references = 1;
	new_share->ID = 0;


	num_pages = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;


	new_share->framesStorage = (struct FrameInfo **) kmalloc(num_pages * sizeof(struct FrameInfo *));

	if (new_share->framesStorage == NULL) {
		kfree(new_share);
		return NULL;
	}


	for (uint32 i = 0; i < num_pages; i++) {
		new_share->framesStorage[i] = NULL;
	}

	return new_share;
#endif
}


//=========================
// [4] Create Share Object:
//=========================
int create_shared_object(int32 ownerID, char* shareName, uint32 size, uint8 isWritable, void* virtual_address)
{
#if USE_KHEAP
	struct Env* owner_env = NULL;
	struct Share* share = NULL;
	uint32 num_pages;
	uint32 va;
	int perm;


	if (shareName == NULL || *shareName == '\0' || size == 0 || virtual_address == NULL) {
		return E_NO_SHARE;
	}


	if ((uint32)virtual_address % PAGE_SIZE != 0) {
		return E_NO_SHARE;
	}


	if (envid2env(ownerID, &owner_env, 0) < 0) {
		return E_NO_SHARE;
	}


	acquire_kspinlock(&AllShares.shareslock);


	if (find_share(ownerID, shareName) != NULL) {
		release_kspinlock(&AllShares.shareslock);
		return E_SHARED_MEM_EXISTS;
	}


	share = alloc_share(ownerID, shareName, size, isWritable);
	if (share == NULL) {
		release_kspinlock(&AllShares.shareslock);
		return E_NO_SHARE;
	}


	share->ID = (int32)virtual_address & 0x7FFFFFFF;


	num_pages = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;


	perm = PERM_PRESENT | PERM_USER | PERM_UHPAGE;
	if (isWritable) {
		perm |= PERM_WRITEABLE;
	}


	va = (uint32)virtual_address;
	uint32 i;
	for (i = 0; i < num_pages; i++) {
		struct FrameInfo* frame = NULL;


		if (allocate_frame(&frame) != 0) {
			for (uint32 j = 0; j < i; j++) {
					if (share->framesStorage[j] != NULL) {
						unmap_frame(owner_env->env_page_directory,
								   (uint32)virtual_address + j * PAGE_SIZE);
					}
				}

				if (share->framesStorage != NULL) {
					kfree(share->framesStorage);
				}
				kfree(share);

				release_kspinlock(&AllShares.shareslock);
				return E_NO_SHARE;
		}


		share->framesStorage[i] = frame;


		if (map_frame(owner_env->env_page_directory, frame, va, perm) != 0) {
			for (uint32 j = 0; j < i; j++) {
					if (share->framesStorage[j] != NULL) {
						unmap_frame(owner_env->env_page_directory,
								   (uint32)virtual_address + j * PAGE_SIZE);
					}
				}

				if (share->framesStorage != NULL) {
					kfree(share->framesStorage);
				}
				kfree(share);

				release_kspinlock(&AllShares.shareslock);
				return E_NO_SHARE;
		}

		va += PAGE_SIZE;
	}

	LIST_INSERT_HEAD(&AllShares.shares_list, share);

	release_kspinlock(&AllShares.shareslock);
	return share->ID;
#endif
}


//======================
// [5] Get Share Object:
//======================
int get_shared_object(int32 ownerID, char* shareName, void* virtual_address)
{
#if USE_KHEAP
	struct Env* myenv = get_cpu_proc();
	struct Share* share = NULL;
	uint32 num_pages;
	uint32 va;
	int perm;

	// Validate input
	if (shareName == NULL || *shareName == '\0' || virtual_address == NULL) {
		return E_SHARED_MEM_NOT_EXISTS;
	}

	// Check page alignment
	if ((uint32)virtual_address % PAGE_SIZE != 0) {
		return E_SHARED_MEM_NOT_EXISTS;
	}

	// Acquire lock
	acquire_kspinlock(&AllShares.shareslock);

	// Find the share
	share = find_share(ownerID, shareName);
	if (share == NULL) {
		release_kspinlock(&AllShares.shareslock);
		return E_SHARED_MEM_NOT_EXISTS;
	}

	// Calculate number of pages
	num_pages = ROUNDUP(share->size, PAGE_SIZE) / PAGE_SIZE;

	// Set permissions based on share's writable flag
	perm = PERM_PRESENT | PERM_USER | PERM_UHPAGE;
	if (share->isWritable) {
		perm |= PERM_WRITEABLE;
	}

	// Map the frames to current environment
	va = (uint32)virtual_address;
	uint32 i;
	for (i = 0; i < num_pages; i++) {
		struct FrameInfo* frame = share->framesStorage[i];

		if (frame == NULL) {
			// Frame should exist
			goto cleanup_mappings;
		}

		// Map the frame (map_frame handles reference counting)
		if (map_frame(myenv->env_page_directory, frame, va, perm) != 0) {
			// Mapping failed
			goto cleanup_mappings;
		}

		va += PAGE_SIZE;
	}

	// Increment share references
	share->references++;

	release_kspinlock(&AllShares.shareslock);
	return share->ID;

cleanup_mappings:
	// Clean up any mappings we created
	for (uint32 j = 0; j < i; j++) {
		unmap_frame(myenv->env_page_directory,
				   (uint32)virtual_address + j * PAGE_SIZE);
	}

	release_kspinlock(&AllShares.shareslock);
	return E_SHARED_MEM_NOT_EXISTS;
#endif
}
//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//
//=========================
// [1] Delete Share Object:
//=========================
//delete the given shared object from the "shares_list"
//it should free its framesStorage and the share object itself
void free_share(struct Share* ptrShare)
{
	//TODO: [PROJECT'25.BONUS#5] EXIT #2 - free_share
	//Your code is here
	//Comment the following line
	panic("free_share() is not implemented yet...!!");
}


//=========================
// [2] Free Share Object:
//=========================
int delete_shared_object(int32 sharedObjectID, void *startVA)
{
	//TODO: [PROJECT'25.BONUS#5] EXIT #2 - delete_shared_object
	//Your code is here
	//Comment the following line
	panic("delete_shared_object() is not implemented yet...!!");

	struct Env* myenv = get_cpu_proc(); //The calling environment

	// This function should free (delete) the shared object from the User Heapof the current environment
	// If this is the last shared env, then the "frames_store" should be cleared and the shared object should be deleted
	// RETURN:
	//	a) 0 if success
	//	b) E_SHARED_MEM_NOT_EXISTS if the shared object is not exists

	// Steps:
	//	1) Get the shared object from the "shares" array (use get_share_object_ID())
	//	2) Unmap it from the current environment "myenv"
	//	3) If one or more table becomes empty, remove it
	//	4) Update references
	//	5) If this is the last share, delete the share object (use free_share())
	//	6) Flush the cache "tlbflush()"

}
