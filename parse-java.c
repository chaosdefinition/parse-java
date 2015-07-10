/*
 * parse-java.c - parser of Java source code
 *
 * Copyright (C) 2015 Chaos Shen
 *
 * This file is part of parse-java.
 *
 * parse-java is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * parse-java is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with parse-java.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef _MSC_VER
/*
 * if using MSVC, suppress stupid security warnings, define snprintf and inline
 */
# define _CRT_SECURE_NO_WARNINGS
# define snprintf _snprintf
# define inline
#endif /* _MSC_VER */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* checker and translator definitions, see line 577 for more details */
#define DEFINE_CHECK(N) static int check_##N(FILE * src)
#define DEFINE_TRANSLATE(N) static void translate_##N(FILE * src, FILE * out)
#define DEFINE_TRANSLATE_RETURN(N) static int translate_##N(FILE * src,\
	FILE * out)
#define CALL_CHECK(N, src) check_##N(src)
#define CALL_TRANSLATE(N, src, out) translate_##N(src, out)

/*
 * set it smaller than the one in lex-java to prevent potential stack overflow
 */
#define BUF_SIZE 512
#define STACK_SIZE BUF_SIZE

/* get the size of an array, maybe defined */
#ifndef ARRAY_SIZE
# define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))
#endif /* ARRAY_SIZE */

/* attribute list */
enum
{
	WRONG          = 0x101,
	SPACE          = 0x102,
	KEYWORD        = 0x103,
	IDENTIFIER     = 0x104,
	BOOLEAN        = 0x105,
	CHAR           = 0x106,
	INT            = 0x107,
	FLOAT          = 0x108,
	STRING         = 0x109,
	ASSIGN         = 0x110,
	CONDITION      = 0x111,
	LOGIC_OR       = 0x112,
	LOGIC_AND      = 0x113,
	BIT_OR         = 0x114,
	XOR            = 0x115,
	BIT_AND        = 0x116,
	EQUAL          = 0x117,
	COMPARE        = 0x118,
	SHIFT          = 0x119,
	ADD_SUB        = 0x11a,
	MUL_DIV        = 0x11b,
	PLUSPLUS       = 0x11c,
	BRACKET_DOT    = 0x11d,
	/* here is a chasm */
	COMMA          = 0x120,
	BIG_BRACKET    = 0x121,
	SEMICOLON      = 0x122,
	COLON          = 0x123, /* in addition */
	REGISTER       = 0x124, /* in addition */
};

/* word type */
struct word_t
{
	int key;
	char value[BUF_SIZE];
};

/* word stack type */
struct stack_t
{
	int top;
	struct word_t words[STACK_SIZE];
};

/* global variable to store returned word */
static struct word_t returned = {
	.key = 0,
	.value = { 0 },
};

/* global stacks of operators and operands */
static struct stack_t operators = {
	.top = 0,
	.words = { 0 },
};
static struct stack_t operands = {
	.top = 0,
	.words = { 0 },
};

/* global register usage indicator */
static int registers[] = {
	[0] = 0, /* eax, specially used as accumulator */
	[1] = 0, /* ebx */
	[2] = 0, /* ecx */
	[3] = 0, /* edx */
};

/* word operations */
static void get_word(FILE * src, struct word_t * ret);
static inline int check_word(const struct word_t * word, int type,
	const char * value);
static inline void return_word(const struct word_t * word);

/* stack operations */
static inline int push_operator(const struct word_t * word);
static inline int push_operand(const struct word_t * word);
static inline int pop_operator(struct word_t * word);
static inline int pop_operand(struct word_t * word);

/* register operations */
static inline int alloc_register(void);
static inline int alloc_accumulator(void);
static inline void free_register(int no);
static inline int get_register_no(const char * name);
static inline const char * get_register_name(int no);

static int do_validate_lex(FILE * src);
static int do_validate_grammar(FILE * src);
static void do_parse(FILE * src, FILE * out);

