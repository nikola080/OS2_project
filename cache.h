#pragma once
#include <stdlib.h>
#include<stdio.h>
#include<Windows.h>

#define CACHE_L1_LINE_SIZE (64)

typedef struct kmem_slab {
	struct kmem_slab* next;
	struct kmem_slab* prev;
	char* object_array;
	unsigned int* bitMap;
}slab;

typedef struct kmem_cache_s kmem_cache_t;

typedef struct kmem_cache_s {
	char name[20];//
	unsigned int type;  //0 - obican, 1 - buffer_cache //
	slab* empty;//
	slab* halfEmpty;//
	slab* full;//
	char* begin;//
	size_t sizeof_object;//
	unsigned int size_of_slab;//
	unsigned int padding;//
	unsigned int currentAligning;//
	unsigned int numOfObj;//
	unsigned int numOfActiveObj;//
	void (*ctor)(void*);//
	void (*dtor)(void*);//
	HANDLE mutex;
}kes;



int amount_of_blocks(size_t sizeof_object);
void findPadding_initNumObj(kes* parent, size_t sizeof_object, unsigned int blocks_taken);
//void init_slab(char* begin, size_t sizeof_object, unsigned int numof_objects);
void init_slab(kmem_cache_t* cache, char* begin, size_t sizeof_object, unsigned int numof_objects);
void* kmem_cache_alloc(kmem_cache_t* cachep);
void kmem_cache_free(kmem_cache_t* cachep, void* objp);
kmem_cache_t* kmem_cache_create(const char* name, size_t size, void (*ctor)(void*), void (*dtor)(void*));
void kmem_cache_destroy(kmem_cache_t* cachep);

void* kmalloc(size_t size); 
void kfree(const void* objp);
void kmem_cache_info(kmem_cache_t* cachep);
int kmem_cache_error(kmem_cache_t* cachep);
void kmem_cache_info(kmem_cache_t* cachep);
void  konstruisi(kmem_cache_t* cachep, void (*ctor)(void*));
int kmem_cache_shrink(kmem_cache_t* cachep);