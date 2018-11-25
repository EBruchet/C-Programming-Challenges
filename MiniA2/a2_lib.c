#include "a2_lib.h"
int kv_store_create(char *kv_store_name){
	// Create SMO if it doesn't already exist, otherwise just opens it (???)
	// S_IRWXU provides owner process with R/W, search/execute privileges
	int fd = shm_open(__KV_STORE_NAME__, O_CREAT | O_RDWR, S_IRWXU);

	if(fd < 0){
		printf("Error in opening shared memory location\n");
		return KV_EXIT_FAILURE;
	}

	ftruncate(fd, sizeof(kv_store));
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

	if(strlen(key) >= KEY_SIZE){
		*(key + KEY_SIZE)  = '\0';
	}

	if(strlen(value) >= VAL_SIZE){
		*(value + VAL_SIZE) = '\0';
	}
	
	int pod_index = generate_hash(key) % NUM_PODS;

	// Initialized semaphore value to 1 so the first process to create it
	// will be able to use it right away - I THINK
	sem = sem_open(__KV_WRITERS_SEMAPHORE__, O_CREAT | O_RDWR, S_IRWXU, 1);
	sem_wait(sem);
	// Try to decrement the current semaphore value
	// If > 0, then decrement succees
	// If == 0, then thread blocks trying to decrement until woken up (???)
	
	/* START: CRITICAL REGION OF CODE*/
	
	// Get a pointer to the pod allocated for this key
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
		sel_key = & (sel_pod->keys[sel_pod->next_key_write]);
		// Clear all bookkeeping and value information for the key in memory
		memset(sel_key->values, 0, sizeof(sel_key->values));
		sel_key->num_vals = 0;
		sel_key->next_val_write = 0;
		sel_key->next_val_read = 0;
		sel_val = & (sel_key->values[sel_key->next_val_write]);

		// Write the key and value names into memory
		memcpy(&sel_key->key_name, key, strlen(key));
		memcpy(&sel_val->val_name, value, strlen(value));
		
	
		int next_key_write_position = sel_pod->next_key_write;
		int next_val_write_position = sel_key->next_val_write;
		sel_pod->next_key_write = (int) (next_key_write_position + 1) % NUM_KEYS_PER_POD;
		sel_key->next_val_write = (int) (next_val_write_position + 1) % NUM_VALS_PER_KEY;
		sel_key->num_vals++;
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
		if(sel_key->num_vals < NUM_VALS_PER_KEY){
			sel_key->num_vals++;
		}
	}

	/* END: CRITICAL REGION OF CODE */
	
	clean_up(sem, my_kv_store, fd);
	return KV_EXIT_SUCCESS;
}

char *kv_store_read(char *key){
	sem_t *sem_read;
	kv_store *my_kv_store;
	char * returned_pointer = NULL;
	int fd = shm_open(__KV_STORE_NAME__, O_CREAT | O_RDWR, S_IRWXU);
	my_kv_store = mmap(NULL, sizeof(kv_store), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

	if(key == NULL || *key == '\0'){
		printf("Key was either null or pointed to an empty array of characters.");
		return NULL;
	}
	
	// Truncate key to maximum possible length
	if(strlen(key) >= KEY_SIZE){
		*(key + KEY_SIZE)  = '\0';
	}

	// Find pod index that the passed key would be found in
	int pod_index = generate_hash(key) % NUM_PODS;

	// Open or create the semaphore
	sem_read = sem_open(__KV_READERS_SEMAPHORE__, O_CREAT | O_RDWR, S_IRWXU, 1);
	sem_wait(sem_read);

	/* START: CRITICAL REGION OF CODE */

	// Get the pod relevant to the passed key
	pod *sel_pod = &(my_kv_store->pods[pod_index]);
	int found_key_index = -1;

	// Loop through all keys to see if key exists within this pod
	for(int i = 0; i < NUM_KEYS_PER_POD; i++){
		kv_key *curr_key = & (sel_pod->keys[i]);
		if(strcmp(curr_key->key_name, key) == 0){
			found_key_index = i;
			break;
		}
	}

	// Key does not exist inside of pod, return NULL
	if(found_key_index < 0){
		clean_up(sem_read, my_kv_store, fd);
		return NULL;
	} 
	// Key exists inside of pod, access next value to read and return it
	else{
		kv_key *sel_key = & (sel_pod->keys[found_key_index]);
		val *sel_val = & (sel_key->values[sel_key->next_val_read]);
		
		returned_pointer = strdup(sel_val->val_name);
		int next_val_write = sel_key->next_val_write;
		int next_val_read = sel_key->next_val_read;
		sel_key->next_val_read = (int)(next_val_read + 1) % sel_key -> num_vals;
	}

	/*END: CRITICAL SECTION*/
	clean_up(sem_read, my_kv_store, fd);
	return returned_pointer;
}

char **kv_store_read_all(char *key){
	sem_t *sem_read;
	kv_store *my_kv_store;
	int fd = shm_open(__KV_STORE_NAME__, O_CREAT|O_RDWR, S_IRWXU);
	char **returned_values = malloc((NUM_VALS_PER_KEY + 1) * sizeof(char *));
	my_kv_store = mmap(NULL, sizeof(kv_store), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

	if(key == NULL || *key == '\0'){
		printf("Key was either null or pointed to an empty array of characters.");
		clean_up(sem_read, my_kv_store, fd);
		return NULL;
	}
	
	int pod_index = generate_hash(key) % NUM_PODS;

	sem_read = sem_open(__KV_READERS_SEMAPHORE__, O_CREAT | O_RDWR, S_IRWXU, 1);
	sem_wait(sem_read);

	pod *sel_pod = & (my_kv_store->pods[pod_index]);
	int num_found_vals = 0;

	// Loop through all keys in the selected pod, checking if the key name
	// matches the one passes as input
	int found_key_index = -1;
	for(int i = 0; i < NUM_KEYS_PER_POD; i++){
		kv_key *curr_key = & (sel_pod->keys[i]);
		if(strcmp(curr_key->key_name, key) == 0){
			found_key_index = i;
			break;
		}
	}

	// If the key was not found, return NULL
	if(found_key_index < 0){
		clean_up(sem_read, my_kv_store, fd);
		return NULL;
	} 
	// If the key was found, add all the values associated to that key to the char **
	// returned_values
	else{
		kv_key *sel_key = & (sel_pod->keys[found_key_index]);
		for(int i = 0; i < sel_key->num_vals; i++){
			val *curr_val = & (sel_key->values[i]);
			if(strlen(curr_val->val_name) == 0){
				returned_values[i] = NULL;
				break;
			}
			returned_values[i] = malloc(sizeof(curr_val->val_name));
			strcpy(returned_values[i], curr_val->val_name);
		}
		// Null terminate the list of values
		returned_values[sel_key->num_vals] = NULL;
	}
	clean_up(sem_read, my_kv_store, fd);
	return returned_values;
}
