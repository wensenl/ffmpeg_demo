#!/bin/sh

#编译安装路径
install_path=${PWD}/build/install
export install_path_env=${install_path}


#编译ffmpeg,支持aac和h264
mkdir build

#build x264
if [ -d build/x264 ]
then
	echo "x264 already build"
else
	tar xzvf ./sdk/x264.tar.gz -C ./build/
	cd ./build/x264
	./configure --prefix=${install_path} --enable-static
	make -j4
	make install

	cd ../..
fi

#build faac-1.28
if [ -d build/faac-1.28 ]
then
	echo "faac-1.28 already build"
else
	tar xzvf ./sdk/faac-1.28.tar.gz -C ./build/
	tar xzvf ./sdk/faac-1.28-bugfix.tar.gz -C ./build/
	cd ./build/faac-1.28
	./configure --prefix=${install_path} --enable-static --disable-shared
	make -j4
	make install
	cd ../..
fi

if [ -d build/ffmpeg-2.8.10 ]
then
	echo "ffmpeg-2.8.10 already build"
else
	tar xzvf ./sdk/ffmpeg-2.8.10.tar.gz -C ./build/
	cd ./build/ffmpeg-2.8.10
	./configure \
		--prefix=${install_path} \
		--extra-cflags='-I./../install/include' \
		--extra-ldflags='-L./../install/lib -gl' \
		--extra-libs='-ldl ' \
		--enable-version3 \
		--enable-pthreads \
		--enable-static \
		--disable-shared \
		--enable-gpl \
		--enable-nonfree \
		--enable-libx264 \
		--enable-libfaac \
		--disable-devices
	make -j4
	make install

	cd ../..
fi

#编译demo
rm -rf build/ffmpeg_demo 
mkdir build/ffmpeg_demo
cd build/ffmpeg_demo
cmake ./../..
make
cp -arf ./video_combine ./../..
cp -arf ./ffmpegdemo ./../..



