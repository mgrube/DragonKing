#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/syscalls.h>
#include <linux/kallsyms.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/unistd.h>
#include <asm/uaccess.h>
#include <linux/namei.h>
#include <linux/fs.h>
#include <linux/utsname.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/slab.h>
#include <linux/proc_ns.h>


asmlinkage long (*orig_execve)(const char __user *filename, char const __user *argv[], char const __user *envp[]);

asmlinkage long hacked_execve(const char __user *filename, char const __user *argv[], char const __user *envp[]);

// List of processes we won't allow to run
const char * const BANNED_PROCESSES[] = {"ping", "clamav", "tcpdump"};


