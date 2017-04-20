##################################################
# 声明CMake的最低要求版本 
cmake_minimum_required(VERSION 2.8)

##################################################
# 指定编译器
SET(CMAKE_STSTEM_NAME Linux)
SET(CMAKE_C_COMPILER gcc)
SET(CMAKE_CXX_COMPILER g++) 

###################################################


##################################################
# 定义项目(工程)名称,  
# 同时定义了以下默认变量:  
# PROJECT_SOURCE_DIR, 代指CMake开始构建的根目录(通常是项目根目录)  
# PROJECT_BINARY_DIR, 代指CMake的编译目录(即执行cmake命令的目录) 
project (ffmpegsample)

# 添加头文件的查找目录  
INCLUDE_DIRECTORIES(
$ENV{install_path_env}/include/ )
# 添加库文件查找目录
LINK_DIRECTORIES(
$ENV{install_path_env}/lib/ )


##################################################
# 定义库文件
SET(LIB avdevice avfilter avformat avcodec swresample swscale avutil dl z bz2 m pthread)

##################################################
# 编译可执行程序 
ADD_EXECUTABLE(ffmpegdemo ffmpegdemo.c)
ADD_EXECUTABLE(video_combine video_combine.c)

TARGET_LINK_LIBRARIES(ffmpegdemo ${LIB})
TARGET_LINK_LIBRARIES(video_combine ${LIB})
