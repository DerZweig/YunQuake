#include "quakedef.h"

#define	DYNAMIC_SIZE	0xc000

#define	ZONEID	0x1d4a11
#define MINFRAGMENT	64

struct memblock_t
{
	int size; // including the header and possibly tiny fragments
	int tag; // a tag of 0 is a free block
	int id; // should be ZONEID
	memblock_t* next;
	memblock_t* prev;
	int pad; // pad to 64 bit boundary
};

struct memzone_t
{
	int size; // total bytes malloced, including header
	memblock_t blocklist; // start / end cap for linked list
	memblock_t* rover;
};

void Cache_FreeLow(int new_low_hunk);
void Cache_FreeHigh(int new_high_hunk);


/*
==============================================================================

						ZONE MEMORY ALLOCATION

There is never any space between memblocks, and there will never be two
contiguous free memblocks.

The rover can be left pointing at a non-empty block

The zone calls are pretty much only used for small strings and structures,
all big things are allocated on the hunk.
==============================================================================
*/

memzone_t* mainzone;

void Z_ClearZone(memzone_t* zone, int size);


/*
========================
Z_ClearZone
========================
*/
void Z_ClearZone(memzone_t* zone, int size)
{
	// set the entire zone to one free block

	zone->blocklist.next = reinterpret_cast<memblock_t *>(reinterpret_cast<byte *>(zone) + sizeof(memzone_t));
	zone->blocklist.prev = zone->blocklist.next;
	auto block = zone->blocklist.next;

	zone->blocklist.tag = 1; // in use block
	zone->blocklist.id = 0;
	zone->blocklist.size = 0;
	zone->rover = block;

	block->prev = block->next = &zone->blocklist;
	block->tag = 0; // free block
	block->id = ZONEID;
	block->size = size - sizeof(memzone_t);
}


/*
========================
Z_Free
========================
*/
void Z_Free(void* ptr)
{
	if (!ptr)
		Sys_Error("Z_Free: nullptr pointer");

	auto block = reinterpret_cast<memblock_t *>(static_cast<byte *>(ptr) - sizeof(memblock_t));
	if (block->id != ZONEID)
		Sys_Error("Z_Free: freed a pointer without ZONEID");
	if (block->tag == 0)
		Sys_Error("Z_Free: freed a freed pointer");

	block->tag = 0; // mark as free

	auto other = block->prev;
	if (!other->tag)
	{ // merge with previous free block
		other->size += block->size;
		other->next = block->next;
		other->next->prev = other;
		if (block == mainzone->rover)
			mainzone->rover = other;
		block = other;
	}

	other = block->next;
	if (!other->tag)
	{ // merge the next free block onto the end
		block->size += other->size;
		block->next = other->next;
		block->next->prev = block;
		if (other == mainzone->rover)
			mainzone->rover = block;
	}
}


/*
========================
Z_Malloc
========================
*/
void* Z_Malloc(int size)
{
	Z_CheckHeap(); // DEBUG
	auto buf = Z_TagMalloc(size, 1);
	if (!buf)
		Sys_Error("Z_Malloc: failed on allocation of %i bytes", size);
	Q_memset(buf, 0, size);

	return buf;
}

void* Z_TagMalloc(int size, int tag)
{
	memblock_t* rover;

	if (!tag)
		Sys_Error("Z_TagMalloc: tried to use a 0 tag");

	//
	// scan through the block list looking for the first free block
	// of sufficient size
	//
	size += sizeof(memblock_t); // account for size of block header
	size += 4; // space for memory trash tester
	size = size + 7 & ~7; // align to 8-byte boundary

	auto base = rover = mainzone->rover;
	auto start = base->prev;

	do
	{
		if (rover == start) // scaned all the way around the list
			return nullptr;
		if (rover->tag)
			base = rover = rover->next;
		else
			rover = rover->next;
	}
	while (base->tag || base->size < size);

	//
	// found a block big enough
	//
	auto extra = base->size - size;
	if (extra > MINFRAGMENT)
	{ // there will be a free fragment after the allocated block
		auto res = reinterpret_cast<memblock_t *>(reinterpret_cast<byte *>(base) + size);
		res->size = extra;
		res->tag = 0; // free block
		res->prev = base;
		res->id = ZONEID;
		res->next = base->next;
		res->next->prev = res;
		base->next = res;
		base->size = size;
	}

	base->tag = tag; // no longer a free block

	mainzone->rover = base->next; // next allocation will start looking here

	base->id = ZONEID;

	// marker for memory trash testing
	*reinterpret_cast<int *>(reinterpret_cast<byte *>(base) + base->size - 4) = ZONEID;

	return static_cast<void *>(reinterpret_cast<byte *>(base) + sizeof(memblock_t));
}


