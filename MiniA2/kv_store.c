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
	unsigned long pod_id;
	int curr_pair;
	kv_pair kv_pairs[RECORDS_PER_POD];
} pod;

typedef struct kv_store {
	char *name;
	// TODO: Do I want this to be an array of pod pointers?
	pod pods[NUM_PODS];
} kv_store;

kv_store *my_kv_store;
sem_t *sem;

unsigned long hash(unsigned char *str);


int kv_store_create(char *name);
int kv_store_write(char *key, char *value);
char *kv_store_read(char *key);
char **kv_store_read_all(char *key);

int main(int argc, char *argv[]) {
	
	if(argc > 1){
		char *string_ptr = argv[1];
		printf("Input: %s\n", string_ptr);
		unsigned long hash_output = hash(string_ptr);
		printf("Output: %lu\n", hash_output);
	} else{
		printf("Provide an argument next time idiot\n");
	}
	
	return 0;
}


// Hashes an input string into a number
unsigned long hash(unsigned char *str) {
	unsigned long hash = 5381;
	int counter;

	while(counter = *str++){
		hash = counter + ((hash << 5) + hash);
	}

	return ((hash > 0 ) ? hash : -(hash));
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

	my_kv_store = mmap(NULL, sizeof(kv_store), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

	if(my_kv_store < 0){
		printf("mmap execution failed");
		return -1;
	}

	ftruncate(fd, sizeof(kv_store));

	my_kv_store->name = name;

	// Deletes the mappings for the KV_Store in this process, saves space/power (???)
	munmap(my_kv_store, sizeof(kv_store));
	close(fd);
	return 0;
}

int kv_store_write(char *key, char *value){
	if(my_kv_store == NULL){
		printf("KV_Store was not created - there is no KV_Store to write to.");
		return -1;
	} else if(key == NULL || *key == '\0'){
		printf("Key was either null or pointed to an empty array of characters.");
		return -1;
	} else if(value == NULL || *value == '\0'){
		printf("Value was either null or pointed to an empty array of characters.");
	}


	if(strlen(key) >= KEY_SIZE){
		*(key + KEY_SIZE + 1) = '\0';			
	}

	if(strlen(value) >= VAL_SIZE){
		*(value + VAL_SIZE + 1) = '\0';
	}

	int pod_index = hash(key) % RECORDS_PER_POD;

	// Initialized semaphore value to 1 so the first process to create it
	// will be able to use it right away - I THINK
	sem = sem_open("/semname", O_CREAT | O_RDWR, S_IRWXU, 1);

	sem_wait(sem);
	// Try to decrement the current semaphore value
	// If > 0, then decrement succees
	// If == 0, then thread blocks trying to decrement until woken up (???)
	
	/* START: CRITICAL REGION OF CODE*/
	
	// Get a pointer to the selected pod for this KV-Pair
	pod *selected_pod = & (my_kv_store->pods[pod_index]);
	// Set pointer for the next kv_pair in the array to be written to
	kv_pair *sel_kv_pair = & (selected_pod->kv_pairs[selected_pod->curr_pair]);

	// Write the key value into memory at the selected
	//
	memcpy(&sel_kv_pair->key, key, strlen(key));
	memcpy(&sel_kv_pair->val, value, strlen(value));

	// Writes to the array of KV-Pairs on a FIFO basis, by wrapping around
	// when we reach the end of the array
	int curr_pair = selected_pod->curr_pair;
	selected_pod->curr_pair = (int) (curr_pair + 1) % RECORDS_PER_POD;


	sem_post(sem);

	/* END: CRITICAL REGION OF CODE */

}

char *kv_store_read(char *key){

	if(key == NULL || *key == '\0'){
		printf("Key was either null or pointed to an empty array of characters.");
		return -1;
	} 

	// TODO: What to do if the key is bigger than maximum allowed size?
	// Option 1: Truncate it and check for the truncated string
	// Option 2: Exit program with error message "Invalid string"

	int pod_index = hash(key) % RECORDS_PER_POD;

	// Open or create the semaphore
	sem = sem_open("/semname", O_CREAT | O_RDWR, S_IRUSR, 1);

	sem_wait(sem);

	pod *selected_pod = &(my_kv_store->pods[pod_index]);


	for(int i = 0; i < RECORDS_PER_POD; i++){
		kv_pair *sel_kv_pair = &(selected_pod->kv_pairs[i]);
		
		
	
	}
	
}
