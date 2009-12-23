# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.

ifneq ($(KERNELRELEASE),)
obj-m := stub.o

EXTRA_CFLAGS := -I$(src)/include  -DREGISTER_STUB_HANDLER


# Otherwise we were called directly from the command
# line; invoke the kernel build system.
else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif

clean:
	@rm -rf *.o *.ko Module.symvers .*.cmd *.mod.c 