DEFINE_CHECK(S);
DEFINE_TRANSLATE_RETURN(S);
DEFINE_CHECK(E);
DEFINE_TRANSLATE(E);
DEFINE_CHECK(A);
DEFINE_TRANSLATE(A);
DEFINE_CHECK(V);
DEFINE_TRANSLATE(V);
DEFINE_CHECK(O);
DEFINE_CHECK(C);
DEFINE_TRANSLATE(C);
DEFINE_CHECK(C1);
DEFINE_TRANSLATE(C1);
DEFINE_CHECK(T);
DEFINE_TRANSLATE(T);
DEFINE_CHECK(T1);
DEFINE_TRANSLATE(T1);
DEFINE_CHECK(P);
DEFINE_CHECK(M);
DEFINE_TRANSLATE(O_P_M);

int main(int argc, char * const * argv)
{
	FILE * fp1 = NULL, * fp2 = NULL;
	const char * src = "scanner_output", * out = "parser_output";
	const char * usage = "Usage: parse-java [SOURCE]\n"
			     "If SOURCE is not specified, 'scanner_output' "
			     "will be used\n\n";
	char err_msg[BUF_SIZE];

	/* restrict exactly 1 or 2 arguments */
	if (argc > 2) {
		fprintf(stderr, "%s", usage);
		goto error;
	} else if (argc == 2) {
		src = argv[1];
	}

	/* open source file */
	if ((fp1 = fopen(src, "r")) == NULL) {
		snprintf(err_msg, BUF_SIZE, "parse-java: cannot open '%s'",
			src);
		perror(err_msg);
		goto error;
	}

	/* do lexical validation */
	if (!do_validate_lex(fp1)) {
		fprintf(stderr, "parse-java: invalid lexical analysis output "
			"file\n");
		goto error;
	}

	/* do grammar validation */
	if (!do_validate_grammar(fp1)) {
		fprintf(stderr, "parse-java: grammar error\n");
		goto error;
	}

	/* open output file */
	if ((fp2 = fopen(out, "w")) == NULL) {
		snprintf(err_msg, BUF_SIZE, "parse-java: cannot open '%s'",
			out);
		perror(err_msg);
		goto error;
	}

	/* do parse */
	do_parse(fp1, fp2);

	fclose(fp1);
	fclose(fp2);
	return 0;

error:
	if (fp1 != NULL) {
		fclose(fp1);
	}
	if (fp2 != NULL) {
		fclose(fp2);
	}
	return 1;
}

/********************* word operations ****************************************/

/*
 * get the next K-V pair
 *
 * @src: a FILE pointer of Java lexical analysis output file
 * @ret: a pointer to struct word_t to store return value
 */
static void get_word(FILE * src, struct word_t * ret)
{
	char * word = NULL;
	int attr;
	char buffer[BUF_SIZE];

	/* first check if there is a previous returned word */
	if (returned.key != 0) {
		ret->key = returned.key;
		strncpy(ret->value, returned.value, BUF_SIZE);

		/* do clear */
		returned.key = 0;
		returned.value[0] = '\0';
		return;
	}

	ret->key = 0;
	ret->value[0] = '\0';
	while (1) {
		if (fgets(buffer, BUF_SIZE, src) == NULL) {
			return;
		}

		/* ignore word counter lines and spaces */
		word = strtok(buffer, " \t\r\n");
		if (strcmp(word, "line") == 0 || strcmp(word, "total") == 0) {
			continue;
		}
		attr = strtol(word, NULL, 16);
		if (attr == SPACE) {
			continue;
		}

		ret->key = attr;
		strncpy(ret->value, strtok(NULL, " \t\r\n"), BUF_SIZE);
		return;
	}
}

/*
 * check a word of the given type and value
 *
 * @word: a pointer to struct word_t
 * @type: attribute key, see line 54
 * @value: value of the word, can be NULL
 *
 * return: 1 if valid, 0 otherwise
 */
