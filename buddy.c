#include<stdio.h>
#include<stdlib.h>
#include "cache.h"


char *buddy;
int velicina;
#define BLOCK_SIZE (50)
int ind = 0;
HANDLE mainMutex;
typedef struct buddy_cp buddy_chunk_pointer;

typedef struct buddy_cb {      
	struct buddy_cb* next;
	struct buddy_cb* prev;
	buddy_chunk_pointer* parent;
	char* adressOfBlock;
}buddy_chunk_block;  // struktura koja pokazuje na sam blok memorije okji je slobodan;

typedef struct buddy_cp {        
	buddy_chunk_block* head;
	unsigned int sizeOfBlock;
}buddy_chunk_pointer; // strukura koja je element niza koji cuva za odredjeni stepen pokazivac na prvi blok velicine sizeOfBlock

typedef struct buddy_c {
	char* begin;
	buddy_chunk_pointer* chunkArray;
	unsigned int sizeOfChunk;
	//HANDLE mutex;
}buddy_chunk; // "chunk", ili ti velicina bloka koja izgradjuje memoriju(velcina je 2^(pozocija gde je 1 u binarnoj
              // predstavi), koja sadrzi pokazivac na svoj niz koji ima pokzivace na blokove odredjenog stepen u 
			  // zavisnosti od pozicije, ukratko ovo je jedan buddy alokator


int printing() {

	char* pok = buddy + 2 * sizeof(char*) + sizeof(int) + 13 * sizeof(kmem_cache_t*);
	buddy_chunk* niz = pok;
	int n = *(buddy + 2 * sizeof(char*));
	printf("N = %d\n", n);
	for (int i = 0; i < n; i++) {
		for (int j = 0; j <= niz[i].sizeOfChunk; j++) {
			if (niz[i].chunkArray[j].head) {
				buddy_chunk_block* pok = niz[i].chunkArray[j].head;
				int br = 0;
				while (pok) {
				//	printf("%p\n", pok);
					if (pok->next == pok) return 1;
					br++;
					pok = pok->next;
				}
				printf("%d ", br);
			}
			else printf("0 ");
		}
		printf("%d \n", niz[i].sizeOfChunk);
	}
	return 0;
}

int isNotPowerTwo(int number) {
	int i = 1;
	while (1) {
		if (i >= number) break;
		else i <<= 1;
	}
	if (number == i) return 0;
	else return 1;
}
void kmem_init(void* space, int size) {
	velicina = size;
	if (!space || size <= 3) return;
	buddy = space;
	int stepen = 0;
	int amount = 1;
	while (1) {
		if (amount == size) break;
		if (amount > size) {
			--stepen;
			break;
		}
		else {
			amount <<= 1;
			++stepen;
		}
	}

	int pow = stepen;
	int chunksTaken = 1;
	int numOfChunks = 0;
	int numofChunkPointers = 0;
	while(1) {
		int sizetmp = size - chunksTaken;
		int powtmp = pow;
		numofChunkPointers = 0;
		numOfChunks = 0;
		if (sizetmp < (1 << pow)) --pow;
		while (1) {

			if (sizetmp == 0) break;
			if (sizetmp == 1) {
				++numOfChunks;
				++numofChunkPointers;		
				break;
			}

			if (!isNotPowerTwo(sizetmp)) {
				++numOfChunks;
				numofChunkPointers += powtmp + 1;
				break;
			}
			if (sizetmp > (1 << powtmp)) {
				++numOfChunks;
				numofChunkPointers += powtmp + 1;
				sizetmp -= (1 << powtmp);
			}
			--powtmp;
		}

	

		if ((chunksTaken * BLOCK_SIZE) < (numOfChunks * sizeof(buddy_chunk) + numofChunkPointers * sizeof(buddy_chunk_pointer) + 2 * sizeof(char*) + sizeof(unsigned int) + ((size - chunksTaken) / 2 + 1) * sizeof(buddy_chunk_block) + 13 * sizeof(kmem_cache_t*))) {
			++chunksTaken;
		}
		else break;
	} 

	mainMutex = CreateMutex(0, FALSE, 0);

	char** freeMem = (char**)buddy; // pocetak memorije koju buddy moze da dodeli 
	*freeMem = buddy + sizeof(char*);

	char** t = *freeMem;
	*t = NULL; // glavni cache
	*freeMem = *freeMem + sizeof(char*);

	int* tt = *freeMem;
	*tt = numOfChunks; // broj chunkova 
	*freeMem = *freeMem + sizeof(int);

	for (int k = 0; k <= 12; k++) {
		kmem_cache_t **buff= *freeMem;
		*buff = NULL;
		*freeMem = *freeMem + sizeof(kmem_cache_t*);
	}


	int powtmp = pow;
	int sizetmp = size - chunksTaken;
	char* beginning = buddy + chunksTaken * BLOCK_SIZE;
	buddy_chunk* nizBlokova = *freeMem;
	char* ptr = *freeMem + numOfChunks * sizeof(buddy_chunk);
	buddy_chunk_pointer* begOfPointers = ptr;
	*freeMem = *freeMem + numOfChunks * sizeof(buddy_chunk) + numofChunkPointers * sizeof(buddy_chunk_pointer);

	while (1) {
		if (!numOfChunks) break;
		if (sizetmp >= (1 << powtmp)) {
			--numOfChunks;
			//nizBlokova[numOfChunks].mutex = CreateMutex(0, FALSE, 0);
			nizBlokova[numOfChunks].begin = beginning;
			nizBlokova[numOfChunks].sizeOfChunk = powtmp;
			nizBlokova[numOfChunks].chunkArray = begOfPointers;
			for (int i = 0; i <= powtmp; i++) {
				begOfPointers[i].sizeOfBlock = i;
				begOfPointers[i].head = NULL;
			}
			begOfPointers[powtmp].head = (buddy_chunk_block*)*freeMem;
			begOfPointers[powtmp].head->adressOfBlock = beginning;
			begOfPointers[powtmp].head->next = NULL;
			begOfPointers[powtmp].head->prev = NULL;
			begOfPointers[powtmp].head->parent = &(begOfPointers[powtmp]);
			*freeMem = *freeMem + sizeof(buddy_chunk_block);
			beginning += (1 << powtmp) * BLOCK_SIZE;
			ptr += (powtmp + 1) * sizeof(buddy_chunk_pointer);
			begOfPointers = ptr; 
			sizetmp -= (1 << powtmp);
		}
		--powtmp;
	}

	printing();
}


