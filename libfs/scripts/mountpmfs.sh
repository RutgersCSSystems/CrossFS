sudo mount -t pmfs -o physaddr=0x800000000,init=16G,nohugeioremap none /mnt/ramfs
sudo chown -R $USER /mnt/ramfs

