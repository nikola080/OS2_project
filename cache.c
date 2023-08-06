#pragma once
#include "buddy.h"
#include "cache.h"
HANDLE mainMutex;
int taken = 0;
int amount_of_blocks(size_t sizeof_object) {
	int blocks_taken = 1;
	while (1) {
		if (4 * sizeof(char*) + sizeof(unsigned int) + 1 * sizeof_object > blocks_taken* BLOCK_SIZE) ++blocks_taken;
		else break;
	}
	return blocks_taken;
}

void findPadding_initNumObj(kes * parent, size_t sizeof_object, unsigned int blocks_taken) { //pronalazi koliko ima objekata i inicjalizuje numOfObjects u cache-u
	int bitmap = 1;
	int remain = blocks_taken * BLOCK_SIZE - (4 * sizeof(char*) + sizeof(unsigned int));
	int i = 0;
	while (remain >= sizeof_object) {
		++i;
		if (!(i % 32)) {
			++bitmap;
			remain -= sizeof(unsigned int);

			if (remain < sizeof_object) {
				remain += sizeof(unsigned int);
				--bitmap;
				--i;
				break;
			}

		}
		remain -= sizeof_object;
	}
	parent->numOfObj = i;
	parent->padding = remain;
	parent->currentAligning = 0;
}

void init_slab(kmem_cache_t* cache, char* begin, size_t sizeof_object, unsigned int numof_objects) {
	slab* s = (slab*)(begin);

	if (cache->padding != 0) {
		if (cache->currentAligning + CACHE_L1_LINE_SIZE >= cache->padding) cache->currentAligning = 0;
		else cache->currentAligning = (cache->currentAligning + CACHE_L1_LINE_SIZE) % cache->padding;
	}

	s->object_array = begin + 4 * sizeof(char*) + ((numof_objects / 32) + 1 - !(numof_objects % 32)) * sizeof(unsigned int) + cache->currentAligning;
	s->next = NULL;
	s->prev = NULL;
	s->bitMap = begin + 4 * sizeof(char*);
	
	unsigned int* pok = s->bitMap;
	int i = 0;
	int n = numof_objects / 32 + 1 - !(numof_objects % 32);
	while (i < n) {
		pok[i++] = 0;
	}
}

