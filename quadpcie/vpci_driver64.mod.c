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
	{ 0xb6a63e1, "module_layout" },
	{ 0x8c03d20c, "destroy_workqueue" },
	{ 0x42160169, "flush_workqueue" },
	{ 0xd73ace02, "device_destroy" },
	{ 0xbef6f27e, "class_destroy" },
	{ 0xf88cac93, "device_create" },
	{ 0x6bc3fbc0, "__unregister_chrdev" },
	{ 0xfbcd51c, "__class_create" },
	{ 0x527130a4, "__register_chrdev" },
	{ 0x49cd25ed, "alloc_workqueue" },
	{ 0x656e4a6e, "snprintf" },
	{ 0xd6ee688f, "vmalloc" },
	{ 0xcbd4898c, "fortify_panic" },
	{ 0xc5b6f236, "queue_work_on" },
	{ 0xa916b694, "strnlen" },
	{ 0x6b10bee1, "_copy_to_user" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0x13c49cc2, "_copy_from_user" },
	{ 0x7656064e, "filp_close" },
	{ 0x14adcd5c, "kernel_write" },
	{ 0x1526d277, "filp_open" },
	{ 0x96848186, "scnprintf" },
	{ 0xd0da656b, "__stack_chk_fail" },
	{ 0x65487097, "__x86_indirect_thunk_rax" },
	{ 0x97893891, "tpci_fops" },
	{ 0x365acda7, "set_normalized_timespec64" },
	{ 0x9ec6ca96, "ktime_get_real_ts64" },
	{ 0xeb233a45, "__kmalloc" },
	{ 0x3754aaaa, "kmem_cache_alloc_trace" },
	{ 0xbc870820, "kmalloc_caches" },
	{ 0x37a0cba, "kfree" },
	{ 0x999e8297, "vfree" },
	{ 0x92997ed8, "_printk" },
	{ 0x3213f038, "mutex_unlock" },
	{ 0x4dfa8d4b, "mutex_lock" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0xbdfb6dbb, "__fentry__" },
};

MODULE_INFO(depends, "pci_driver64");


MODULE_INFO(srcversion, "7EF594CC91D1EBCC9F4BEF8");
