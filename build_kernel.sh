#!/bin/bash

# setup
source build_stuff.sh
DATE=$(date +%m%d)

# execution!
cd ..

# check for voodoo/non-voodoo - backup/restore .git if needed
if [ "$1" != "V" ]; then
	CONFIG="novoodoo"
	RESTORE_GIT="y"
	for MODEL in $MODELS
	do
		REPO="$MODEL"_initramfs && fetch_repo
		cd "$REPO"
		zip -r -q "$REPO"_git.zip .git
		mv "$REPO"_git.zip "$WORK"
		rm -rf .git >/dev/null 2>&1
		cd ..
	done
else
	CONFIG="voodoo5"
fi

# fetch the toolchain if needed
if [ ! -d arm-2009q3 ]; then
	tarball="arm-2009q3-67-arm-none-linux-gnueabi-i686-pc-linux-gnu.tar.bz2"
	if [ ! -f "$tarball" ]; then
		echo "***** Downloading toolchain *****"
		CMD="wget http://www.codesourcery.com/public/gnu_toolchain/arm-none-linux-gnueabi/\"$tarball\"" && doit
	fi
	echo "***** Unpacking toolchain *****"
	CMD="tar -xjf \"$tarball\"" && doit
	# don't remove tarball; bandwidth conservation :)
fi

# build the kernel
cd linux-2.6-fascinate
for MODEL in $MODELS
do
	TARGET="$MODEL"_"$CONFIG"
	echo "***** Building : $TARGET *****"
	make clean mrproper >/dev/null 2>&1
	rm -f update/*.zip update/kernel_update/zImage "$DATE"_"$TARGET".zip

	sed -i "s/\/.*_/\/"$MODEL"_/" arch/arm/configs/jt1134_"$CONFIG"_defconfig
	CMD="make ARCH=arm jt1134_\"$CONFIG\"_defconfig" && doit
	CMD="make -j8 CROSS_COMPILE=../arm-2009q3/bin/arm-none-linux-gnueabi- \
		ARCH=arm HOSTCFLAGS=\"-g -O3\"" && doit

	cp arch/arm/boot/zImage update/kernel_update/zImage
	cd update
	zip -r -q kernel_update.zip .
	mv kernel_update.zip ../"$DATE"_"$TARGET".zip
	cd ..
	echo -e "***** Successfully compiled: $TARGET *****\n"

	if [ "$RESTORE_GIT" = "y" ]; then
		cd ../"$MODEL"_initramfs
		unzip -q "$WORK"/"$MODEL"_initramfs_git.zip
		cd "$WORK"
		rm -f "$MODEL"_initramfs_git.zip
	fi
done

if [ "$1" == "A" ]; then ./build_voodoo.sh; fi

