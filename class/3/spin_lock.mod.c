#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x2c635209, "module_layout" },
	{ 0x7a08b1ac, "param_ops_uint" },
	{ 0xae2ae519, "param_ops_int" },
	{ 0x93d6dd8c, "complete_all" },
	{ 0xf9a482f9, "msleep" },
	{ 0x9ce84760, "wake_up_process" },
	{ 0x3363cbcd, "kthread_bind" },
	{ 0xaa44a707, "cpumask_next" },
	{ 0x8810754a, "_find_first_bit" },
	{ 0x5a5a2271, "__cpu_online_mask" },
	{ 0x17de3d5, "nr_cpu_ids" },
	{ 0x851c15cf, "kthread_create_on_node" },
	{ 0xc60d0620, "__num_online_cpus" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0xa6257a2f, "complete" },
	{ 0xeae3dfd6, "__const_udelay" },
	{ 0x71038ac7, "pv_ops" },
	{ 0xba8fbd64, "_raw_spin_lock" },
	{ 0x25974000, "wait_for_completion" },
	{ 0x92997ed8, "_printk" },
	{ 0x7a2af7b4, "cpu_number" },
	{ 0xbdfb6dbb, "__fentry__" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "18E7F12E4DF71344C958820");
