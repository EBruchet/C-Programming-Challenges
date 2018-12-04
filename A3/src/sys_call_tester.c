#define _GNU_SOURCE

#include <linux/mempolicy.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <assert.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <numaif.h>

int clone_func();

int main(int argc, char *argv[]){
	int option = 0;
	int i = 0;
	option = getopt(argc, argv, "upmiolhUG");
	int pid = getpid();
	switch (option){
		

		// These flag names are ass

		case 'u':{
			printf("Unshare\n");
			unshare(CLONE_NEWUSER);
			break;
		}
		case 'F':{
			printf("Unshare\n");
			unshare(CLONE_FS);
			break;
		}

		case 'p':{
			printf("Ptrace\n");
			ptrace(PTRACE_ATTACH, pid, NULL, NULL); 
			break;
		}
		case 'm':{
			printf("Mbind\n");
			int shmid = shmget(1, 1024, IPC_CREAT | 0666 | SHM_HUGETLB);
			char *buf = shmat(1, 0, 0);

			assert(argc == 2);
			int preferred_node = atoi(argv[1]);
			const unsigned long nodemask = (1 << preferred_node);
			printf("\tNodemask = %ld\n", nodemask);

			mbind(buf, 1000,
					MPOL_BIND,		/** < A strict assignment to nodes in nodemask */
					&nodemask,		/** < A bitmask of valid NUMA nodes */
					32,				/** < Num. of bits to consider. XXX: 2 does not work */
					0);
			break;
		}
		case 'i':{
			printf("Migrate\n");
			unsigned long maxnode = 0;
			unsigned long *old_nodes = NULL;
			unsigned long *new_nodes = NULL;
			migrate_pages(pid, maxnode, old_nodes, new_nodes);
			break;
		}
		case 'o':{
			printf("Move\n");
			unsigned long count = 1;
			void **pages = NULL;
			int *nodes = NULL;
			int *status = NULL;
			int flags = MPOL_MF_MOVE;
			
			move_pages(pid, count, pages, nodes, status, flags);
			break;
		}
		case 'l':{
			printf("Clone with Invalid Flag\n");
			char *stack;
			char *stack_top;
			stack = malloc(1024 * 1024);
			if(stack == NULL){
				printf("Error allocating memory to stack with malloc. \n");
			}
			
			stack_top = stack + (1024 * 1024);
			int clone_flags = CLONE_NEWUSER;

			clone(clone_func, stack_top, clone_flags, NULL);
			break;
		}
		case 'v':{
			printf("Clone with Valid Flag (NEWIPC)\n");
			char *stack;
			char *stack_top;
			stack = malloc(1024 * 1024);
			if(stack == NULL){
				printf("Error allocating memory to stack with malloc. \n");
			}
			
			stack_top = stack + (1024 * 1024);
			int clone_flags = CLONE_NEWIPC;

			clone(clone_func, stack_top, clone_flags, NULL);
			break;
		}
		case 'h':{
			printf("Chmod Both Flags\n");
			chmod("/example/path", S_ISUID | S_ISGID);
			break;
		}
		case 'U':{
			printf("Chmod UID Flag\n");
			chmod("/example/path", S_ISUID);
			break;
		}
		case 'G':{
			printf("Chmod GID Flag\n");
			chmod("/example/path", S_ISGID);
			break;
		}
		case 'R':{
			printf("Chmod IRUSR Flag\n");
			chmod("/example/path", S_IRUSR);
			break;
		}
		case 'W':{
			printf("Chmod IWUSR Flag\n");
			chmod("/example/path", S_IWUSR);
			break;
		}
			
	}

	while(1);
}

int clone_func(){
	printf("Change chhhhroot");
}