/*
========================
Z_Print
========================
*/
void Z_Print(memzone_t* zone)
{
	Con_Printf("zone size: %i  location: %p\n", mainzone->size, mainzone);

	for (auto block = zone->blocklist.next; ; block = block->next)
	{
		Con_Printf("block:%p    size:%7i    tag:%3i\n",
		           block, block->size, block->tag);

		if (block->next == &zone->blocklist)
			break; // all blocks have been hit	
		if (reinterpret_cast<byte *>(block) + block->size != reinterpret_cast<byte *>(block->next))
			Con_Printf("ERROR: block size does not touch the next block\n");
		if (block->next->prev != block)
			Con_Printf("ERROR: next block doesn't have proper back link\n");
		if (!block->tag && !block->next->tag)
			Con_Printf("ERROR: two consecutive free blocks\n");
	}
}


/*
========================
Z_CheckHeap
========================
*/
void Z_CheckHeap(void)
{
	for (auto block = mainzone->blocklist.next; ; block = block->next)
	{
		if (block->next == &mainzone->blocklist)
			break; // all blocks have been hit	
		if (reinterpret_cast<byte *>(block) + block->size != reinterpret_cast<byte *>(block->next))
			Sys_Error("Z_CheckHeap: block size does not touch the next block\n");
		if (block->next->prev != block)
			Sys_Error("Z_CheckHeap: next block doesn't have proper back link\n");
		if (!block->tag && !block->next->tag)
			Sys_Error("Z_CheckHeap: two consecutive free blocks\n");
	}
}

//============================================================================

#define	HUNK_SENTINAL	0x1df001ed

struct hunk_t
{
	int sentinal;
	int size; // including sizeof(hunk_t), -1 = not allocated
	char name[8];
};

byte* hunk_base;
int hunk_size;

int hunk_low_used;
int hunk_high_used;

qboolean hunk_tempactive;
int hunk_tempmark;


/*
==============
Hunk_Check

Run consistancy and sentinal trahing checks
==============
*/
void Hunk_Check(void)
{
	for (auto h = reinterpret_cast<hunk_t *>(hunk_base); reinterpret_cast<byte *>(h) != hunk_base + hunk_low_used;)
	{
		if (h->sentinal != HUNK_SENTINAL)
			Sys_Error("Hunk_Check: trahsed sentinal");
		if (h->size < 16 || h->size + reinterpret_cast<byte *>(h) - hunk_base > hunk_size)
			Sys_Error("Hunk_Check: bad size");
		h = reinterpret_cast<hunk_t *>(reinterpret_cast<byte *>(h) + h->size);
	}
}

/*
==============
Hunk_Print

If "all" is specified, every single allocation is printed.
Otherwise, allocations with the same name will be totaled up before printing.
==============
*/
void Hunk_Print(qboolean all)
{
	char name[9];

	name[8] = 0;
	auto count = 0;
	auto sum = 0;
	auto totalblocks = 0;

	auto h = reinterpret_cast<hunk_t *>(hunk_base);
	auto endlow = reinterpret_cast<hunk_t *>(hunk_base + hunk_low_used);
	auto starthigh = reinterpret_cast<hunk_t *>(hunk_base + hunk_size - hunk_high_used);
	auto endhigh = reinterpret_cast<hunk_t *>(hunk_base + hunk_size);

	Con_Printf("          :%8i total hunk size\n", hunk_size);
	Con_Printf("-------------------------\n");

	while (true)
	{
		//
		// skip to the high hunk if done with low hunk
		//
		if (h == endlow)
		{
			Con_Printf("-------------------------\n");
			Con_Printf("          :%8i REMAINING\n", hunk_size - hunk_low_used - hunk_high_used);
			Con_Printf("-------------------------\n");
			h = starthigh;
		}

		//
		// if totally done, break
		//
		if (h == endhigh)
			break;

		//
		// run consistancy checks
		//
		if (h->sentinal != HUNK_SENTINAL)
			Sys_Error("Hunk_Check: trahsed sentinal");
		if (h->size < 16 || h->size + reinterpret_cast<byte *>(h) - hunk_base > hunk_size)
			Sys_Error("Hunk_Check: bad size");

		auto next = reinterpret_cast<hunk_t *>(reinterpret_cast<byte *>(h) + h->size);
		count++;
		totalblocks++;
		sum += h->size;

		//
		// print the single block
		//
		memcpy(name, h->name, 8);
		if (all)
			Con_Printf("%8p :%8i %8s\n", h, h->size, name);

		//
		// print the total
		//
		if (next == endlow || next == endhigh ||
			strncmp(h->name, next->name, 8))
		{
			if (!all)
				Con_Printf("          :%8i %8s (TOTAL)\n", sum, name);
			count = 0;
			sum = 0;
		}

		h = next;
	}

	Con_Printf("-------------------------\n");
	Con_Printf("%8i total blocks\n", totalblocks);
}

