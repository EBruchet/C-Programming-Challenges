/**
 *  @title      :   sr_container_helpers.c
 *  @author     :   Shabir Abdul Samadh (shabirmean@cs.mcgill.ca)
 *  @purpose    :   COMP310/ECSE427 Operating Systems (Assingment 3) - Phase 2
 *  @description:   Fill in the following methods
 *                      switch_child_root()
 *                      setup_child_capabilities()
 *                      setup_syscall_filters()
 *                  No other changes are needed to this file
*/
#include "sr_container.h"

int switch_child_root(const char *new_root, const char *put_old)
{
    /**
     *  ------------------------ TODO ------------------------
     *  Simply use the "pivot_root()" system call to switch child's root to the new root
     *  ------------------------------------------------------
     * */ 
    // pivot_root(new_root, put_old);
    syscall(SYS_pivot_root, new_root, put_old);
    return 0;
}

/**
 * ------------------------ TODO ------------------------
 *      This method is for altering child capabilities
 *      Complete this method as described in the assingment handout to disable a list of capabilities
 * ------------------------------------------------------
 **/ 
int setup_child_capabilities()
{
    /**
     *  Follow these steps to check if you successfully set the capabilities
     *      Copy the binary 'capsh' found inside the [/sbin] folder of the docker container
     *          into the [/sbin] folder of the 'rootfs' you downloaded to run containers
     *              cp /sbin/capsh $ROOTFS/sbin/
     *  
     *  Now if you run 'capsh --print' without this method implemented the output for [Bounding set]
     *      will indicate many capabilities. But after properly implementing this method if you run the same
     *      command inside your container you will see a smaller set of capabilities for [Bounding set]
     **/
    int drop_caps[] = {CAP_AUDIT_CONTROL, CAP_AUDIT_READ, CAP_AUDIT_WRITE,
                        CAP_BLOCK_SUSPEND, CAP_DAC_READ_SEARCH, CAP_FSETID, CAP_IPC_LOCK,
                        CAP_MAC_ADMIN, CAP_MAC_OVERRIDE, CAP_MKNOD, CAP_SETFCAP,
                        CAP_SYSLOG, CAP_SYS_ADMIN, CAP_SYS_BOOT, CAP_SYS_MODULE,
                        CAP_SYS_NICE, CAP_SYS_RAWIO, CAP_SYS_RESOURCE, CAP_SYS_TIME,
                        CAP_WAKE_ALARM};
    size_t num_caps_to_drop = 20;

    // STEP(1) :: Dropping the capability from the AMBIENT set
    for (size_t i = 0; i < num_caps_to_drop; i++)
    {
        if(prctl(PR_CAPBSET_DROP, drop_caps[i], 0, 0, 0)){
            fprintf(stderr, "prctl failed: %m\n");
            return 1;
        }
    } 

    cap_t caps = cap_get_proc();
    if(caps == NULL) {
        perror("cap_get_proc");
        if(caps){
            cap_free(caps);
        }
        return EXIT_FAILURE;
    }

    // STEP(2) :: Now from the above caps clear our 2 capabilities from the INHERITED set
    int clear_inh_set = cap_set_flag(caps, CAP_INHERITABLE, num_caps_to_drop, drop_caps, CAP_CLEAR);
    if(clear_inh_set){
        perror("cap_set_flag");
        cap_free(caps);
        return EXIT_FAILURE;
    }

    // Now set the cleared caps-structure as the processes new capability set
    int set_cap_set = cap_set_proc(caps);
    if(set_cap_set){
        perror("cap_set_proc");
        cap_free(caps);
        return EXIT_FAILURE;
    }
    
    cap_free(caps);

    return 0;
}

/**
 * ------------------------ TODO ------------------------
 *      This method is for restricting system_calls from within the container
 *      Complete this method as described in the assingment handout to restrict a list of system calls
 * ------------------------------------------------------
 **/ 
