obj-m += klvoltage.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

load:
	sudo insmod ./klvoltage.ko

unload:
	sudo rmmod klvoltage