/*
===================
Hunk_AllocName
===================
*/
void* Hunk_AllocName(int size, char* name)
{
#ifdef PARANOID
	Hunk_Check ();
#endif

	if (size < 0)
		Sys_Error("Hunk_Alloc: bad size: %i", size);

	size = sizeof(hunk_t) + (size + 15 & ~15);

	if (hunk_size - hunk_low_used - hunk_high_used < size)
		Sys_Error("Hunk_Alloc: failed on %i bytes", size);

	auto h = reinterpret_cast<hunk_t *>(hunk_base + hunk_low_used);
	hunk_low_used += size;

	Cache_FreeLow(hunk_low_used);

	memset(h, 0, size);

	h->size = size;
	h->sentinal = HUNK_SENTINAL;
	Q_strncpy(h->name, name, 8);

	return static_cast<void *>(h + 1);
}

/*
===================
Hunk_Alloc
===================
*/
void* Hunk_Alloc(int size)
{
	return Hunk_AllocName(size, "unknown");
}

int Hunk_LowMark(void)
{
	return hunk_low_used;
}

void Hunk_FreeToLowMark(int mark)
{
	if (mark < 0 || mark > hunk_low_used)
		Sys_Error("Hunk_FreeToLowMark: bad mark %i", mark);
	memset(hunk_base + mark, 0, hunk_low_used - mark);
	hunk_low_used = mark;
}

int Hunk_HighMark(void)
{
	if (hunk_tempactive)
	{
		hunk_tempactive = qfalse;
		Hunk_FreeToHighMark(hunk_tempmark);
	}

	return hunk_high_used;
}

void Hunk_FreeToHighMark(int mark)
{
	if (hunk_tempactive)
	{
		hunk_tempactive = qfalse;
		Hunk_FreeToHighMark(hunk_tempmark);
	}
	if (mark < 0 || mark > hunk_high_used)
		Sys_Error("Hunk_FreeToHighMark: bad mark %i", mark);
	memset(hunk_base + hunk_size - hunk_high_used, 0, hunk_high_used - mark);
	hunk_high_used = mark;
}


/*
===================
Hunk_HighAllocName
===================
*/
void* Hunk_HighAllocName(int size, char* name)
{
	if (size < 0)
		Sys_Error("Hunk_HighAllocName: bad size: %i", size);

	if (hunk_tempactive)
	{
		Hunk_FreeToHighMark(hunk_tempmark);
		hunk_tempactive = qfalse;
	}

#ifdef PARANOID
	Hunk_Check ();
#endif

	size = sizeof(hunk_t) + (size + 15 & ~15);

	if (hunk_size - hunk_low_used - hunk_high_used < size)
	{
		Con_Printf("Hunk_HighAlloc: failed on %i bytes\n", size);
		return nullptr;
	}

	hunk_high_used += size;
	Cache_FreeHigh(hunk_high_used);

	auto h = reinterpret_cast<hunk_t *>(hunk_base + hunk_size - hunk_high_used);

	memset(h, 0, size);
	h->size = size;
	h->sentinal = HUNK_SENTINAL;
	Q_strncpy(h->name, name, 8);

	return static_cast<void *>(h + 1);
}


/*
=================
Hunk_TempAlloc

Return space from the top of the hunk
=================
*/
void* Hunk_TempAlloc(int size)
{
	size = size + 15 & ~15;

	if (hunk_tempactive)
	{
		Hunk_FreeToHighMark(hunk_tempmark);
		hunk_tempactive = qfalse;
	}

	hunk_tempmark = Hunk_HighMark();

	auto buf = Hunk_HighAllocName(size, "temp");

	hunk_tempactive = qtrue;

	return buf;
}

