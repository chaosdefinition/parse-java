# Makefile for parse-java - Chaos Shen

target: lex-java parse-java

lex-java: lex-java.c
	gcc -o lex-java lex-java.c

parse-java: parse-java.c
	gcc -o parse-java parse-java.c

clean:
	rm -rf *-java
