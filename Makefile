SRCS = *.cpp
BIN = test
#需要编译源文件中也有test.cpp才能保证clean正确运行

debug: clean
	g++ -fdiagnostics-color=always -g ${SRCS} -o ${BIN}
#--coverage option for converage stat, recommand for gcc -v > 9. remove if dont want the stat

coverage: clean
	g++ --coverage -fdiagnostics-color=always ${SRCS} -o ${BIN}

clean: clean_BIN clean_gcda clean_gcno

clean_BIN:
	if [ -a ${BIN} ];then\
		rm ${BIN};\
	fi

clean_gcda:
	if [ -a ${BIN}.gcda ];then\
		rm *.gcda;\
	fi

clean_gcno:
	if [ -a ${BIN}.gcno ];then\
		rm *.gcno;\
	fi