void* require(unsigned int size) {

	WaitForSingleObject(mainMutex,INFINITE);
	//printf("Uzimanje");
	if (size <= 0) {
		ReleaseMutex(mainMutex);
		return NULL;
	}
	char** freeMem = (char**)(buddy);
	int n = *(buddy + 2 * sizeof(char*));
	buddy_chunk* niz = buddy + sizeof(int) + 2 * sizeof(char*) + 13 * sizeof(kmem_cache_t*);
	int flag = 0;
	int j;
	int i;
	for (i = 0; i < n; ++i) {
		if (1 << niz[i].sizeOfChunk < size) continue;
		else {
			buddy_chunk_pointer* niz1 = niz[i].chunkArray;		
			for (j = 0; j <= niz[i].sizeOfChunk; ++j) {
				if (!niz1[j].head) continue;
				else {
					if (1 << niz1[j].sizeOfBlock >= size) {
						flag = 1;
						break;
					}
				}
			}
		}
		if (flag) break;
	}

	if (i == n) {
		ReleaseMutex(mainMutex);

		return NULL;
	}
	while (j != 0 && (1 << niz[i].chunkArray[j-1].sizeOfBlock) >= size) {
		buddy_chunk_block* block1 = niz[i].chunkArray[j].head;
		niz[i].chunkArray[j].head = niz[i].chunkArray[j].head->next;

		if(niz[i].chunkArray[j].head) niz[i].chunkArray[j].head->prev = NULL;

		buddy_chunk_block* block2 = *freeMem;
		*freeMem = *freeMem + sizeof(buddy_chunk_block);
		block2->adressOfBlock = block1->adressOfBlock + (1 << niz[i].chunkArray[j - 1].sizeOfBlock) * BLOCK_SIZE;
		block2->next = niz[i].chunkArray[j - 1].head;
		if(block2->next) block2->next->prev = block2;
		block2->prev = block1;
		block1->next = block2; 
		block1->prev = NULL;
		niz[i].chunkArray[j - 1].head = block1;
		block2->parent = &(niz[i].chunkArray[j - 1]);
		//block1->parent = &(niz[i].chunkArray[j - 1]);
		--j;
	} 

	buddy_chunk_block* block = niz[i].chunkArray[j].head;
	if(block->next) block->next->prev = NULL; //greska
	niz[i].chunkArray[j].head = block->next;
	block->next = NULL;

	buddy_chunk_block* blk = *freeMem - sizeof(buddy_chunk_block);
	char* ret = block->adressOfBlock;

	if (block != blk) {
		if (blk->next) blk->next->prev = block;
		if (blk->prev) blk->prev->next = block;
		if (blk->parent->head == blk) blk->parent->head = block;
		block->adressOfBlock = blk->adressOfBlock;
		block->next = blk->next;
		block->prev = blk->prev;
		block->parent = blk->parent;
	}
	*freeMem = *freeMem - sizeof(buddy_chunk_block);
	//printf("Velicina: %d\n:adr %p", size,ret);

	//printing();
	ReleaseMutex(mainMutex);
	return ret; // ako se vratilo nesto sto nije NULL to znaci da mu je dodeljen prostor unapred poznate velicine sa adresom ret 
}