int setup_syscall_filters()
{
    scmp_filter_ctx seccomp_ctx = seccomp_init(SCMP_ACT_ALLOW);     
    if (!seccomp_ctx) {
        fprintf(stderr, "seccomp initialization failed: %m\n");
        return EXIT_FAILURE;
    }

    /**
    *  Now we must set up filters for certain system calls for which we want different behavior
    *      Lets say we want to block the following system_calls. As in we want to kill any thread that tries to do them
    *          move_pages  - moves memory pages of a process to different blocks
    *          ptrace      - observe, track and control execution of another process
    * 
    *          seccomp_rule_add() is the method used to add a filtering rule
    *          SCMP_FAIL is the action to take when there is match on this filter (IE. KILL the thread - no mercy!!!)
    *          SCMP_SYS(move_pages) -  the 3rd argument is the system-call number to which this filter applies. 
    *                                  we use the SCMP_SYS() macro to get the correct number based on the underlying architecture
    *          4th argument - You use this argument if you dont want to capture all calls to the system call but want to capture
    *                          calls only when the system call has certain matching arguments.
    *                          Ex: the read() system call is called with the first argument being 0 (STDOUT)
    **/
    /*------ MOVE_PAGES RULE ------*/
    int filter_set_status = seccomp_rule_add(
                                                seccomp_ctx,            // the context to which the rule applies
                                                SCMP_FAIL,          // action to take on rule match
                                                SCMP_SYS(move_pages),   // get the sys_call number using SCMP_SYS() macro
                                                0                       // any additional argument matches
                                            );
    if (filter_set_status) {
        if (seccomp_ctx)
            seccomp_release(seccomp_ctx);
        fprintf(stderr, "seccomp could not add KILL rule for 'move_pages': %m\n");
        return EXIT_FAILURE;
    }

    /*------ MBIND RULE ------*/
    filter_set_status = seccomp_rule_add(seccomp_ctx, SCMP_FAIL, SCMP_SYS(mbind), 0);
    if (filter_set_status) {
        if (seccomp_ctx)
            seccomp_release(seccomp_ctx);
        fprintf(stderr, "seccomp could not add KILL rule for 'mbind': %m\n");
        return EXIT_FAILURE;
    }

    /*------ MIGRATE_PAGES RULE ------*/
    filter_set_status = seccomp_rule_add(seccomp_ctx, SCMP_FAIL, SCMP_SYS(migrate_pages), 0);
    if (filter_set_status) {
        if (seccomp_ctx)
            seccomp_release(seccomp_ctx);
        fprintf(stderr, "seccomp could not add KILL rule for 'migrate_pages': %m\n");
        return EXIT_FAILURE;
    }

    /*------ PTRACE RULE ------*/
    filter_set_status = seccomp_rule_add(seccomp_ctx, SCMP_FAIL, SCMP_SYS(ptrace), 0);
    if (filter_set_status) {
        if (seccomp_ctx)
            seccomp_release(seccomp_ctx);
        fprintf(stderr, "seccomp could not add KILL rule for 'ptrace': %m\n");
        return EXIT_FAILURE;
    }

    /**
    *  As another example lets say you want to disallow the 'unshare' system call
    *  But say that you want to disallow it ONLY if the 'CLONE_NEWUSER' flag is set in its argument
    *  Now we observe the signature for unshare() is as follows:
    *                  int unshare(int flags);
    *  Here flags can be an OR'ed combination of CLONE_FILES, CLONE_FS, CLONE_NEWCGROUP, CLONE_NEWUSER and many
    *  So we want to just capture calls to unshare() only if the 'CLONE_NEWUSER' flag is OR'ed in the argument.
    *  So seccomp_rule_add() would be as follows 
    **/
    /*------ UNSHARE RULE ------*/
    filter_set_status = seccomp_rule_add(
                                        seccomp_ctx,                    // the context to which the rule applies
                                        SCMP_FAIL,                  // action to take on rule match
                                        SCMP_SYS(unshare),              // get the sys_call number using SCMP_SYS() macro
                                        1,                              // any additional argument matches
                                        SCMP_A0(SCMP_CMP_MASKED_EQ, CLONE_NEWUSER, CLONE_NEWUSER)
                                        );
    if (filter_set_status) {
        if (seccomp_ctx)
            seccomp_release(seccomp_ctx);
        fprintf(stderr, "seccomp could not add KILL rule for 'unshare': %m\n");
        return EXIT_FAILURE;
    }

    /*------ CLONE RULE ------*/
    filter_set_status = seccomp_rule_add(
                                        seccomp_ctx,                    // the context to which the rule applies
                                        SCMP_FAIL,                  // action to take on rule match
                                        SCMP_SYS(clone),              // get the sys_call number using SCMP_SYS() macro
                                        1,                              // any additional argument matches
                                        SCMP_A2(SCMP_CMP_MASKED_EQ, CLONE_NEWUSER, CLONE_NEWUSER)
                                        );
    if (filter_set_status) {
        if (seccomp_ctx)
            seccomp_release(seccomp_ctx);
        fprintf(stderr, "seccomp could not add KILL rule for 'clone': %m\n");
        return EXIT_FAILURE;
    }
    
    /*------ CHMOD RULE ------*/
    filter_set_status = seccomp_rule_add(
                                        seccomp_ctx,                    // the context to which the rule applies
                                        SCMP_FAIL,                  // action to take on rule match
                                        SCMP_SYS(chmod),              // get the sys_call number using SCMP_SYS() macro
                                        1,                              // any additional argument matches
                                        SCMP_A3(SCMP_CMP_MASKED_EQ, S_ISUID, S_ISUID),
                                        SCMP_A3(SCMP_CMP_MASKED_EQ, S_ISGID, S_ISGID)
                                        );
    if (filter_set_status) {
        if (seccomp_ctx)
            seccomp_release(seccomp_ctx);
        fprintf(stderr, "seccomp could not add KILL rule for 'clone': %m\n");
        return EXIT_FAILURE;
    }

    /**
    *  Set the 'SCMP_FLTATR_CTL_NNP' attribute on the newly created context
    *  This attribute is used to ensure that the NO_NEW_PRIVS functionality is enabled 
    *  This is to control previledge escalation of child processes spawned with exec() in a parent with lesser priviledge
    *  You can just copy this and use it at the end of all seccomp rules.
    **/
    // TODO: COPY THIS TO THE END OF ALL SECCOMP RULES
    filter_set_status = seccomp_attr_set(seccomp_ctx, SCMP_FLTATR_CTL_NNP, 0);
    if (filter_set_status) {
        if (seccomp_ctx)
            seccomp_release(seccomp_ctx);
        fprintf(stderr, "seccomp could not set attribute 'SCMP_FLTATR_CTL_NNP': %m\n");
        return EXIT_FAILURE;
    }


    /**
    *  Finally load the created context into the kernel and release it from the current process memory
    **/ 
    filter_set_status = seccomp_load(seccomp_ctx);
    if (filter_set_status) {
        if (seccomp_ctx)
            seccomp_release(seccomp_ctx);               // release from current process memory.
        fprintf(stderr, "seccomp could not load the new context: %m\n");
        return EXIT_FAILURE;
    }

    return 0;
}

