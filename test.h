#pragma once

#include "cache.h"

#define MASK (0xA5)

struct data_s {
	int id;
	kmem_cache_t* shared;
	int iterations;
};


void run_threads(void(*work)(void*), struct data_s* data, int num);