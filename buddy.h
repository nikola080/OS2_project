#pragma once

#include<stdio.h>
#include<stdlib.h>
#include<windows.h>

#define BLOCK_SIZE (50)
#define shared_size (7)
char* buddy;


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
	HANDLE mutex;
}buddy_chunk; // "chunk", velicina bloka koja izgradjuje memoriju(velcina je 2^(pozocija gde je 1 u binarnoj
			  // predstavi), koja sadrzi pokazivac na svoj niz koji ima pokzivace na blokove odredjenog stepen u 
			  // zavisnosti od pozicije, ukratk	o ovo je jedan buddy alokator


void printing();
int isNotPowerTwo(int number);
void kmem_init(void* space, int size);
void* require(unsigned int size);
buddy_chunk_block* findBuddy(char* begin, char* adress, buddy_chunk_block* poc);
int find(int k);
void returning(void* adr, int size);