## FFmpeg编译

#### 编译环境
- macOS (10.11.6)
- FFmpeg源码(4.2.1)
- ndk(android-ndk-r13)

#### .configure文件修改
> 目的是编译的产出so库能让android使用，否则会生成库的格式形如 **xxx.版本.100** 的文件，无法使用。
```
SLIBNAME_WITH_MAJOR='$(SLIBNAME).$(LIBMAJOR)'
LIB_INSTALL_EXTRA_CMD='$$(RANLIB)"$(LIBDIR)/$(LIBNAME)"'
SLIB_INSTALL_NAME='$(SLIBNAME_WITH_VERSION)'
SLIB_INSTALL_LINKS='$(SLIBNAME_WITH_MAJOR)$(SLIBNAME)'
```
替换成
```
SLIBNAME_WITH_MAJOR='$(SLIBPREF)$(FULLNAME)-$(LIBMAJOR)$(SLIBSUF)'
LIB_INSTALL_EXTRA_CMD='$$(RANLIB)"$(LIBDIR)/$(LIBNAME)"'
SLIB_INSTALL_NAME='$(SLIBNAME_WITH_MAJOR)'
SLIB_INSTALL_LINKS='$(SLIBNAME)'
```

#### shell脚本编写
- build.sh : 产出多个so库，如libavformat.so、libavfilter.so等多个so库，以及include头文件；
- build-static.sh : 编译期间生成.a静态库，然后将多个静态库合并成一个.so动态库，提供android使用，当然.a文件也可以供iOS使用。

> 注意事项：可能在windows上编写的sh文件，放在OS x上无法使用，回报最后一行不可用，导致shell脚本无法执行，可以在vim中使用｀set ff｀参看
如果是doc，通过｀set ff=unix(或set fileformat=unix)｀改成unix；
> 注意sh脚本不要写错了，否则也会出现上面无法执行的清空;
> 脚本方法build_one中./configure后面拼接ffmpeg的定制开关，可通过｀./configure --help｀查看具体配置信息。

+ TMPDIR为编译生成的临时文件存放的目录，必须要有。
+ SYSROOT为so文件支持的最低Android版本的平台目录
+ CPU为指定so文件支持的平台
+ PREFIX为生成的so文件存放目录
+ TOOLCHAIN为编译所使用的工具链目录
+ cross-prefix为编译所使用的工具链文件
+ enable和disable指定了需要编译的项
+ target-os为目标操作系统；

#### 编译常见错误
1. 4.x版本ffmpeg会出现B0、xB0变量的错误。具体原因不详；
    - ibavcodec/aaccoder.c中的所有变量B0都改成b0
    - 提示y0000000' undeclared 把libavcodec/hevc_mvs.c文件的所有变量B0改成b0，xB0改成xb0，yB0改成yb0;
    - libavcodec/opus_pvq.c中所有变量B0都改成b0;
2. 合并库时会偶尔抽风，提示一些方法或变量找不到或未定义，多试了一下又过了。


> todo:编译的libavcodec.so还有问题，依赖了一些其他的库