/*
===============================================================================

CACHE MEMORY

===============================================================================
*/

struct cache_system_t
{
	int size; // including this header
	cache_user_t* user;
	char name[16];
	cache_system_t *prev, *next;
	cache_system_t *lru_prev, *lru_next; // for LRU flushing	
};

cache_system_t* Cache_TryAlloc(int size, qboolean nobottom);

cache_system_t cache_head;

/*
===========
Cache_Move
===========
*/
void Cache_Move(cache_system_t* c)
{
	// we are clearing up space at the bottom, so only allocate it late
	auto res = Cache_TryAlloc(c->size, qtrue);
	if (res)
	{
		//		Con_Printf ("cache_move ok\n");

		Q_memcpy(res + 1, c + 1, c->size - sizeof(cache_system_t));
		res->user = c->user;
		Q_memcpy(res->name, c->name, sizeof res->name);
		Cache_Free(c->user);
		res->user->data = static_cast<void *>(res + 1);
	}
	else
	{
		//		Con_Printf ("cache_move failed\n");

		Cache_Free(c->user); // tough luck...
	}
}

/*
============
Cache_FreeLow

Throw things out until the hunk can be expanded to the given point
============
*/
void Cache_FreeLow(int new_low_hunk)
{
	while (true)
	{
		auto c = cache_head.next;
		if (c == &cache_head)
			return; // nothing in cache at all
		if (reinterpret_cast<byte *>(c) >= hunk_base + new_low_hunk)
			return; // there is space to grow the hunk
		Cache_Move(c); // reclaim the space
	}
}

/*
============
Cache_FreeHigh

Throw things out until the hunk can be expanded to the given point
============
*/
void Cache_FreeHigh(int new_high_hunk)
{
	cache_system_t* prev = nullptr;
	while (true)
	{
		auto c = cache_head.prev;
		if (c == &cache_head)
			return; // nothing in cache at all
		if (reinterpret_cast<byte *>(c) + c->size <= hunk_base + hunk_size - new_high_hunk)
			return; // there is space to grow the hunk
		if (c == prev)
			Cache_Free(c->user); // didn't move out of the way
		else
		{
			Cache_Move(c); // try to move it
			prev = c;
		}
	}
}

void Cache_UnlinkLRU(cache_system_t* cs)
{
	if (!cs->lru_next || !cs->lru_prev)
		Sys_Error("Cache_UnlinkLRU: nullptr link");

	cs->lru_next->lru_prev = cs->lru_prev;
	cs->lru_prev->lru_next = cs->lru_next;

	cs->lru_prev = cs->lru_next = nullptr;
}

void Cache_MakeLRU(cache_system_t* cs)
{
	if (cs->lru_next || cs->lru_prev)
		Sys_Error("Cache_MakeLRU: active link");

	cache_head.lru_next->lru_prev = cs;
	cs->lru_next = cache_head.lru_next;
	cs->lru_prev = &cache_head;
	cache_head.lru_next = cs;
}

/*
============
Cache_TryAlloc

Looks for a free block of memory between the high and low hunk marks
Size should already include the header and padding
============
*/
cache_system_t* Cache_TryAlloc(int size, qboolean nobottom)
{
	cache_system_t* res;

	// is the cache completely empty?

	if (!nobottom && cache_head.prev == &cache_head)
	{
		if (hunk_size - hunk_high_used - hunk_low_used < size)
			Sys_Error("Cache_TryAlloc: %i is greater then free hunk", size);

		res = reinterpret_cast<cache_system_t *>(hunk_base + hunk_low_used);
		memset(res, 0, sizeof*res);
		res->size = size;

		cache_head.prev = cache_head.next = res;
		res->prev = res->next = &cache_head;

		Cache_MakeLRU(res);
		return res;
	}

	// search from the bottom up for space

	res = reinterpret_cast<cache_system_t *>(hunk_base + hunk_low_used);
	auto cs = cache_head.next;

	do
	{
		if (!nobottom || cs != cache_head.next)
		{
			if (reinterpret_cast<byte *>(cs) - reinterpret_cast<byte *>(res) >= size)
			{ // found space
				memset(res, 0, sizeof*res);
				res->size = size;

				res->next = cs;
				res->prev = cs->prev;
				cs->prev->next = res;
				cs->prev = res;

				Cache_MakeLRU(res);

				return res;
			}
		}

		// continue looking		
		res = reinterpret_cast<cache_system_t *>(reinterpret_cast<byte *>(cs) + cs->size);
		cs = cs->next;
	}
	while (cs != &cache_head);

	// try to allocate one at the very end
	if (hunk_base + hunk_size - hunk_high_used - reinterpret_cast<byte *>(res) >= size)
	{
		memset(res, 0, sizeof*res);
		res->size = size;

		res->next = &cache_head;
		res->prev = cache_head.prev;
		cache_head.prev->next = res;
		cache_head.prev = res;

		Cache_MakeLRU(res);

		return res;
	}

	return nullptr; // couldn't allocate
}

