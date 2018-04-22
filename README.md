## MCC
My C Compiler

### Intro
一个基于C11标准实现的C语言编译器。

### Install
执行`make mcc`命令编译获得编译器。编译器默认的路径位于`/usr/local/bin`。

### Usage
在命令行界面使用mcc命令对C程序进行编译。命令选项如下：
~~~c
Usage: mcc [options] file...
Options:
-h                       Display this information
-E                       Preprocess only; do not compile, assemble or link
-S                       Compile only; do not assemble or link
-c                       Compile and assemble, but do not link
-o <file>                Place the output into <file>
-I <path>                add include path
-D <name>[=def]          Predefine name as a macro
-U <name>                Undefine name
-l <library>             link library
~~~

### Example
单元测试在unittest目录下。C程序实例在test/cprogram目录下。

### TODO
1.发现并除掉bug
2.机器无关代码优化
2.重构