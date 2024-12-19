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



static const char ____versions[]
__used __section("__versions") =
	"\x24\x00\x00\x00\x33\xb3\x91\x60"
	"unregister_chrdev_region\0\0\0\0"
	"\x10\x00\x00\x00\x53\x39\xc0\xed"
	"iounmap\0"
	"\x1c\x00\x00\x00\x84\x90\x16\xc5"
	"pci_release_regions\0"
	"\x1c\x00\x00\x00\xdd\x83\x19\xd6"
	"pci_disable_device\0\0"
	"\x14\x00\x00\x00\x4b\x8d\xfa\x4d"
	"mutex_lock\0\0"
	"\x18\x00\x00\x00\x38\xf0\x13\x32"
	"mutex_unlock\0\0\0\0"
	"\x10\x00\x00\x00\xba\x0c\x7a\x03"
	"kfree\0\0\0"
	"\x14\x00\x00\x00\x46\x90\x16\x6d"
	"_dev_info\0\0\0"
	"\x18\x00\x00\x00\xf5\x76\x81\x28"
	"class_destroy\0\0\0"
	"\x18\x00\x00\x00\xc2\x9c\xc4\x13"
	"_copy_from_user\0"
	"\x1c\x00\x00\x00\xcb\xf6\xfd\xf0"
	"__stack_chk_fail\0\0\0\0"
	"\x14\x00\x00\x00\xf3\xf5\x8a\xa7"
	"ioread32\0\0\0\0"
	"\x18\x00\x00\x00\xe1\xbe\x10\x6b"
	"_copy_to_user\0\0\0"
	"\x18\x00\x00\x00\x5e\xd0\xc5\xfe"
	"devm_kmalloc\0\0\0\0"
	"\x1c\x00\x00\x00\xaf\x7d\x62\x9d"
	"pci_enable_device\0\0\0"
	"\x1c\x00\x00\x00\x52\xb9\xae\x25"
	"pci_request_regions\0"
	"\x10\x00\x00\x00\x09\xcd\x80\xde"
	"ioremap\0"
	"\x1c\x00\x00\x00\x2b\x2f\xec\xe3"
	"alloc_chrdev_region\0"
	"\x14\x00\x00\x00\x60\x73\x77\x14"
	"cdev_init\0\0\0"
	"\x14\x00\x00\x00\xb1\x19\x54\xaa"
	"cdev_add\0\0\0\0"
	"\x18\x00\x00\x00\xc3\x9e\x64\x92"
	"device_create\0\0\0"
	"\x18\x00\x00\x00\x9f\x0c\xfb\xce"
	"__mutex_init\0\0\0\0"
	"\x14\x00\x00\x00\x10\xf5\xd0\x5b"
	"_dev_err\0\0\0\0"
	"\x18\x00\x00\x00\xe2\x75\x96\x16"
	"class_create\0\0\0\0"
	"\x20\x00\x00\x00\x69\x99\xe8\xc5"
	"pci_unregister_driver\0\0\0"
	"\x14\x00\x00\x00\xbb\x6d\xfb\xbd"
	"__fentry__\0\0"
	"\x1c\x00\x00\x00\xca\x39\x82\x5b"
	"__x86_return_thunk\0\0"
	"\x20\x00\x00\x00\xd9\x2b\x8f\xdd"
	"__pci_register_driver\0\0\0"
	"\x10\x00\x00\x00\x7e\x3a\x2c\x12"
	"_printk\0"
	"\x18\x00\x00\x00\x10\x5e\x65\x38"
	"device_destroy\0\0"
	"\x14\x00\x00\x00\x05\xb4\xd2\x39"
	"cdev_del\0\0\0\0"
	"\x18\x00\x00\x00\xd7\xd3\x75\x6d"
	"module_layout\0\0\0"
	"\x00\x00\x00\x00\x00\x00\x00\x00";

MODULE_INFO(depends, "");

MODULE_ALIAS("pci:v0000800Ed00007011sv*sd*bc*sc*i*");

MODULE_INFO(srcversion, "5638DD3AC6CF41049A509B5");
