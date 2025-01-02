#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

#ifdef CONFIG_UNWINDER_ORC
#include <asm/orc_header.h>
ORC_HEADER;
#endif

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
	{ 0xedc03953, "iounmap" },
	{ 0xba1aa243, "pci_release_regions" },
	{ 0xa915b11a, "pci_disable_device" },
	{ 0x4dfa8d4b, "mutex_lock" },
	{ 0x3213f038, "mutex_unlock" },
	{ 0x37a0cba, "kfree" },
	{ 0x646397e6, "_dev_info" },
	{ 0x9901e5e3, "class_destroy" },
	{ 0x13c49cc2, "_copy_from_user" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0xa78af5f3, "ioread32" },
	{ 0x6b10bee1, "_copy_to_user" },
	{ 0xb94827b9, "devm_kmalloc" },
	{ 0xe26e4f68, "pci_enable_device" },
	{ 0xbc0f1ae9, "pci_request_regions" },
	{ 0xde80cd09, "ioremap" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0x5c658724, "cdev_init" },
	{ 0xa2dc7695, "cdev_add" },
	{ 0x23a131f9, "device_create" },
	{ 0xcefb0c9f, "__mutex_init" },
	{ 0x1e898af9, "_dev_err" },
	{ 0x38e073, "class_create" },
	{ 0x459fa94a, "pci_unregister_driver" },
	{ 0x54b1fac6, "__ubsan_handle_load_invalid_value" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x3bb4acde, "__pci_register_driver" },
	{ 0x2f8479f2, "device_destroy" },
	{ 0xd58dd2b1, "cdev_del" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0xf079b8f9, "module_layout" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("pci:v0000800Ed00007011sv*sd*bc*sc*i*");

MODULE_INFO(srcversion, "4B4C12676FDC7E74CA9121A");
