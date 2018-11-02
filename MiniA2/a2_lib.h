#ifndef __A2_LIB_HEADER__
#define __A2_LIB_HEADER__

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>

/* -------------------------------------
	Define your own globals here
----------------------------------------*/

#define __KEY_VALUE_STORE_SIZE__	50*1024*1024        			//50MB
#define __KV_WRITERS_SEMAPHORE__	"/WRITER_EVANBRUCHET_260613457"
#define __KV_READERS_SEMAPHORE__	"/READER_EVANBRUCHET_260613457" 
#define __KV_STORE_NAME__			"KV_STORE_EVANBRUCHET_260613457"
#define KV_EXIT_SUCCESS				0
#define KV_EXIT_FAILURE				-1

#define KEY_SIZE 32
#define VAL_SIZE 256
#define NUM_VALS_PER_KEY 256
#define NUM_PODS 512
#define NUM_KEYS_PER_POD 5

unsigned long generate_hash(unsigned char *str);

int kv_store_create(char *kv_store_name);
int kv_store_write(char *key, char *value);
char *kv_store_read(char *key);
char **kv_store_read_all(char *key);


typedef struct val {
	char val_name[VAL_SIZE];
} val;

typedef struct kv_key {
	int num_vals;
	int next_val_read;
	int next_val_write;
	char key_name[KEY_SIZE];
	val values[NUM_VALS_PER_KEY];
} kv_key;

typedef struct pod {
	int next_key_write;
	kv_key keys[NUM_KEYS_PER_POD];
} pod;

typedef struct kv_store {
	pod pods[NUM_PODS];
} kv_store;

void clean_up(sem_t *sem, kv_store *my_kv_store, int fd);
#endif