/*
============
Cache_Flush

Throw everything out, so new data will be demand cached
============
*/
void Cache_Flush(void)
{
	while (cache_head.next != &cache_head)
		Cache_Free(cache_head.next->user); // reclaim the space
}


/*
============
Cache_Print

============
*/
void Cache_Print(void)
{
	for (auto cd = cache_head.next; cd != &cache_head; cd = cd->next)
	{
		Con_Printf("%8i : %s\n", cd->size, cd->name);
	}
}

/*
============
Cache_Report

============
*/
void Cache_Report(void)
{
	Con_DPrintf("%4.1f megabyte data cache\n", (hunk_size - hunk_high_used - hunk_low_used) / static_cast<float>(1024 * 1024));
}

/*
============
Cache_Compact

============
*/
void Cache_Compact(void)
{
}

/*
============
Cache_Init

============
*/
void Cache_Init(void)
{
	cache_head.next = cache_head.prev = &cache_head;
	cache_head.lru_next = cache_head.lru_prev = &cache_head;

	Cmd_AddCommand("flush", Cache_Flush);
}

/*
==============
Cache_Free

Frees the memory and removes it from the LRU list
==============
*/
void Cache_Free(cache_user_t* c)
{
	if (!c->data)
		Sys_Error("Cache_Free: not allocated");

	auto cs = static_cast<cache_system_t *>(c->data) - 1;

	cs->prev->next = cs->next;
	cs->next->prev = cs->prev;
	cs->next = cs->prev = nullptr;

	c->data = nullptr;

	Cache_UnlinkLRU(cs);
}


/*
==============
Cache_Check
==============
*/
void* Cache_Check(cache_user_t* c)
{
	if (!c->data)
		return nullptr;

	auto cs = static_cast<cache_system_t *>(c->data) - 1;

	// move to head of LRU
	Cache_UnlinkLRU(cs);
	Cache_MakeLRU(cs);

	return c->data;
}


/*
==============
Cache_Alloc
==============
*/
void* Cache_Alloc(cache_user_t* c, int size, char* name)
{
	if (c->data)
		Sys_Error("Cache_Alloc: allready allocated");

	if (size <= 0)
		Sys_Error("Cache_Alloc: size %i", size);

	size = size + sizeof(cache_system_t) + 15 & ~15;

	// find memory for it	
	while (true)
	{
		auto cs = Cache_TryAlloc(size, qfalse);
		if (cs)
		{
			strncpy(cs->name, name, sizeof cs->name - 1);
			c->data = static_cast<void *>(cs + 1);
			cs->user = c;
			break;
		}

		// free the least recently used cahedat
		if (cache_head.lru_prev == &cache_head)
			Sys_Error("Cache_Alloc: out of memory");
		// not enough memory at all
		Cache_Free(cache_head.lru_prev->user);
	}

	return Cache_Check(c);
}

//============================================================================


/*
========================
Memory_Init
========================
*/
void Memory_Init(void* buf, int size)
{
	auto zonesize = DYNAMIC_SIZE;

	hunk_base = static_cast<byte*>(buf);
	hunk_size = size;
	hunk_low_used = 0;
	hunk_high_used = 0;

	Cache_Init();
	auto p = COM_CheckParm("-zone");
	if (p)
	{
		if (p < com_argc - 1)
			zonesize = Q_atoi(com_argv[p + 1]) * 1024;
		else
			Sys_Error("Memory_Init: you must specify a size in KB after -zone");
	}
	mainzone = static_cast<memzone_t*>(Hunk_AllocName(zonesize, "zone"));
	Z_ClearZone(mainzone, zonesize);
}