static inline int check_word(const struct word_t * word, int type,
	const char * value)
{
	if (word->key != type) {
		goto invalid;
	}
	if (value != NULL && strcmp(word->value, value) != 0) {
		goto invalid;
	}
	return 1;

invalid:
	return 0;
}

/*
 * pretend to return a word to the file stream (only one word supported)
 *
 * @word: a pointer to struct word_t
 */
static inline void return_word(const struct word_t * word)
{
	if (returned.key == 0) {
		returned.key = word->key;
		strncpy(returned.value, word->value, BUF_SIZE);
	}
}

/********************* word stack operations **********************************/

/*
 * push to operator stack
 *
 * @word: a pointer to struct word_t to push
 *
 * return: 0 on success, -1 otherwise
 */
static inline int push_operator(const struct word_t * word)
{
	if (operators.top == STACK_SIZE) {
		return -1;
	}

	operators.words[operators.top].key = word->key;
	strncpy(operators.words[operators.top].value, word->value, BUF_SIZE);
	++operators.top;
	return 0;
}

/*
 * push to operand stack
 *
 * @word: a pointer to struct word_t to push
 *
 * return: 0 on success, -1 otherwise
 */
static inline int push_operand(const struct word_t * word)
{
	if (operands.top == STACK_SIZE) {
		return -1;
	}

	operands.words[operands.top].key = word->key;
	strncpy(operands.words[operands.top].value, word->value, BUF_SIZE);
	++operands.top;
	return 0;
}

/*
 * pop from operator stack
 *
 * @word: a pointer to struct word_t to store popped word, can be NULL
 *
 * return: 0 on success, -1 otherwise
 */
static inline int pop_operator(struct word_t * word)
{
	if (operators.top == 0) {
		return -1;
	}

	--operators.top;
	if (word != NULL) {
		word->key = operators.words[operators.top].key;
		strncpy(word->value, operators.words[operators.top].value,
			BUF_SIZE);
	}
	return 0;
}

/*
 * pop from operand stack
 *
 * @word: a pointer to struct word_t to store popped word, can be NULL
 *
 * return: 0 on success, -1 otherwise
 */
static inline int pop_operand(struct word_t * word)
{
	if (operands.top == 0) {
		return -1;
	}

	--operands.top;
	if (word != NULL) {
		word->key = operands.words[operands.top].key;
		strncpy(word->value, operands.words[operands.top].value,
			BUF_SIZE);
	}
	return 0;
}

/********************* register operations ************************************/

/*
 * allocate a register
 *
 * return: register number (except 0) on success, -1 otherwise
 */
static inline int alloc_register(void)
{
	int i;

	for (i = 1; i < ARRAY_SIZE(registers); ++i) {
		if (!registers[i]) {
			registers[i] = 1;
			return i;
		}
	}
	return -1;
}

/*
 * allocate the accumulator
 *
 * return: 0 on success, -1 otherwise
 */
static inline int alloc_accumulator(void)
{
	if (!registers[0]) {
		registers[0] = 1;
		return 0;
	}
	return -1;
}

/*
 * free a register
 *
 * @no: register number
 */
static inline void free_register(int no)
{
	if (no >= 0 && no < ARRAY_SIZE(registers)) {
		registers[no] = 0;
	}
}

/*
 * get register number according to the given name
 */
static inline int get_register_no(const char * name)
{
	if (strcmp(name, "eax") == 0) {
		return 0;
	}
	if (strcmp(name, "ebx") == 0) {
		return 1;
	}
	if (strcmp(name, "ecx") == 0) {
		return 2;
	}
	if (strcmp(name, "edx") == 0) {
		return 3;
	}
	return -1;
}

/*
 * get register name according to the given number
 */
static inline const char * get_register_name(int no)
{
	switch (no) {
	case 0:
		return "eax";
	case 1:
		return "ebx";
	case 2:
		return "ecx";
	case 3:
		return "edx";
	default:
		return NULL;
	}
}

