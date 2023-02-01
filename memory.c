#include <errno.h>
#include <stddef.h>
#include "memory.h"
#include "bitset.h"

/**@brief Order of the largest possible memory block. */
#define ORDER_MAX 10

/**@brief Size of the smallest possible memory block. */
#define PAGE_SIZE 64

/**@brief Size of available memory. */
#define HEAP_SIZE (PAGE_SIZE << ORDER_MAX)

/**	Stores memory blocks as doubly-linked list */
typedef struct block_s
{
	size_t order;
	struct block_s *prev;
	struct block_s *next;
} block_t;

/**	Points to linked list of a certain order - use of struct is legacy */
typedef struct free_area_struct
{
	block_t *free_list;
} free_area_t;

/**@brief Heap memory. */
static char heap[HEAP_SIZE];

/** Array that maintains linked lists per order */
static free_area_t free_area_struct_array[ORDER_MAX + 1];

/** One bit per buddy pair. If toggled then buddies in different states. Otherwise both free or blocked */
static Bitset map[(1 << ORDER_MAX) - 1];

/** taken from bit-twiddling hacks. Computes binary log (not quite -1 for powers of two) */
int logbin(size_t size)
{
	static const int MultiplyBitPosition[32] = {
		0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30,
		8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31};
	size--;
	size |= size >> 1;
	size |= size >> 2;
	size |= size >> 4;
	size |= size >> 8;
	size |= size >> 16;

	return MultiplyBitPosition[(u_int32_t)(size * 0x07C4ACDDU) >> 27];
}

/** given the size of a memory block - return its order */
int get_order(size_t size)
{
	return logbin(size) - logbin(PAGE_SIZE);
}

/** given a mem block - return its size */
int get_block_size(block_t *block)
{
	return (1 << (block->order)) * PAGE_SIZE;
}

/** given a mem block - return its block number within the order */
int get_page_number(block_t *block)
{
	ptrdiff_t offset = (char *)block - heap;
	return (offset / get_block_size(block));
}

/** given a mem block - return the corresponding index in the bitmap */
int get_gitmap_idx(block_t *block)
{
	int page_number = get_page_number(block);
	return page_number + (1 << (ORDER_MAX - block->order)) - 1;
}

/** given a mem block - return the corresponding index of its parent in the bitmap */
int get_gitmap_idx_parent(block_t *block)
{
	int s = get_block_size(block);
	int uneven = get_page_number(block) % 2;
	block_t *parent = (block_t *)(uneven ? ((char *)block - s) : ((char *)block));
	ptrdiff_t offset = (char *)parent - heap;
	int page_number = (offset / ((1 << (block->order + 1)) * PAGE_SIZE));
	return page_number + (1 << (ORDER_MAX - (block->order + 1))) - 1;
}

/** toggle the corresponding bit for a block - i.e. flip its value */
void toggle(block_t *block)
{
	int bit_map_idx = get_gitmap_idx(block);
	bitsetFlip(&map[0], bit_map_idx);
}

/** toggle the corresponding bit for the parent of a block - i.e. flip its value */
void toggle_parent(block_t *block)
{
	int bit_map_idx = get_gitmap_idx_parent(block);
	bitsetFlip(&map[0], bit_map_idx);
}

/** insert mem block in the linked list of its order */
void insert(block_t *block)
{
	block_t *former_head = free_area_struct_array[block->order].free_list;
	if (former_head != NULL)
	{
		former_head->prev = block;
		block->next = former_head;
		block->prev = NULL;
	}
	else
	{
		block->next = NULL;
		block->prev = NULL;
	}
	free_area_struct_array[block->order].free_list = block;
}

/** delete mem block from the linked list of its order */
void delete(block_t *block)
{
	if ((block->prev) != NULL)
	{
		block_t *prev = block->prev;
		if (block->next != NULL)
		{
			block_t *next = block->next;

			prev->next = next;
			next->prev = prev;
		}
		else
		{
			prev->next = NULL;
		}
	}
	else
	{
		if (block->next != NULL)
		{
			block_t *next = block->next;
			next->prev = NULL;
			free_area_struct_array[block->order].free_list = next;
		}
		else
		{
			free_area_struct_array[block->order].free_list = NULL;
		}
	}
}

/** initialize the data structures for the allocator */
void mem_init()
{
	// Initialize bit set to all zeros
	bitsetInit(&map[0], (1 << ORDER_MAX) - 1, 0);

	// Initialize all linked lists to empty
	for (int i = 0; i <= ORDER_MAX; i++)
	{
		if (i != ORDER_MAX)
		{
			(&free_area_struct_array[i])->free_list = NULL;
		}
		else
		{
			// except for the highest order where we have on block with all the heap mem
			block_t *node = (block_t *)heap;
			node->prev = NULL;
			node->next = NULL;
			node->order = ORDER_MAX;
			(&free_area_struct_array[i])->free_list = node;
		}
	}
}