void* kmem_cache_alloc(kmem_cache_t* cachep) {

	if (cachep == NULL) {
		;
		return NULL;
	}
	WaitForSingleObject(cachep->mutex, INFINITE);
	kmem_cache_t* mm = *((kmem_cache_t**)(buddy + sizeof(char*)));
	if (cachep == mm) {
		printf("Uzimanje od glavnog kesa\n\n\n");
	}
	//printf("Alokacija\n");
	slab* find;
	if (!cachep->empty && !cachep->halfEmpty) {
		cachep->empty = require(cachep->size_of_slab);
		if (cachep->empty == NULL) {

			ReleaseMutex(cachep->mutex);
			return NULL;
		}
		init_slab(cachep,cachep->empty, cachep->sizeof_object, cachep->numOfObj);

		cachep->empty->bitMap[0] = 1;
		if(cachep->ctor) konstruisi(cachep,cachep->empty, cachep->ctor);
		if (cachep->numOfObj != 1) {
			cachep->halfEmpty = cachep->empty;
			cachep->empty = NULL;
			cachep->halfEmpty->next = NULL;
			cachep->halfEmpty->prev = NULL;
			++(cachep->numOfActiveObj);
			char* ret = cachep->halfEmpty->object_array;
			ReleaseMutex(cachep->mutex);
			return ret;
		}
		else {
			//if (cachep->full->next) cachep->full->next->prev = cachep->empty;
			cachep->empty->next = cachep->full;
			cachep->full = cachep->empty;
			cachep->empty = NULL;
			cachep->full->prev = NULL;
			if (cachep->full->next) cachep->full->next->prev = cachep->full;
			++(cachep->numOfActiveObj);
			char* ret = cachep->full->object_array;
			ReleaseMutex(cachep->mutex);
			return ret;

		}
	}
	if (cachep->halfEmpty) find = cachep->halfEmpty;
	else find = cachep->empty;
	int i = 0; int j = 0;
	int flag = 1;

	while (flag & find->bitMap[i]) {
		if (j == cachep->numOfObj)
		{
			ReleaseMutex(cachep->mutex);
			return NULL;
		}
		++j;
		if (!(j % 32)) {
			flag = 1;
			++i;
		}
		else flag <<= 1;	
	}


	if (find == cachep->empty) {
		slab* pok = cachep->empty;
		cachep->empty = cachep->empty->next;
		if (cachep->empty) cachep->empty->prev = NULL;

		pok->next = cachep->halfEmpty;
		if (pok->next) pok->next->prev = pok;
		cachep->halfEmpty = pok;
		pok->prev = NULL;
		pok->bitMap[0] = 1;

		if (cachep->numOfObj == 1) {
			slab* halfempt = cachep->halfEmpty;
			if (cachep->halfEmpty->next) cachep->halfEmpty->next->prev = NULL;
			cachep->halfEmpty = cachep->halfEmpty->next;

			slab* head = cachep->full;

			halfempt->next = head;
			if (head) head->prev = halfempt;
			halfempt->prev = NULL;
			cachep->full = halfempt;
		}
		
	}
	else {
		
		find->bitMap[i] |= flag;
		int limit = cachep->numOfObj / 32 - !(cachep->numOfObj % 32);
		unsigned isFull = limit + 1;
		for (int k = 0; k <= limit; k++) {
			if (k == limit) {
				int maska = 1;
				int iter = cachep->numOfObj % 32;
				if (iter != 0) {
					for (int t = 0; t < iter - 1; t++) {
						maska <<= 1;
						maska |= 1;
					}
				}
				else maska = ~0;

				if (!(maska ^ find->bitMap[k])) isFull--;
			}
			else if (!(~find->bitMap[k])) isFull--;
		}
		if (!isFull) {
			if (cachep->halfEmpty->next) cachep->halfEmpty->next->prev = NULL;
			cachep->halfEmpty = cachep->halfEmpty->next;
			
			slab* head = cachep->full;
			
			find->next = head;
			if(head) head->prev = find;
			find->prev = NULL;
			cachep->full = find;

		}
	}
	++(cachep->numOfActiveObj);
	char* rett = (char*)find->object_array + j * cachep->sizeof_object;
	ReleaseMutex(cachep->mutex);

	return (void*)rett;
}


