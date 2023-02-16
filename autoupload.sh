#!/bin/sh
udisksctl monitor | grep '/dev/disk/by-label/piboot' --line-buffered | xargs -L1 sh -c "echo 'starting!'; udisksctl mount -b /dev/disk/by-label/piboot; SD_DIR=/run/media/matt/piboot make install; eject /dev/disk/by-label/piboot; echo 'done!'"
