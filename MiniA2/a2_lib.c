#include "a2_lib.h"

typedef struct val {
	char val_name[VAL_SIZE];
} val;

typedef struct kv_key {
	int next_val_read;
	int next_val_write;
	char key_name[KEY_SIZE];
	val values[NUM_VALS_PER_KEY];
} kv_key;

typedef struct pod {
	int next_key_write_position;
	kv_key keys[NUM_KEYS_PER_POD];
} pod;

typedef struct kv_store {
	pod pods[NUM_PODS];
} kv_store;

int kv_store_create(char *kv_store_name){
	int fd;
	kv_store *my_kv_store;
	// Create SMO if it doesn't already exist, otherwise just opens it (???)
	// S_IRWXU provides owner process with R/W, search/execute privileges
	fd = shm_open(__KV_STORE_NAME__, O_CREAT|O_RDWR, S_IRWXU);

	if(fd < 0){
		printf("Error in opening shared memory location\n");
		return KV_EXIT_FAILURE;
	}

	my_kv_store = mmap(NULL, sizeof(kv_store), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

	if(my_kv_store < 0){
		printf("mmap execution failed");
		return KV_EXIT_FAILURE;
	}

	ftruncate(fd, sizeof(kv_store));

	// Deletes the mappings for the KV_Store in this process, saves space/power (???)
	munmap(my_kv_store, sizeof(kv_store));
	close(fd);
	return KV_EXIT_SUCCESS;
}

int kv_store_write(char *key, char *value){
	int fd = shm_open(__KV_STORE_NAME__, O_CREAT|O_RDWR, S_IRWXU);
	sem_t *sem;
	kv_store *my_kv_store;
	my_kv_store = mmap(NULL, sizeof(kv_store), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	
	if(my_kv_store == NULL){
		printf("KV_Store was not created - there is no KV_Store to write to.");
		return KV_EXIT_FAILURE;
	} else if(key == NULL || *key == '\0'){
		printf("Key was either null or pointed to an empty array of characters.");
		return KV_EXIT_FAILURE;
	} else if(value == NULL || *value == '\0'){
		printf("Value was either null or pointed to an empty array of characters.");
		return KV_EXIT_FAILURE;
	}


	//TODO: FIX DA FUCKING TING
	if(strlen(key) >= KEY_SIZE)
		*(key + KEY_SIZE) = '\0';			
	

	if(strlen(value) >= VAL_SIZE)
		*(value + VAL_SIZE) = '\0';
	
	int pod_index = generate_hash(key) % NUM_PODS;

	// Initialized semaphore value to 1 so the first process to create it
	// will be able to use it right away - I THINK
	sem = sem_open(__KV_WRITERS_SEMAPHORE__, O_CREAT | O_RDWR, S_IRWXU, 1);
	sem_wait(sem);
	// Try to decrement the current semaphore value
	// If > 0, then decrement succees
	// If == 0, then thread blocks trying to decrement until woken up (???)
	
	/* START: CRITICAL REGION OF CODE*/
	
	// Get a pointer to the selected pod for this KV-Pair
	pod *sel_pod = & (my_kv_store->pods[pod_index]);
	int sel_key_index = -1;
	for(int i = 0; i < NUM_KEYS_PER_POD; i++){
		kv_key *curr_key = &(sel_pod->keys[i]);
		if(strcmp(curr_key->key_name, key) == 0){
			sel_key_index = i;
			break;
		}
	}

	kv_key *sel_key = NULL;
	val *sel_val = NULL;

	// If the key is not already in the KV-Store, try to insert it
	if(sel_key_index < 0){
		sel_key = & (sel_pod->keys[sel_pod->next_key_write_position]);
		sel_val = & (sel_key->values[sel_key->next_val_write]);

		memcpy(&sel_key->key_name, key, strlen(key));
		memcpy(&sel_val->val_name, value, strlen(value));

		int next_key_write_position = sel_pod->next_key_write_position;
		sel_pod->next_key_write_position = (int) (next_key_write_position + 1) % NUM_KEYS_PER_POD;

		int next_val_write_position = sel_key->next_val_write;
		sel_key->next_val_write = (int) (next_val_write_position + 1) % NUM_VALS_PER_KEY;

	}
	// The key is arleady in the store, append the value to the existing list of values for the key
	else{
		sel_key = & (sel_pod->keys[sel_key_index]);
		sel_val = & (sel_key->values[sel_key->next_val_write]);

		// Store the value into the key's list of values
		memcpy(&sel_val->val_name, value, strlen(value));

		// Increment the index that points to the next write position for a value to a key
		int next_val_write_position = sel_key->next_val_write;
		sel_key->next_val_write = (int) (next_val_write_position + 1) % NUM_VALS_PER_KEY;
	}

	sem_post(sem);
	/* END: CRITICAL REGION OF CODE */

	munmap(my_kv_store, sizeof(kv_store));
	close(fd);
	return KV_EXIT_SUCCESS;
}

char *kv_store_read(char *key){
	char *returned_pointer = NULL;
	sem_t *sem;
	int fd = shm_open(__KV_STORE_NAME__, O_CREAT|O_RDWR, S_IRWXU);
	kv_store *my_kv_store;
	my_kv_store = mmap(NULL, sizeof(kv_store), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

	if(key == NULL || *key == '\0'){
		printf("Key was either null or pointed to an empty array of characters.");
		return returned_pointer;
	}

	// Option 1: Truncate it and check for the truncated string
	// Option 2: Exit program with error message "Invalid string"

	int pod_index = generate_hash(key) % NUM_PODS;

	// Open or create the semaphore
	sem = sem_open(__KV_READERS_SEMAPHORE__, O_CREAT | O_RDWR, S_IRWXU, 1);
	sem_wait(sem);

	/* START: CRITICAL REGION OF CODE */
	// Get the pod relevant to the passed key
	pod *sel_pod = &(my_kv_store->pods[pod_index]);
	int found_key_index = -1;
	for(int i = 0; i < NUM_KEYS_PER_POD; i++){
		kv_key *curr_key = & (sel_pod->keys[i]);
		if(strcmp(curr_key->key_name, key) == 0){
			found_key_index = i;
			break;
		}
	}

	if(found_key_index < 0){
		return returned_pointer;
	} else{
		kv_key *sel_key = & (sel_pod->keys[found_key_index]);
		val *sel_val = & (sel_key->values[sel_key->next_val_read]);
		returned_pointer = strdup(sel_val->val_name);
		int next_val_read = sel_key->next_val_read;
		sel_key->next_val_read = (int)(next_val_read + 1) % NUM_VALS_PER_KEY;
	}


	sem_post(sem);
	munmap(my_kv_store, sizeof(kv_store));
	close(fd);

	return returned_pointer;
}

char **kv_store_read_all(char *key){
	// implement your create method code here
	/*char **returned_array = NULL;
	sem_t *sem;
	int fd = shm_open(__KV_STORE_NAME__, O_CREAT|O_RDWR, S_IRWXU);
	kv_store *my_kv_store;
	my_kv_store = mmap(NULL, sizeof(kv_store), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

	if(key == NULL || *key == '\0'){
		printf("Key was either null or pointed to an empty array of characters.");
		return NULL;
	}
	
	int pod_index = generate_hash(key) % RECORDS_PER_POD;

	sem = sem_open(__KV_READERS_SEMAPHORE__, O_CREAT | O_RDWR, S_IRWXU, 1);
	sem_wait(sem);

	pod *selected_pod = &(my_kv_store->pods[pod_index]);
	int num_found_pairs = 0;

	for(int i = 0; i < RECORDS_PER_POD; i++){
		kv_pair *sel_kv_pair = &(selected_pod->kv_pairs[i]);
		char *found_key = sel_kv_pair->key;
		if(strcmp(found_key, key) == 0){
			num_found_pairs++;
		}
	}

	char **returned_values = (char **) malloc((num_found_pairs + 1) *sizeof(char*));

	for(int i = 0; i < num_found_pairs; i++){
		kv_pair *sel_kv_pair = &(selected_pod->kv_pairs[i]);
		char *found_key = sel_kv_pair->key;
		if(strcmp(found_key, key) == 0){
			for(int j = 0; j < KEY_SIZE; j++){
				returned_values[i] = strdup(sel_kv_pair->val);				
			}
		}
	}

	sem_post(sem);
	munmap(my_kv_store, sizeof(kv_store));
	close(fd);
	*/
	return NULL;
}


/* -------------------------------------------------------------------------------
	MY MAIN:: Use it if you want to test your impementation (the above methods)
	with some simple tests as you go on implementing it (without using the tester)
	-------------------------------------------------------------------------------
*/
/*int main(int argc, char *argv[]) {
	kv_store_create(__KV_STORE_NAME__);
	//kv_store_write(argv[1], argv[2]);
	char *fk_u_feras = kv_store_read(argv[1]);
	printf("Found: %s", fk_u_feras);
	return EXIT_SUCCESS;
}*/
