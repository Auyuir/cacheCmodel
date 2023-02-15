SRCS = *.cpp
BIN = test

debug:
	g++ -fdiagnostics-color=always -g ${SRCS} -o ${BIN}
#--coverage option for converage stat, recommand for gcc -v > 9. remove if dont want the stat

coverage: clean
	g++ --coverage -fdiagnostics-color=always ${SRCS} -o ${BIN}

clean:
	rm ${BIN} *.gcda *.gcno