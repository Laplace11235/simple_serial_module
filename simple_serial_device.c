#include <linux/module.h>
#include <linux/kernel.h>

static int myinit(void)
{
	pr_info("Init serial device vscode\n");
	return 0;
}

static void myexit(void)
{
	pr_info("exit serial device\n");
}

module_init(myinit)
module_exit(myexit)
MODULE_LICENSE("GPL");
