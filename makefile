nmon: nmon.cpp
	gcc -l curses -o nmon nmon.cpp

clean:
	rm nmon