void kmem_cache_free(kmem_cache_t* cachep, void* objp) {
	WaitForSingleObject(cachep->mutex, INFINITE);

	//printf("Vracanje\n");
	slab* pok = cachep->halfEmpty;
	int type = 0;

	char* pokk = NULL;
	while (pok) {
		pokk = pok->object_array;
		if (objp >= pok->object_array && objp < pokk+ cachep->size_of_slab * BLOCK_SIZE) {
			type = 1;
			break;
		}
		pok = pok->next;
	}
	if (!pok) { // ako ga nema u halfFull slab-u
		pok = cachep->full;
		while (pok) {
			pokk = pok->object_array;
			if (objp >= pok->object_array && objp < pokk + cachep->size_of_slab * BLOCK_SIZE) {
				type = 2;
				break;
			}
			pok = pok->next;
		}
	}

	if (!pok || !type) {
		ReleaseMutex(cachep->mutex);
		return; //ovo znaci da je ili u empty ili ga nema uopste sto u oba slucaja znaci da treba da se vrati iz fje
	}
	char* radress = pok->object_array;
	int i = 0;
	//i = ((char*)objp - pok->object_array) / cachep->sizeof_object - 1;
	while (radress != objp) {
		radress += cachep->sizeof_object;
		++i;
	}

	int j = i / 32;
	pok->bitMap[j] &= ~(1 << (i % 32));

	if (type == 2) { // ako se objekat nalazio u full slab-u onda mora da predje u half slab, ovde je implementirano da se preveze na kraj
		if (cachep->full == pok) {
			cachep->full = pok->next;
		}
		if (pok->prev) pok->prev->next = pok->next;
		if (pok->next) pok->next->prev = pok->prev;

		if (cachep->numOfObj != 1) {

			if (!cachep->halfEmpty) {
				cachep->halfEmpty = pok;
				cachep->halfEmpty->next = NULL;
				cachep->halfEmpty->prev = NULL;
			}
			else {
				cachep->halfEmpty->prev = pok;
				pok->next = cachep->halfEmpty;
				cachep->halfEmpty = pok;
				cachep->halfEmpty->prev = NULL;
			}
		}
		else {
			if (!cachep->empty) {
				cachep->empty = pok;
				cachep->empty->next = NULL;
				cachep->empty->prev = NULL;
			}
			else {
				cachep->empty->prev = pok;
				pok->next = cachep->empty;
				cachep->empty = pok;
				cachep->empty->prev = NULL;
			}

			--(cachep->numOfActiveObj);
			ReleaseMutex(cachep->mutex);
			return;
		}
	}
	else {
		int limit = cachep->numOfObj / 32 - !(cachep->numOfObj % 32);
		int check = limit + 1;
		for (int k = 0; k <= limit; k++) {
			if (!pok->bitMap[k]) check--;
		}

		if (!check) { // ako je u  halfslabu onda se prebacuje u empty samo ako je svaki element niza bitMap jednak nuli
			if (cachep->halfEmpty == pok) {
				cachep->halfEmpty = cachep->halfEmpty->next;
			}
			if (pok->prev) pok->prev->next = pok->next;
			if (pok->next) pok->next->prev = pok->prev;

			if (!cachep->empty) {
				cachep->empty = pok;
				cachep->empty->next = NULL;
				cachep->empty->prev = NULL;
			}
			else {
				pok->next = cachep->empty;
				cachep->empty->prev = pok;
				cachep->empty = pok;
				cachep->empty->prev = NULL;
			}
		}
	}
	--(cachep->numOfActiveObj);
	ReleaseMutex(cachep->mutex);
}

void kmem_cache_destroy(kmem_cache_t* cachep) {
	kmem_cache_t* mainK = *((char**)(buddy + sizeof(char*)));

	WaitForSingleObject(cachep->mutex,INFINITE);
	slab* pok = cachep->empty;
	while (pok) {
		slab* p = pok->next;
		returning(pok, cachep->size_of_slab);
		pok = p;
	}
	pok = cachep->halfEmpty;
	while (pok) {
		slab* p = pok->next;
		returning(pok, cachep->size_of_slab);
		pok = p;
	}
	pok = cachep->full;
	while (pok) {
		slab* p = pok->next;
		returning(pok, cachep->size_of_slab);
		pok = p;
	}
	
	if (cachep == mainK) { // za main_cache
		returning(cachep, sizeof(kmem_cache_t) / BLOCK_SIZE + 1 - !(sizeof(kmem_cache_t) % BLOCK_SIZE));
		*((char**)(buddy + sizeof(char*))) = NULL;
	}
	else {
		ReleaseMutex(cachep->mutex);
		kmem_cache_free(mainK, cachep);
		printing();

		//returning(cachep, amount_of_blocks(cachep->sizeof_object));
	}
}