buddy_chunk_block* findBuddy(char* begin, char* adress, buddy_chunk_block* poc) {
	int size = poc->parent->sizeOfBlock;
	while (poc) {		
		if ((((int)(poc->adressOfBlock - begin)/ BLOCK_SIZE) ^ ((int)(1 << size))) == ((int)((adress - begin) / BLOCK_SIZE))) return poc;
		poc = poc->next;
	}
	return NULL;
}

int find(int k) {
	int h = 0;
	while ((1 << h) < k) ++h;
	return h;
}

int checkIfAlreadzExists(buddy_chunk * niz, int n, char* adr) {
	for (int i = 0; i < n; i++) {
		for (int j = 0; j <= niz[i].sizeOfChunk; j++) {
			buddy_chunk_block* pok = niz[i].chunkArray[j].head;
			while (pok) {
				char* ptr = pok->adressOfBlock;
				if (pok->adressOfBlock <= adr && adr < (ptr + pok->parent->sizeOfBlock * BLOCK_SIZE)) return 1; // greska
				pok = pok->next;
			}
		}
	}
	return 0;
}


void returning(void* adr, int size) {
	WaitForSingleObject(mainMutex, INFINITE);
	//printing();
	if (!size || !adr || adr < buddy || adr > buddy + BLOCK_SIZE * velicina) {
		ReleaseMutex(mainMutex);
		return;
	}
	int pow = find(size);
	buddy_chunk* niz = buddy + sizeof(int) + 2 * sizeof(char*) + 13 * sizeof(kmem_cache_t*);
	int n = *(buddy + 2 * sizeof(char*));
	
	if (checkIfAlreadzExists(niz, n, adr)) {
		printf("Vec postoji\n");
		ReleaseMutex(mainMutex);
		return;
	}
	int i = 0; //finding which chunk is an owner of a block
	while (1) {
		if (i == n) {
			ReleaseMutex(mainMutex);
			return;
		}
		char* ptr = niz[i].begin;
		if (niz[i].begin <= adr && ptr + (1 << niz[i].sizeOfChunk) * BLOCK_SIZE > adr) break;
		++i;
	}
	//printf("Vracanje: %d; size:%d; adr:%p\n",i,size,adr);

	//negde nisi premestio head 
	char** freeMem = (char**)(buddy);
	buddy_chunk_block* novi = *freeMem;
	novi->next = NULL;
	novi->adressOfBlock = adr;
	novi->prev = NULL;
	novi->parent = NULL;
	while (1) {
		buddy_chunk_block* poc = niz[i].chunkArray[pow].head;
		if (poc) {

			if (pow == niz[i].sizeOfChunk) {

				break;
			}

			buddy_chunk_block* pom = findBuddy(niz[i].begin, novi->adressOfBlock, poc);
			if (!pom) {

				if (*freeMem != novi) {
					buddy_chunk_block* fr = *freeMem;
					//fr->parent->head = fr;
					fr->adressOfBlock = novi->adressOfBlock;
					fr->next = novi->next;
					fr->prev = novi->prev;
					fr->parent = novi->parent;
					fr->parent->head = fr;
					if (novi->next) novi->next->prev = fr;
					//printf("Nije nasao buddija\n");
					break;
				}
				else {
					//printf("Nije nasao buddija odmah\n");
					novi->next = poc;
					novi->parent = poc->parent;
					novi->parent->head = novi;
					poc->prev = novi;
					novi->prev = NULL;
					break;
				}
				
			}
			else {
				char* pomAdr;
				if (pom->adressOfBlock > novi->adressOfBlock) pomAdr = novi->adressOfBlock;
				else pomAdr = pom->adressOfBlock;

				*freeMem = *freeMem - sizeof(buddy_chunk_block);

				novi->adressOfBlock = pomAdr;

				buddy_chunk_block* last = *freeMem;
			    
				if (pom->next) pom->next->prev = pom->prev;
				if (pom->prev) pom->prev->next = pom->next;
				if (pom->parent->head == pom) pom->parent->head = pom->next;

				if (novi->parent) {
					if (novi->parent->head == novi) {
						novi->parent->head = novi->next;
					    if(novi->next) novi->next->prev = NULL; 
					}
				}

				novi->next = niz[i].chunkArray[pow + 1].head;
				if(novi->next) novi->next->prev = novi;
				niz[i].chunkArray[pow + 1].head = novi;
				novi->parent = &(niz[i].chunkArray[pow + 1]);
				
				if (pom != last) {
					if (last->next) last->next->prev = pom;
					if (last->prev) last->prev->next = pom;
					if (last->parent->head == last) last->parent->head = pom;

					//*pom = *last;

					pom->adressOfBlock = last->adressOfBlock;
					pom->next = last->next;
					pom->prev = last->prev;
					pom->parent = last->parent;
					//printf("Nasao buddija\n");
				}
			}
		}
		else {
			//printf("Ne postoji nista na pocetu\n");
			niz[i].chunkArray[pow].head = novi;
			novi->next = NULL;
			novi->prev = NULL;
			novi->parent = &(niz[i].chunkArray[pow]);
			novi->parent->head = novi;
			break;
		}
		++pow;
	}

	if (novi != *freeMem) {
		if (novi->next) novi->next->prev = *freeMem;
		if (novi->prev) novi->prev->next = *freeMem;
		if (novi->parent->head == novi) novi->parent->head = *freeMem;
		buddy_chunk_block *pom = *freeMem;
		pom->adressOfBlock = novi->adressOfBlock;
		pom->next = novi->next;
		pom->prev = novi->prev;
		pom->parent = novi->parent;
	}
	*freeMem = *freeMem + sizeof(buddy_chunk_block);
	//int br = printing();
	ReleaseMutex(mainMutex);
}

