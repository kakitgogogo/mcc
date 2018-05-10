#/bin/bash

sudo make && sudo make install PREFIX=your_path_to_webbench

./webbench -t 5 http://www.baidu.com/

sudo make clean
