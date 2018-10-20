#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
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
	char key[32];
	char val[256];
} kv_pair;

typedef struct pod {
	int pod_id;
	kv_pair *kv_pairs[RECORDS_PER_POD];
} pod;

typedef struct kv_store {
	char *name;
	pod pods[NUM_PODS];
} kv_store;

kv_store *my_kv_store;

unsigned long hash(unsigned char *str);


int kv_store_create(char *name);
int kv_store_write(char *key, char *value);
char *kv_store_read(char *key);
char **kv_store_read_all(char *key);


int main() {
	

	return 0;
}


// Hashes an input string into a number
unsigned long hash(unsigned char *str) {
	unsigned long hash = 5381;
	int counter;

	while(counter = *str++){
		hash= counter + ((hash << 5) + hash);
	}

	return (hash > 0) ? hash : -(hash);

}

int kv_store_create(char *name){
	int fd; 

	// Create SMO if it doesn't already exist, otherwise just opens it (???)
	// S_IRWXU provides owner process with R/W, search/execute privileges
	fd = shm_open(name, O_CREAT|O_RDWR, S_IRWXU);

	if(fd < 0){
		printf("Error in opening shared memory location\n");
		return -1;
	}

	my_kv_store = mmap(NULL, KV_STORE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

	if(my_kv_store < 0){
		printf("mmap execution failed");
		return -1;
	}

	ftruncate(fd, KV_STORE_SIZE);


	//TODO: Check wtf this does and if it's necessary?
	// close(fd);
	return 0;
}