int setup_child_mounts(struct child_config *config)
{
    fprintf(stderr, "####### > child process remounting root with MS_PRIVATE...");
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL))
    {
        fprintf(stderr, "invocation to mount() failed! %m\n");
        return -1;
    }
    fprintf(stderr, "successfull remount.\n");

    fprintf(stderr, "####### > a bind mount is done on a newly created tmp directory...");
    char mount_dir[] = "/tmp/tmp.XXXXXX";
    if (!mkdtemp(mount_dir))
    {
        fprintf(stderr, "could not make a directory for mounting!\n");
        return -1;
    }

    if (mount(config->mount_dir, mount_dir, NULL, MS_BIND | MS_PRIVATE, NULL))
    {
        fprintf(stderr, "attempt to bind mount crashed!\n");
        return -1;
    }

    char inner_mount_dir[] = "/tmp/tmp.XXXXXX/oldroot.XXXXXX";
    memcpy(inner_mount_dir, mount_dir, sizeof(mount_dir) - 1);
    if (!mkdtemp(inner_mount_dir))
    {
        fprintf(stderr, "attempt to make inner directory failed!\n");
        return -1;
    }

    fprintf(stderr, "####### > changing root with pivot_root...");
    if (switch_child_root(mount_dir, inner_mount_dir))
    {
        fprintf(stderr, "failed!\n");
        return -1;
    }
    fprintf(stderr, "inner mount done.\n");

    char *old_root_dir = basename(inner_mount_dir);
    char old_root[sizeof(inner_mount_dir) + 1] = {"/"};
    strcpy(&old_root[1], old_root_dir);

    fprintf(stderr, "####### > unmounting old_root %s...", old_root);
    if (chdir("/"))
    {
        fprintf(stderr, "could not change directory with chdir()! %m\n");
        return -1;
    }
    if (umount2(old_root, MNT_DETACH))
    {
        fprintf(stderr, "attempt to umount() failed! %m\n");
        return -1;
    }
    if (rmdir(old_root))
    {
        fprintf(stderr, "invocation to rmdir() failed! %m\n");
        return -1;
    }
    
    if (mount(NULL, "/proc", "proc", 0, NULL)) {
		fprintf(stderr, "attempt to mount proc failed!\n");
		return -1;
	}
    fprintf(stderr, "successfully setup child mounts.\n");
    return 0;
}