/********************* main stuffs ********************************************/

/*
 * simply validate the lexical analysis output file
 * format of each line: <0xDDD	value> is a normal K-V pair line
 *                      <0x101	value at line X> is a wrong K-V pair line
 *                      <line X has W word[s]> is a word counter line
 *                      <total W words> is a global word counter line
 *
 * @src: a FILE pointer of Java lexical analysis output file
 *
 * return: 1 if valid, 0 otherwise
 */
static int do_validate_lex(FILE * src)
{
	struct word_t word;

	rewind(src);
	while (1) {
		/* extract the word of each line */
		get_word(src, &word);
		if (word.key == 0) {
			return 1;
		}

		/*
		 * the attribute key must be within the range and not be WRONG
		 */
		if (word.key <= WRONG || word.key > COLON ||
			(word.key > BRACKET_DOT && word.key < COMMA)) {
			return 0;
		}
	}
}

/*
 * validate the grammar
 * grammar: S -> while (E) A; | A;
 *          E -> V O V
 *          A -> [identifier] = C
 *          V -> [identifier] | [integer constant]
 *          O -> < | >
 *          C -> T C1
 *          C1 -> P T C1 | [epsilon]
 *          T -> V T1
 *          T1 -> M V T1 | [epsilon]
 *          P -> + | -
 *          M -> * | /
 *
 * @src: a FILE pointer of Java lexical analysis output file
 *
 * return: 1 if valid, 0 otherwise
 */
static int do_validate_grammar(FILE * src)
{
	int ret;

	rewind(src);
	while (1) {
		ret = CALL_CHECK(S, src);
		/* error */
		if (ret == 0) {
			return 0;
		}
		/* EOF */
		else if (ret == -1) {
			return 1;
		}
	}
}

/*
 * do parse
 *
 * @src: a FILE pointer of Java lexical analysis output file
 * @out: a FILE pointer of output file
 */
static void do_parse(FILE * src, FILE * out)
{
	int ret;

	rewind(src);
	while (1) {
		if (CALL_TRANSLATE(S, src, out) == -1) {
			/* EOF */
			return;
		}
	}
}

/********************* checkers and translators *******************************/
/*
 * checkers are used in grammar check, in which phase no output is generated
 * they all have a return value of int indicating whether the input complies
 * with the grammar
 *
 * translators are used after checkers being called so they do nothing about
 * grammar check and merely do translate and output
 * most of them do not have a return value, and the only special case is the
 * translator of S, which returns an int indicating whether it meets an EOF
 */

/*
 * check statement
 * S -> while (E) A; | A;
 */
DEFINE_CHECK(S)
{
	struct word_t word;

	/* first check if no statement is available */
	get_word(src, &word);
	if (word.key == 0) {
		return -1;
	}

	/* catch a 'while' */
	if (check_word(&word, KEYWORD, "while")) {
		/* catch a '(' */
		get_word(src, &word);
		if (!check_word(&word, BRACKET_DOT, "(")) {
			goto error;
		}
		/* check E */
		if (!CALL_CHECK(E, src)) {
			goto error;
		}
		/* catch a ')' */
		get_word(src, &word);
		if (!check_word(&word, BRACKET_DOT, ")")) {
			goto error;
		}
		/* fall-through to check A */
	} else {
		/* not catch a 'while', should return the word and check A */
		return_word(&word);
	}
	/* check A */
	if (!CALL_CHECK(A, src)) {
		goto error;
	}
	/* catch a ';' */
	get_word(src, &word);
	if (!check_word(&word, SEMICOLON, ";")) {
		goto error;
	}
	return 1;

error:
	return 0;
}

/*
 * translate statement
 * S -> while (E) A; | A;
 */