void* kmalloc(size_t size) {
	WaitForSingleObject(mainMutex, INFINITE);
	int k = 0;
	while (1) {
		if (1 << k < size) k++;
		else break;
	}

	if (k < 5 || k >= 18) {
		ReleaseMutex(mainMutex);
		return NULL;

	}
	kmem_cache_t** buff = buddy + 2 * sizeof(char*) + sizeof(int) + (k - 5) * sizeof(kmem_cache_t*);

	if (!(*buff)) {
		*buff = require((sizeof(kmem_cache_t) / BLOCK_SIZE) + 1 - !(sizeof(kmem_cache_t) % BLOCK_SIZE));
		if ((*buff) == NULL) {
			ReleaseMutex(mainMutex);
			return NULL;
		}
		(*buff)->name[0] = 's';
		(*buff)->name[1] = 'i';
		(*buff)->name[2] = 'z';
		(*buff)->name[3] = 'e';
		(*buff)->name[4] = '-';
		(*buff)->name[5] = 48 + k;
		(*buff)->name[6] = ' ';
		(*buff)->name[7] = 'c';
		(*buff)->name[8] = 'a';
		(*buff)->name[9] = 'c';
		(*buff)->name[10] = 'h';
		(*buff)->name[11] = 'e';
		(*buff)->name[12] = '\0';

		(*buff)->type = 1;
		(*buff)->mutex = CreateMutex(0, FALSE, 0);
		(*buff)->sizeof_object = 1 << k;
		(*buff)->begin = (*buff);
		(*buff)->numOfActiveObj = 0;
		(*buff)->ctor = NULL;
		(*buff)->dtor = NULL;
		(*buff)->size_of_slab = amount_of_blocks((*buff)->sizeof_object);
		findPadding_initNumObj((*buff), 1 << k, (*buff)->size_of_slab);
		(*buff)->empty = require((*buff)->size_of_slab);
		if ((*buff)->empty == NULL) {
			returning((*buff)->empty, (sizeof(kmem_cache_t) / BLOCK_SIZE) + 1 - !(sizeof(kmem_cache_t) % BLOCK_SIZE));
			ReleaseMutex(mainMutex);
			return NULL;
		}
		init_slab((*buff), (*buff)->empty, (*buff)->sizeof_object, (*buff)->numOfObj);
		(*buff)->halfEmpty = NULL;
		(*buff)->full = NULL;
	}
	ReleaseMutex(mainMutex);

	char* ret = kmem_cache_alloc(*buff);
	return (void*)ret;
}

void kfree(const void* objp) {
	
	kmem_cache_t* pok = *((kmem_cache_t**)(buddy + 2 * sizeof(char*) + sizeof(int)));
	int i = 0;
	WaitForSingleObject(mainMutex, INFINITE);
	char* begin = NULL;
	while (i != 13) {
		if (pok) {
			slab* empty = pok->halfEmpty;
			if (empty) {
				begin = empty->object_array;
				while (begin < empty->object_array + pok->size_of_slab * BLOCK_SIZE) {
					if (begin == objp) {
						kmem_cache_free(pok, objp);
						ReleaseMutex(mainMutex);

						return;
					}
					begin += pok->sizeof_object;
				}
			}

			slab* halfEmpty = pok->halfEmpty;
			if (halfEmpty) {
				begin = halfEmpty->object_array;
				while (begin < halfEmpty->object_array + pok->size_of_slab * BLOCK_SIZE) {
					if (begin == objp) {
						kmem_cache_free(pok, objp);
						ReleaseMutex(mainMutex);
						return;
					}
					begin += pok->sizeof_object;
				}
			}

			slab* full = pok->full;
			if (full) {
				begin = full->object_array;
				while (begin < full->object_array + pok->size_of_slab * BLOCK_SIZE) {
					if (begin == objp) {
						kmem_cache_free(pok, objp);
						ReleaseMutex(mainMutex);
						return;
					}
					begin += pok->sizeof_object;
				}
			}
		}
		++i;
		pok = *((kmem_cache_t**)(buddy + 2 * sizeof(char*) + sizeof(int) + i * sizeof(kmem_cache_t*)));
	
	}
	ReleaseMutex(mainMutex);

}

struct objects_s {
	kmem_cache_t* cache;
	void* data;
};