int setup_cgroup_controls(struct child_config *config, struct cgroups_control **cgrps)
{
    fprintf(stderr, "####### > setting up SRContainer cgroups...");
    for (struct cgroups_control **cgrp = cgrps; *cgrp; cgrp++)
    {
        char dir[PATH_MAX] = {0};
        fprintf(stderr, "%s...", (*cgrp)->control);
        if (snprintf(dir, sizeof(dir), "/sys/fs/cgroup/%s/%s",
                     (*cgrp)->control, config->hostname) == -1)
        {
            return -1;
        }
        if (mkdir(dir, S_IRUSR | S_IWUSR | S_IXUSR))
        {
            fprintf(stderr, "directory creation with mkdir() %s failed: %m\n", dir);
            return -1;
        }
        for (struct cgroup_setting **setting = (*cgrp)->settings; *setting; setting++)
        {
            char path[PATH_MAX] = {0};
            int fd = 0;
            if (snprintf(path, sizeof(path), "%s/%s", dir,
                         (*setting)->name) == -1)
            {
                fprintf(stderr, "invocation to snprintf() failed: %m\n");
                return -1;
            }
            if ((fd = open(path, O_WRONLY)) == -1)
            {
                fprintf(stderr, "invocation to open() %s failed: %m\n", path);
                return -1;
            }
            if (write(fd, (*setting)->value, strlen((*setting)->value)) == -1)
            {
                fprintf(stderr, "write() attempt %s failed: %m\n", path);
                close(fd);
                return -1;
            }
            close(fd);
        }
    }

    fprintf(stderr, "####### > setting up rlimit...");
    if (setrlimit(RLIMIT_NOFILE,
                  &(struct rlimit){
                      .rlim_max = FD_COUNT,
                      .rlim_cur = FD_COUNT,
                  }))
    {
        fprintf(stderr, "setrlimit() failed: %m\n");
        return 1;
    }
    fprintf(stderr, "successfully set up cgroup controls.\n");
    return 0;
}