DEFINE_TRANSLATE_RETURN(S)
{
	struct word_t word;
	static int begin_counter = 0;
	static int branch_counter = 0;

	/* first check if no statement is available */
	get_word(src, &word);
	if (word.key == 0) {
		return -1;
	}

	/* catch a 'while' */
	if (check_word(&word, KEYWORD, "while")) {
		/* generate label S.begin */
		fprintf(out, "begin_%d:\n", ++begin_counter);

		/* catch a '(' */
		get_word(src, &word);

		/* translate E */
		CALL_TRANSLATE(E, src, out);

		/* catch a ')' */
		get_word(src, &word);

		/* now generate branch instructions */
		pop_operator(&word);
		if (strcmp(word.value, "<") == 0) {
			/* generate 'jl' and 'jge' instructions */
			fprintf(out, "\tjl\ttrue_%d\n", ++branch_counter);
			fprintf(out, "\tjge\tfalse_%d\n", branch_counter);
		} else if (strcmp(word.value, ">") == 0) {
			/* generate 'jg' and 'jle' instructions */
			fprintf(out, "\tjg\ttrue_%d\n", ++branch_counter);
			fprintf(out, "\tjle\tfalse_%d\n", branch_counter);
		}

		/* generate label E.true */
		fprintf(out, "true_%d:\n", branch_counter);

		/* fall-through to translate A */
	} else {
		/* return the word and translate A */
		return_word(&word);
	}

	/* translate A */
	CALL_TRANSLATE(A, src, out);

	/*
	 * need to generate 'jmp' instruction and label E.false before
	 * catching a ';'
	 */
	if (word.key == COMPARE) {
		fprintf(out, "\tjmp\tbegin_%d\n", begin_counter);
		fprintf(out, "false_%d:\n", branch_counter);
	}

	/* catch a ';' */
	get_word(src, &word);

	return 0;
}

/*
 * check boolean expression
 * E -> V O V
 */
DEFINE_CHECK(E)
{
	if (!CALL_CHECK(V, src)) {
		goto error;
	}
	if (!CALL_CHECK(O, src)) {
		goto error;
	}
	if (!CALL_CHECK(V, src)) {
		goto error;
	}
	return 1;

error:
	return 0;
}

/*
 * translate boolean expression
 * E -> V O V
 */
DEFINE_TRANSLATE(E)
{
	struct word_t word, word2;

	CALL_TRANSLATE(V, src, out);
	CALL_TRANSLATE(O_P_M, src, out);
	CALL_TRANSLATE(V, src, out);

	/* get operands */
	pop_operand(&word2);
	pop_operand(&word);

	/* generate 'cmp' instruction */
	fprintf(out, "\tcmp\t%s, %s\n", word.value, word2.value);
}

/*
 * check assignment
 * A -> [identifier] = C
 */
DEFINE_CHECK(A)
{
	struct word_t word;

	/* catch an identifier */
	get_word(src, &word);
	if (!check_word(&word, IDENTIFIER, NULL)) {
		goto error;
	}
	/* catch a '=' */
	get_word(src, &word);
	if (!check_word(&word, ASSIGN, "=")) {
		goto error;
	}
	/* check C */
	if (!CALL_CHECK(C, src)) {
		goto error;
	}
	return 1;

error:
	return 0;
}

/*
 * translate assignment
 * A -> [identifier] = C
 */
DEFINE_TRANSLATE(A)
{
	struct word_t word, word2;

	/* catch an identifier */
	get_word(src, &word);

	/* catch a '=' */
	get_word(src, &word2);

	/* translate C */
	CALL_TRANSLATE(C, src, out);

	/* generate 'mov' instruction */
	pop_operand(&word2);
	fprintf(out, "\tmov\t%s, %s\n", word.value, word2.value);

	/* release operand2 if it is a register */
	if (check_word(&word2, REGISTER, NULL)) {
		free_register(get_register_no(word2.value));
	}
}

/*
 * check operands
 * V -> [identifier] | [integer constant]
 */