/** splits a block recursively until block of proper order is found */
block_t *split_block(block_t *block, int order_req)
{
	// return block when we have reached the proper order
	if (block->order == order_req)
	{
		return block;
	}
	int block_size = get_block_size(block);

	// Toggle - block becomes busy
	toggle(block);

	// Compute adresses of the children
	block_t *left = (block_t *)(char *)block;
	block_t *right = (block_t *)(((char *)left) + (block_size / 2));

	// children have one order less
	left->order = (block->order) - 1;
	right->order = left->order;

	// insert left block as free - keep splitting the right one until recursion base
	insert(left);
	return split_block(right, order_req);
}

void *mem_alloc(size_t size)
{
	block_t *block, **pred;

	// reserve space for the preamble
	size += sizeof(size_t);

	// ensure that the space is at least a page
	if (size < PAGE_SIZE)
		size = PAGE_SIZE;

	// round up to fulfill alignment restrictions
	size = (size + 7) & ~7;

	// Compute the requested order
	int order_requested = get_order(size);
	int order;

	// Look for block of fitting order
	for (order = order_requested;; order++)
	{

		if (order > ORDER_MAX)
			goto fail;

		// find a suitably sized block
		pred = &free_area_struct_array[order].free_list, block = free_area_struct_array[order].free_list;

		if (block != NULL)
		{
			break;
		}
	}

	// remove the block from the list
	*pred = block->next;

	block_t *n_block = block;

	// see if we have to split the block
	if (order > order_requested)
	{

		// We are splitting a formerly free block. I.e. the block becomes busy, i.e. we have to toggle the parent as well!
		if (order < ORDER_MAX)
		{
			toggle_parent(n_block);
		}

		// Split recursively
		n_block = split_block(block, order_requested);
	}
	else
	{
		toggle_parent(n_block);
	}

	// return the final block
	return ((size_t *)n_block) + 1;

fail:
	errno = ENOMEM;
	return NULL;
}

/** Merges a block recursively until buddy not available anymore */
block_t *merge(block_t *block, int order_req)
{
	if (block->order >= order_req)
	{
		return block;
	}
	int s = get_block_size(block);
	int bit_map_idx_parent = get_gitmap_idx_parent(block);

	// When the page number is even, then the buddy will be to the right and vice versa
	int uneven = get_page_number(block) % 2;

	// Bit is set if and only if buddy is currently free
	if (bitsetGet(&map[0], bit_map_idx_parent))
	{

		// We are chaning the state of our block, i.e. have to toggle parent
		toggle_parent(block);

		// When the page number is even, then the buddy will be to the right and vice versa
		block_t *buddy = (block_t *)(uneven ? ((char *)block - s) : ((char *)block + s));

		// Drop the body from the free list
		delete (buddy);

		// When the page number is uneven, then the start of the new block has to be offset (i.e. it was formerly the start of the buddy)
		block_t *new = (block_t *)(uneven ? ((char *)block - s) : ((char *)block));

		// resulting block has one order more
		new->order = block->order + 1;

		// keep merging until we reached the top levl or the buddy is not availbale anymore
		return merge(new, order_req);
	}
	else
	{
		toggle_parent(block);
		return block;
	}
}

/** increases the size of an existing block through merging
 * OR  allocate a new one with identical contents */
void *mem_realloc(void *oldptr, size_t new_size)
{
	// Recreate proper pointer to block struct from the input
	block_t *block = (block_t *)((size_t *)oldptr - 1);
	size_t old_size = get_block_size(block) - sizeof(size_t);

	int order_requested = get_order(new_size);

	// The simple base cases
	if (order_requested == block->order)
		return oldptr;
	if (order_requested > ORDER_MAX)
		goto fail;

	// Merge recursively as far as the requested order (if possible)
	block_t *n_block = block;
	n_block = merge(block, order_requested);

	void *newBlock;
	if (n_block->order < order_requested)
	{
		// If merging did not yield a bid enough block - allocate a new one
		newBlock = mem_alloc(new_size);
		insert(n_block);
	}
	else
	{
		// else we can use the merged block
		newBlock = ((size_t *)n_block) + 1;
	}
	if (newBlock == NULL)
		goto fail;

	// make sure the original content is copied over properly
	memcpy(newBlock, oldptr, old_size);

	// return the new block
	return newBlock;

fail:
	errno = ENOMEM;
	return NULL;
}

/** Marks a block as unused */
void mem_free(void *ptr)
{
	// Recreate proper pointer to block struct from the input
	block_t *block = (block_t *)((size_t *)ptr - 1);
	// Merge as much as possible
	block_t *n_block = merge(block, ORDER_MAX);
	// Give back the merged block
	insert(n_block);
}

/** Prints the current state of the allocator */
void mem_dump(FILE *file)
{
	for (int i = ORDER_MAX; i >= 0; i--)
	{
		fprintf(file, "Order %d -> ", i);
		block_t *block = free_area_struct_array[i].free_list;
		while (1)
		{
			if (block == NULL)
				break;
			fprintf(file, "Block [%p, %d]) -> ", block, get_page_number(block));
			block = block->next;
		}
		fprintf(file, "\n");
	}
}