int free_cgroup_controls(struct child_config *config, struct cgroups_control **cgrps)
{
    fprintf(stderr, "####### > cleaning cgroups...");
    for (struct cgroups_control **cgrp = cgrps; *cgrp; cgrp++)
    {
        char dir[PATH_MAX] = {0};
        char task[PATH_MAX] = {0};
        int task_fd = 0;
        if (snprintf(dir, sizeof(dir), "/sys/fs/cgroup/%s/%s",
                     (*cgrp)->control, config->hostname) == -1 ||
            snprintf(task, sizeof(task), "/sys/fs/cgroup/%s/tasks",
                     (*cgrp)->control) == -1)
        {
            fprintf(stderr, "invocation to snprintf() failed: %m\n");
            return -1;
        }
        if ((task_fd = open(task, O_WRONLY)) == -1)
        {
            fprintf(stderr, "invocation to open() %s failed: %m\n", task);
            return -1;
        }
        if (write(task_fd, "0", 2) == -1)
        {
            fprintf(stderr, "write() attempt %s failed: %m\n", task);
            close(task_fd);
            return -1;
        }
        close(task_fd);
        if (rmdir(dir))
        {
            fprintf(stderr, "invocation to rmdir() %s failed: %m", dir);
            return -1;
        }
    }
    fprintf(stderr, "cgroup resources free'ed successfully.\n");
    return 0;
}

int setup_child_uid_map(pid_t child_pid, int fd)
{
    int uid_map = 0;
    int has_userns = -1;
    if (read(fd, &has_userns, sizeof(has_userns)) != sizeof(has_userns))
    {
        fprintf(stderr, "read() attempt from child failed!\n");
        return -1;
    }
    if (has_userns)
    {
        char path[PATH_MAX] = {0};
        for (char **file = (char *[]){"uid_map", "gid_map", 0}; *file; file++)
        {
            if (snprintf(path, sizeof(path), "/proc/%d/%s", child_pid, *file) > sizeof(path))
            {
                fprintf(stderr, "invocation to snprintf() failed. %m\n");
                return -1;
            }
            fprintf(stderr, "setting UID/GID mapping %s...", path);
            if ((uid_map = open(path, O_WRONLY)) == -1)
            {
                fprintf(stderr, "invocation to open() failed: %m\n");
                return -1;
            }
            if (dprintf(uid_map, "0 %d %d\n", USERNS_OFFSET, USERNS_COUNT) == -1)
            {
                fprintf(stderr, "invocation to dprintf() failed: %m\n");
                close(uid_map);
                return -1;
            }
            close(uid_map);
        }
    }
    if (write(fd, &(int){0}, sizeof(int)) != sizeof(int))
    {
        fprintf(stderr, "write() failed inside child: %m\n");
        return -1;
    }
    return 0;
}

int setup_child_userns(struct child_config *config)
{
    fprintf(stderr, "####### > attempting a new user namespace...");
    int has_userns = !unshare(CLONE_NEWUSER);
    if (write(config->fd, &has_userns, sizeof(has_userns)) != sizeof(has_userns))
    {
        fprintf(stderr, "write() inside child failed: %m\n");
        return -1;
    }
    int result = 0;
    if (read(config->fd, &result, sizeof(result)) != sizeof(result))
    {
        fprintf(stderr, "read() attempt inside child failed: %m\n");
        return -1;
    }
    if (result)
        return -1;
    if (has_userns)
    {
        fprintf(stderr, "all done successfully.\n");
    }
    else
    {
        fprintf(stderr, "some error occurred?? progressing anyways...\n");
    }
    fprintf(stderr, "####### > switching to UID %d / GID %d...", config->uid, config->uid);
    if (setgroups(1, &(gid_t){config->uid}) ||
        setresgid(config->uid, config->uid, config->uid) ||
        setresuid(config->uid, config->uid, config->uid))
    {
        fprintf(stderr, "%m\n");
        return -1;
    }
    fprintf(stderr, "setting up usernamespace done.\n");
    return 0;
}