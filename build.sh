BRPATH=/malina/korzeniewskim/buildroot-2018.02
(
    export PATH=$BRPATH/output/host/usr/bin:$PATH
    make ARCH=arm CROSS_COMPILE=arm-none-linux-gnueabi- timer
    rm -f ~/.ssh/known_hosts 
    scp timer root@192.168.143.117:/timer
)