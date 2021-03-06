# Makefile for parse-java - Chaos Shen

target: lex-java parse-java

lex-java: lex-java.c
	cc -O2 -Wall -o lex-java lex-java.c

parse-java: parse-java.c
	cc -O2 -Wall -o parse-java parse-java.c

clean:
	rm -rf *-java
