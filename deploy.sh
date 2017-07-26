make clean;
make;
sudo insmod DragonKing.ko
sudo mknod /dev/dkCommand c 666 0
sudo chmod a+w /dev/dkCommand 
