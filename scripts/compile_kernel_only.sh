set -x

PARA=32

cd $KERN_SRC
sudo cp mlfs.config .config
sudo make -j$PARA
#sudo make modules

 y="4.8.12"
   if [[ x$ == x ]];
  then
      echo You have to say a version!
      exit 1
   fi

sudo cp $KERN_SRC/arch/x86/boot/bzImage $KERNEL/vmlinuz-$y
sudo cp $KERN_SRC/System.map $KERNEL/System.map-$y
sudo cp $KERN_SRC/.config $KERNEL/config-$y
set +x
