cmake_minimum_required(VERSION 3.8)
project(lab4)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        traces/amptjp-bal.rep
        traces/binary-bal.rep
        traces/binary2-bal.rep
        traces/cccp-bal.rep
        traces/coalescing-bal.rep
        traces/cp-decl-bal.rep
        traces/expr-bal.rep
        traces/random-bal.rep
        traces/random2-bal.rep
        traces/realloc-bal.rep
        traces/realloc2-bal.rep
        traces/short1.rep
        traces/short2.rep
        clock.c
        clock.h
        clock.o
        config.h
        fcyc.c
        fcyc.h
        fcyc.o
        fsecs.c
        fsecs.h
        fsecs.o
        ftimer.c
        ftimer.h
        ftimer.o
        Makefile
        mdriver
        mdriver.c
        mdriver.o
        memlib.c
        memlib.h
        memlib.o
        mm.h
        mm.o)

add_executable(lab4 ${SOURCE_FILES})