/*void* require(int size) {

	WaitForSingleObject(mainMutex, INFINITE);
	char** freeMem = (char**)(buddy);
	int n = *(buddy + 2 * sizeof(char*));

	buddy_chunk* chunks = buddy + 2 * sizeof(char*) + sizeof(int) + 13 * sizeof(kmem_cache_t*);
	int found = 0;
	int i = 0;
	int j = 0;
	for (i = 0; i < n; i++) {

		if (1 << chunks[i].sizeOfChunk < size) continue;

		buddy_chunk_pointer* chunk_ptr = chunks[i].chunkArray;
		for (j = 0; j <= chunks[i].sizeOfChunk; j++) {

			if (chunk_ptr[j].head != NULL && (1 << chunk_ptr[j].sizeOfBlock) >= size) {
				found = 1;
				break;
			}
		}
		if (found == 1) break;
	}

	if (found == 0) {
		ReleaseMutex(mainMutex);
		return NULL;
	}


	buddy_chunk_block* block1 = chunks[i].chunkArray[j].head;

	while (1) {
		if (j == 0 || (1 << chunks[i].chunkArray[j - 1].sizeOfBlock) < size) {
			break;
		}

		block1 = chunks[i].chunkArray[j].head;
		chunks[i].chunkArray[j].head = chunks[i].chunkArray[j].head->next;
		if (chunks[i].chunkArray[j].head) chunks[i].chunkArray[j].head->prev = NULL;

		buddy_chunk_block* block2 = *freeMem;
		block2->adressOfBlock = block1->adressOfBlock + (1 << chunks[i].chunkArray[j - 1].sizeOfBlock) * BLOCK_SIZE;
		block2->next = chunks[i].chunkArray[j - 1].head;
		if(block2->next) block2->next->prev = block2;
		block1->next = block2;
		block2->prev = block1;
		block1->prev = NULL;
		block2->parent = &(chunks[i].chunkArray[j - 1]);
		block1->parent = &(chunks[i].chunkArray[j - 1]);

		chunks[i].chunkArray[j - 1].head = block1;
		*freeMem = *freeMem + sizeof(buddy_chunk_block);
		--j;
	}

	block1->parent->head = block1->next;
	if(block1->next) block1->next->prev = NULL;

	char* ret = block1->adressOfBlock;
	if (block1 != *freeMem) {
		buddy_chunk_block* ptr = *freeMem - sizeof(buddy_chunk_block);
		block1->adressOfBlock = ptr->adressOfBlock;
		block1->next = ptr->next;
		block1->prev = ptr->prev;
		block1->parent = ptr->parent;

		if (ptr->next) ptr->next->prev = block1;
		if (ptr->prev) ptr->prev->next = block1;
		if (ptr->parent->head == ptr) ptr->parent->head = block1;

	}

	*freeMem = *freeMem - sizeof(buddy_chunk_block);
	printf("Uzimanje: %d: size:%d adr:%p\n", i, size, ret);
	printing();
	ReleaseMutex(mainMutex);
	return ret;
}*/