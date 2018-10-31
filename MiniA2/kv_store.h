#ifndef kv_store_h
#define kv_store_h

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//TODO: Define proper values instead of arbitrary ones here

#define KEY_SIZE 32
#define VAL_SIZE 256

#define NUM_PODS 128
#define RECORDS_PER_POD 128

#define KV_STORE_SIZE (NUM_PODS * RECORDS_PER_POD * (KEY_SIZE + VAL_SIZE))

typedef struct kv_pair {
	char key[KEY_SIZE];
	char val[VAL_SIZE];
} kv_pair;

typedef struct pod {
	//unsigned long pod_id;
	int curr_pair;
	kv_pair kv_pairs[RECORDS_PER_POD];
} pod;

typedef struct kv_store {
	char *name;
	// TODO: Do I want this to be an array of pod pointers?
	pod pods[NUM_PODS];
} kv_store;

kv_store* my_kv_store;
sem_t* sem;
char *kv_store_name;


unsigned long hash(unsigned char *str);


int kv_store_create(char *name);
int kv_store_write(char *key, char *value);
char *kv_store_read(const char *key);
char **kv_store_read_all(const char *key);


#endif
