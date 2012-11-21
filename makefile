default: program

program:
	clang++ -lphash -lboost_system-mt -lboost_filesystem-mt *.cpp -o imagegrouper

nomt:
	clang++ -lphash -lboost_system -lboost_filesystem *.cpp -o imagegrouper

clean:
	-rm -f imagegrouper