DEFINE_CHECK(V)
{
	struct word_t word;

	get_word(src, &word);
	if (check_word(&word, IDENTIFIER, NULL)) {
		return 1;
	}
	if (check_word(&word, INT, NULL)) {
		return 1;
	}
	return 0;
}

/*
 * translate operands
 * V -> [identifier] | [integer constant]
 */
DEFINE_TRANSLATE(V)
{
	struct word_t word;

	get_word(src, &word);
	push_operand(&word);
}

/*
 * check comparison operators
 * O -> < | >
 */
DEFINE_CHECK(O)
{
	struct word_t word;

	get_word(src, &word);
	if (check_word(&word, COMPARE, "<")) {
		return 1;
	}
	if (check_word(&word, COMPARE, ">")) {
		return 1;
	}
	/* '<=' and '>=' are not allowed */
	return 0;
}

/*
 * check arithmetics
 * C -> T C1
 */
DEFINE_CHECK(C)
{
	if (!CALL_CHECK(T, src)) {
		return 0;
	}
	if (!CALL_CHECK(C1, src)) {
		return 0;
	}
	return 1;
}

/*
 * translate arithmetics
 * C -> T C1
 */
DEFINE_TRANSLATE(C)
{
	CALL_TRANSLATE(T, src, out);
	CALL_TRANSLATE(C1, src, out);
}

/*
 * check C1
 * C1 -> P T C1 | [epsilon]
 */
DEFINE_CHECK(C1)
{
	struct word_t word;

	get_word(src, &word);
	if (check_word(&word, ADD_SUB, NULL)) {
		return_word(&word);
		if (!CALL_CHECK(P, src)) {
			return 0;
		}
		if (!CALL_CHECK(T, src)) {
			return 0;
		}
		if (!CALL_CHECK(C1, src)) {
			return 0;
		}
		return 1;
	}
	return_word(&word);
}

/*
 * translate C1
 * C1 -> P T C1 | [epsilon]
 */
DEFINE_TRANSLATE(C1)
{
	struct word_t word, word2, word3;
	int reg_no;

	get_word(src, &word);
	if (check_word(&word, ADD_SUB, NULL)) {
		return_word(&word);

		CALL_TRANSLATE(O_P_M, src, out);
		CALL_TRANSLATE(T, src, out);

		/*
		 * here we get an add/subtract operation
		 * do generate an instruction before translating C1
		 */
		pop_operand(&word3);
		pop_operator(&word2);
		pop_operand(&word);
		if (!check_word(&word, REGISTER, NULL)) {
			/*
			 * operand1 is not a register, so move it to a register
			 * first
			 */
			reg_no = alloc_register();
			fprintf(out, "\tmov\t%s, %s\n",
				get_register_name(reg_no), word.value);
			/* do move */
			word.key = REGISTER;
			strncpy(word.value, get_register_name(reg_no),
				BUF_SIZE);
		}
		/* do add/subtract */
		if (check_word(&word2, ADD_SUB, "+")) {
			fprintf(out, "\tadd\t%s, %s\n", word.value,
				word3.value);
		} else if (check_word(&word2, ADD_SUB, "-")) {
			fprintf(out, "\tsub\t%s, %s\n", word.value,
				word3.value);
		}
		/* release operand2 if it is a register */
		if (check_word(&word3, REGISTER, NULL)) {
			free_register(get_register_no(word3.value));
		}
		/* finally push the result before translating C1 */
		push_operand(&word);

		CALL_TRANSLATE(C1, src, out);
	} else {
		return_word(&word);
	}
}

/*
 * check T
 * T -> V T1
 */
DEFINE_CHECK(T)
{
	if (!CALL_CHECK(V, src)) {
		return 0;
	}
	if (!CALL_CHECK(T1, src)) {
		return 0;
	}
	return 1;
}

/*
 * translate T
 * T -> V T1
 */