void konstruisi(kmem_cache_t* cachep,slab* sll, void (*ctor)(void*)) {

	int n = cachep->numOfObj / 32 + 1 - !(cachep->numOfObj % 32);
	char* objekti = sll->object_array;
	for (int i = 0; i < cachep->numOfObj; i++) {
		ctor(objekti + i * shared_size);
	}

}
kmem_cache_t* kmem_cache_create(const char* name, size_t size, void (*ctor)(void*), void (*dtor)(void*)) {
	
	WaitForSingleObject(mainMutex, INFINITE);
	char** freeMem = (char**)(buddy + sizeof(char*));
	//printf("Kreiranje\n");
	kmem_cache_t* main_cache = *freeMem;

	if (!main_cache) {
		mainMutex = CreateMutex(0, FALSE, 0);
		main_cache = require((sizeof(kmem_cache_t)/BLOCK_SIZE) + 1 - !(sizeof(kmem_cache_t) % BLOCK_SIZE));
		*freeMem = main_cache;
		int k = 0;
		char main_cache_name[11] = "Main_Cache";
		for (int i = 0; i <= 10; main_cache->name[i] = main_cache_name[i++]);
		main_cache->sizeof_object = sizeof(kmem_cache_t);
		main_cache->mutex = CreateMutex(0, FALSE, 0);
		main_cache->type = 0;
		main_cache->begin = main_cache;
		main_cache->numOfActiveObj = 0;
		main_cache->ctor = NULL;//NULL
		main_cache->dtor = NULL;
		main_cache->size_of_slab = amount_of_blocks(main_cache->sizeof_object);
		findPadding_initNumObj(main_cache, sizeof(kmem_cache_t), main_cache->size_of_slab);
		main_cache->empty = require(main_cache->size_of_slab);
		init_slab(main_cache,main_cache->empty, main_cache->sizeof_object, main_cache->numOfObj);
		main_cache->halfEmpty = NULL;
		main_cache->full = NULL;
	}
	kmem_cache_t* ret = kmem_cache_alloc(main_cache);

	if (ret == NULL)
	{
		ReleaseMutex(mainMutex);
		return NULL;
	}
	int j = 0;
	while (name[j] != '\0') {
		ret->name[j] = name[j++];
	}
	ret->name[j] = '\0';
	ret->type = 0;
	ret->sizeof_object = size;
	ret->begin = ret;
	ret->numOfActiveObj = 0;
	ret->ctor = ctor;
	ret->dtor = dtor;
	ret->mutex = CreateMutex(0,FALSE, 0);
	ret->size_of_slab = amount_of_blocks(ret->sizeof_object);
	findPadding_initNumObj(ret, size, ret->size_of_slab);
	ret->empty = require(ret->size_of_slab);

	if (ret->empty == NULL) {
		ReleaseMutex(mainMutex);
		return NULL;
	}
	init_slab(ret, ret->empty, ret->sizeof_object, ret->numOfObj);

	ret->halfEmpty = NULL;
	ret->full = NULL;
	if(ctor != NULL) konstruisi(ret,ret->empty, ctor);
	ReleaseMutex(mainMutex);
	return ret;
}	

int kmem_cache_shrink(kmem_cache_t* cachep) {
	WaitForSingleObject(cachep->mutex, INFINITE);

	slab* pok = cachep->empty;
	while (pok) {
		cachep->empty = cachep->empty->next;
		returning(pok,cachep->size_of_slab);
		pok = pok->next;
	}

	ReleaseMutex(cachep->mutex);
}

void kmem_cache_info(kmem_cache_t* cachep) {
	WaitForSingleObject(cachep->mutex,INFINITE);
	int n = cachep->numOfObj / 32 + 1 - !(cachep->numOfObj % 32);
	printf("%s\n", cachep->name);
	printf("Empty stats: \n");
	slab* empty = cachep->empty;
	int numEmpty = 0;
	while (empty) {
		numEmpty++;
		for (int i = 0; i < n; i++) {
			printf("Bitmap[%d]: %d\n", i, empty->bitMap[i]);
		}
		
		empty = empty->next;
	}
	printf("Broj empty slabova: %d\n\n", numEmpty);
	printf("HalfEmpty stats: \n");
	slab* half = cachep->halfEmpty;
	int numHalf = 0;
	while (half) {
		numHalf++;
		for (int i = 0; i < n; i++) {
			printf("Bitmap[%d]: %d\n", i, half->bitMap[i]);
		}
		half = half->next;
	}
	printf("Broj half-empty slabova: %d\n\n", numHalf);
	printf("Full stats: \n");
	slab* full = cachep->full;
	int numFull = 0;
	while (full) {
		numFull++;
		for (int i = 0; i < n; i++) {
			printf("Bitmap[%d]: %d\n", i, full->bitMap[i]);
		}
		full = full->next;
	}
	printf("Broj full slabova: %d\n", numFull);
	printf("Aktivni objekti: %d\n", cachep->numOfActiveObj);
	printf("Broj objekata: %d\n\n", cachep->numOfObj);
	ReleaseMutex(cachep->mutex);
}