DEFINE_TRANSLATE(T)
{
	CALL_TRANSLATE(V, src, out);
	CALL_TRANSLATE(T1, src, out);
}

/*
 * check T1
 * T1 -> M V T1 | [epsilon]
 */
DEFINE_CHECK(T1)
{
	struct word_t word;

	get_word(src, &word);
	if (check_word(&word, MUL_DIV, NULL)) {
		return_word(&word);
		if (!CALL_CHECK(M, src)) {
			return 0;
		}
		if (!CALL_CHECK(V, src)) {
			return 0;
		}
		if (!CALL_CHECK(T1, src)) {
			return 0;
		}
		return 1;
	}
	return_word(&word);
}

/*
 * translate T1
 * T1 -> M V T1 | [epsilon]
 */
DEFINE_TRANSLATE(T1)
{
	struct word_t word, word2, word3;
	int reg_no, reg_no2;

	get_word(src, &word);
	if (check_word(&word, MUL_DIV, NULL)) {
		return_word(&word);

		CALL_TRANSLATE(O_P_M, src, out);
		CALL_TRANSLATE(V, src, out);

		/*
		 * here we get an multiply/divide operation
		 * do generate an instruction before translating T1
		 */
		pop_operand(&word3);
		pop_operator(&word2);
		pop_operand(&word);
		if (!check_word(&word, REGISTER, "eax")) {
			/* operand1 is not eax, so move it to eax first */
			reg_no = alloc_accumulator();
			fprintf(out, "\tmov\t%s, %s\n",
				get_register_name(reg_no), word.value);
			/* release operand1 if it is a register */
			if (check_word(&word, REGISTER, NULL)) {
				free_register(get_register_no(word.value));
			}
			/* do move */
			word.key = REGISTER;
			strncpy(word.value, get_register_name(reg_no),
				BUF_SIZE);
		}
		if (check_word(&word3, INT, NULL)) {
			/* operand2 is an immediate, move it to a register */
			reg_no2 = alloc_register();
			fprintf(out, "\tmov\t%s, %s\n",
				get_register_name(reg_no2), word3.value);
			word3.key = REGISTER;
			strncpy(word3.value, get_register_name(reg_no2),
				BUF_SIZE);
		}
		/* do multiply/divide */
		if (check_word(&word2, MUL_DIV, "*")) {
			fprintf(out, "\tmul\t%s\n", word3.value);
		} else if (check_word(&word2, MUL_DIV, "/")) {
			fprintf(out, "\tdiv\t%s\n", word3.value);
		}
		/* release operand2 if it is a register */
		if (check_word(&word3, REGISTER, NULL)) {
			free_register(get_register_no(word3.value));
		}
		/* do not occupy eax */
		reg_no2 = alloc_register();
		fprintf(out, "\tmov\t%s, %s\n", get_register_name(reg_no2),
			word.value);
		strncpy(word.value, get_register_name(reg_no2), BUF_SIZE);
		free_register(reg_no);
		/* finally push the result before translating T1 */
		push_operand(&word);

		CALL_TRANSLATE(T1, src, out);
	} else {
		return_word(&word);
	}
}

/*
 * check plus and minus
 * P -> + | -
 */
DEFINE_CHECK(P)
{
	struct word_t word;

	get_word(src, &word);
	if (check_word(&word, ADD_SUB, NULL)) {
		return 1;
	}
	return 0;
}

/*
 * check mul and div
 * M -> * | /
 */
DEFINE_CHECK(M)
{
	struct word_t word;

	get_word(src, &word);
	if (check_word(&word, MUL_DIV, "*")) {
		return 1;
	}
	if (check_word(&word, MUL_DIV, "/")) {
		return 1;
	}
	/* '%' is not allowed */
	return 0;
}

/*
 * translators of O, P and M are the same
 * they just get a word and then push it into operator stack
 */
DEFINE_TRANSLATE(O_P_M)
{
	struct word_t word;

	get_word(src, &word);
	push_operator(&word);
}
