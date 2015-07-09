/*
 * lex-java.c - scanner of Java source code
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

/*
 * two styles of state handler, one having a return value, the other not
 * see line 945 for more details
 */
#define DEFINE_DO_STATE(stat) static inline void do_state_##stat(char c,\
	int * state, char * word, int * length)
#define DEFINE_DO_STATE_RETURN(stat) static inline int do_state_##stat(char c,\
	int * state, char * word, int * length)

#define BUF_SIZE 4096

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
	COMMA          = 0x120,
	BIG_BRACKET    = 0x121,
	SEMICOLON      = 0x122,
	COLON          = 0x123, /* in addition */
};

/*
 * keyword list
 * I have removed 'true' and 'false' from it since they are considered as
 * boolean constants
 */
const static char * keywords[] = {
	/* a */ "abstract",
	/* b */ "boolean", "break", "byte",
	/* c */ "case", "catch", "char", "class", "const", "continue",
	/* d */ "default", "do", "double",
	/* e */ "else", "extends",
	/* f */ "final", "finally", "float", "for",
	/* g */ "goto",
	/* i */ "if", "implements", "import", "instanceof", "int", "interface",
	/* l */ "long",
	/* n */ "native", "new", "null",
	/* p */ "package", "private", "protected", "public",
	/* r */ "return",
	/* s */ "short", "static","super","switch", "synchronized",
	/* t */ "this", "throw", "throws", "transient", "try",
	/* v */ "void", "volatile",
	/* w */ "while",
};

static void do_lex(FILE * src, FILE * out);
static inline void do_output_word(FILE * out, const char * word, int type);
static inline void do_output_wrong_word(FILE * out, const char * word,
	int lines);
static inline int do_judgement(const char * word);
static inline void do_update_word_count(int * words, int * words_in_line);
static inline void do_update_line_count(FILE * out, int * lines,
	int * words_in_line);
static inline void do_clear(char * word, int * length, int * state);
static inline void do_output_word_count(FILE * out, int words);

/* wrong word state handler */
DEFINE_DO_STATE_RETURN(m1);

/* initial state handler */
DEFINE_DO_STATE_RETURN(0);

/* keyword, boolean, identifier state handler */
DEFINE_DO_STATE_RETURN(1);

/* string, char state handler */
DEFINE_DO_STATE(3);
DEFINE_DO_STATE(5);
DEFINE_DO_STATE(6_16);
DEFINE_DO_STATE(7);
DEFINE_DO_STATE(8_9_10_18_19_20);
DEFINE_DO_STATE(11);
DEFINE_DO_STATE(12);
DEFINE_DO_STATE(13);
DEFINE_DO_STATE(15);
DEFINE_DO_STATE(17);
DEFINE_DO_STATE(21);

/* numeric constant state handler */
DEFINE_DO_STATE_RETURN(22);
DEFINE_DO_STATE_RETURN(23);
DEFINE_DO_STATE_RETURN(24);
DEFINE_DO_STATE(26);
DEFINE_DO_STATE(27);
DEFINE_DO_STATE_RETURN(28);
DEFINE_DO_STATE_RETURN(29);
DEFINE_DO_STATE(30);
DEFINE_DO_STATE_RETURN(31);
DEFINE_DO_STATE_RETURN(33);
DEFINE_DO_STATE(34);

/* operator state handler (except '/') */
DEFINE_DO_STATE_RETURN(39);
DEFINE_DO_STATE_RETURN(42);
DEFINE_DO_STATE_RETURN(45_49_57_60_64_70_72);
DEFINE_DO_STATE_RETURN(51);
DEFINE_DO_STATE_RETURN(54);
DEFINE_DO_STATE_RETURN(62);
DEFINE_DO_STATE_RETURN(66_68);

/* '/' state handler */
DEFINE_DO_STATE_RETURN(47);
DEFINE_DO_STATE_RETURN(74);
DEFINE_DO_STATE_RETURN(75);
DEFINE_DO_STATE_RETURN(77);

int main(int argc, char * const * argv)
{
	FILE * fp1 = NULL, * fp2 = NULL;
	const char * usage = "Usage: lex-java <SOURCE>\n\n";
	char err_msg[BUF_SIZE];

	/* restrict exactly 2 arguments */
	if (argc != 2) {
		fprintf(stderr, "%s", usage);
		goto error;
	}

	/* open source file */
	if ((fp1 = fopen(argv[1], "r")) == NULL) {
		snprintf(err_msg, BUF_SIZE, "lex-java: cannot open '%s'",
			argv[1]);
		perror(err_msg);
		goto error;
	}

	/* open output file */
	if ((fp2 = fopen("scanner_output", "w")) == NULL) {
		perror("lex-java: cannot open 'scanner_output'");
		goto error;
	}

	/* do lexical analysis */
	do_lex(fp1, fp2);

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

/*
 * do lexical analysis
 *
 * @src: a FILE pointer of Java source file
 * @out: a FILE pointer of output file
 */
static void do_lex(FILE * src, FILE * out)
{
	char buffer[BUF_SIZE << 1]; /* double-sized buffer */
	int pos = 0, end = 0;
	size_t nread;
	int i;

	int state = 0, condition_flag = 0, tmp;
	int words = 0, lines = 0;
	int words_in_line = 0;
	int length = 0;
	char word[BUF_SIZE] = { 0 };

	while (!end) {
		nread = fread(buffer + pos, sizeof(char), BUF_SIZE, src);
		/* manually add a newline at the last buffer */
		if (nread < BUF_SIZE) {
			buffer[pos + nread] = '\n';
			++nread;
			end = 1;
		}

		i = 0; /* character counter in current buffer */
		while (i < nread) {
			switch (state) {
			/* inside a wrong word */
			case -1:
				if (!do_state_m1(buffer[pos + i], &state, word,
					&length)) {
					++i;
				}
				break;

			/* get a wrong word */
			case -2:
				do_output_wrong_word(out, word, lines + 1);
				do_update_word_count(&words, &words_in_line);
				do_clear(word, &length, &state);
				break;

			/* initial */
			case 0:
				tmp = do_state_0(buffer[pos + i], &state, word,
					&length);
				if (tmp) {
					if (!condition_flag) {
						condition_flag = tmp;
						word[--length] = '\0';
					} else {
						state = -1;
					}
				}
				++i;
				break;

			/* inside a keyword, boolean value or identifier */
			case 1:
				if (!do_state_1(buffer[pos + i], &state, word,
					&length)) {
					++i;
				}
				break;

			/* get a keyword, boolean value or identifier */
			case 2:
				do_output_word(out, word, do_judgement(word));
				do_update_word_count(&words, &words_in_line);
				do_clear(word, &length, &state);
				break;

			/* inside a string */
			case 3:
				do_state_3(buffer[pos + i], &state, word,
					&length);
				++i;
				break;

			/* get a string */
			case 4:
				do_output_word(out, word, STRING);
				do_update_word_count(&words, &words_in_line);
				do_clear(word, &length, &state);
				break;

			/* inside a string and after a back slash */
			case 5:
				do_state_5(buffer[pos + i], &state, word,
					&length);
				++i;
				break;

			/*
			 * inside a string or a char and after a back slash and
			 * an octal digit
			 */
			case 6: case 16:
				do_state_6_16(buffer[pos + i], &state, word,
					&length);
				++i;
				break;

			/*
			 * inside a string and after a back slash and 2 octal
			 * digits
			 */
			case 7:
				do_state_7(buffer[pos + i], &state, word,
					&length);
				++i;
				break;

			/*
			 * inside a string or a char and after a back slash and
			 * a char 'u' and 0~2 hexadecimal digits
			 */
			case 8: case 9: case 10: /* string */
			case 18: case 19: case 20: /* char */
				do_state_8_9_10_18_19_20(buffer[pos + i],
					&state, word, &length);
				++i;
				break;

			/*
			 * inside a string and after a back slash and a char 'u'
			 * and 3 hexadecimal digits
			 */
			case 11:
				do_state_11(buffer[pos + i], &state, word,
					&length);
				++i;
				break;

			/* inside a char and do not have a char */
			case 12:
				do_state_12(buffer[pos + i], &state, word,
					&length);
				++i;
				break;

			/* inside a char and have a char */
			case 13:
				do_state_13(buffer[pos + i], &state, word,
					&length);
				++i;
				break;

			/* get a char */
			case 14:
				do_output_word(out, word, CHAR);
				do_update_word_count(&words, &words_in_line);
				do_clear(word, &length, &state);
				break;

			/* inside a char and after a back slash */
			case 15:
				do_state_15(buffer[pos + i], &state, word,
					&length);
				++i;
				break;

			/*
			 * inside a char and after a back slash and two octal
			 * digits
			 */
			case 17:
				do_state_17(buffer[pos + i], &state, word,
					&length);
				++i;
				break;

			/*
			 * inside a char and after a back slash and a char 'u'
			 * and 3 hexadecimal digits
			 */
			case 21:
				do_state_21(buffer[pos + i], &state, word,
					&length);
				++i;
				break;

			/* catch a dot */
			case 22:
				if (do_state_22(buffer[pos + i], &state, word,
					&length)) {
					do_output_word(out, word, BRACKET_DOT);
					do_update_word_count(&words,
						&words_in_line);
					do_clear(word, &length, &state);
				} else {
					++i;
				}
				break;

			/* catch a '1' ~ '9' */
			case 23:
				if (do_state_23(buffer[pos + i], &state, word,
					&length)) {
					do_output_word(out, word, INT);
					do_update_word_count(&words,
						&words_in_line);
					do_clear(word, &length, &state);
				} else {
					++i;
				}
				break;

			/* a float without 'f', 'F', 'd', 'D' or 'e', 'E' */
			case 24:
				if (do_state_24(buffer[pos + i], &state, word,
					&length)) {
					do_output_word(out, word, FLOAT);
					do_update_word_count(&words,
						&words_in_line);
					do_clear(word, &length, &state);
				} else {
					++i;
				}
				break;

			/* a float ending with 'f', 'F', 'd' or 'D' */
			case 25:
				do_output_word(out, word, FLOAT);
				do_update_word_count(&words, &words_in_line);
				do_clear(word, &length, &state);
				break;

			/* a float ending with 'e' or 'E' */
			case 26:
				do_state_26(buffer[pos + i], &state, word,
					&length);
				++i;
				break;

			/* a float ending with 'e+', 'e-', 'E+' or 'E-' */
			case 27:
				do_state_27(buffer[pos + i], &state, word,
					&length);
				++i;
				break;

			/* a float ending with 'e' or 'E' and a valid number */
			case 28:
				if (do_state_28(buffer[pos + i], &state, word,
					&length)) {
					do_output_word(out, word, FLOAT);
					do_update_word_count(&words,
						&words_in_line);
					do_clear(word, &length, &state);
				} else {
					++i;
				}
				break;

			/* catch a '0' */
			case 29:
				if (do_state_29(buffer[pos + i], &state, word,
					&length)) {
					do_output_word(out, word, INT);
					do_update_word_count(&words,
						&words_in_line);
					do_clear(word, &length, &state);
				} else {
					++i;
				}
				break;

			/* catch a '0x' or '0X' */
			case 30:
				do_state_30(buffer[pos + i], &state, word,
					&length);
				++i;
				break;

			/* int in hexadecimal */
			case 31:
				if (do_state_31(buffer[pos + i], &state, word,
					&length)) {
					do_output_word(out, word, INT);
					do_update_word_count(&words,
						&words_in_line);
					do_clear(word, &length, &state);
				} else {
					++i;
				}
				break;

			/* int ending with 'l' or 'L' */
			case 32:
				do_output_word(out, word, INT);
				do_update_word_count(&words, &words_in_line);
				do_clear(word, &length, &state);
				break;

			/* int in octal */
			case 33:
				if (do_state_33(buffer[pos + i], &state, word,
					&length)) {
					do_output_word(out, word, INT);
					do_update_word_count(&words,
						&words_in_line);
					do_clear(word, &length, &state);
				} else {
					++i;
				}
				break;

			/*
			 * number starting with '0' and have occurrence of '8'
			 * or '9'
			 */
			case 34:
				do_state_34(buffer[pos + i], &state, word,
					&length);
				++i;
				break;

			/* catch a '[', ']', '(' or ')' */
			case 35:
				do_output_word(out, word, BRACKET_DOT);
				do_update_word_count(&words, &words_in_line);
				do_clear(word, &length, &state);
				break;

			/* catch a ',' */
			case 36:
				do_output_word(out, word, COMMA);
				do_update_word_count(&words, &words_in_line);
				do_clear(word, &length, &state);
				break;

			/* catch a '{' or '}' */
			case 37:
				do_output_word(out, word, BIG_BRACKET);
				do_update_word_count(&words, &words_in_line);
				do_clear(word, &length, &state);
				break;

			/* catch a ';' */
			case 38:
				do_output_word(out, word, SEMICOLON);
				do_update_word_count(&words, &words_in_line);
				do_clear(word, &length, &state);
				break;

			/* catch a '+' */
			case 39:
				if (do_state_39(buffer[pos + i], &state, word,
					&length)) {
					do_output_word(out, word, ADD_SUB);
					do_update_word_count(&words,
						&words_in_line);
					do_clear(word, &length, &state);
				} else {
					++i;
				}
				break;

			/*
			 * catch a '+=', '-=', '*=', '/=', '%=',
			 * '&=', '|=', '^=', '<<=', '>>=' or '>>>='
			 */
			case 40: case 43: case 46: case 48: case 50:
			case 52: case 55: case 58: case 65: case 69: case 71:
				do_output_word(out, word, ASSIGN);
				do_update_word_count(&words, &words_in_line);
				do_clear(word, &length, &state);
				break;

			/* catch a '++', '--' or '~' */
			case 41: case 44: case 59:
				do_output_word(out, word, PLUSPLUS);
				do_update_word_count(&words, &words_in_line);
				do_clear(word, &length, &state);
				break;

			/* catch a '-' */
			case 42:
				if (do_state_42(buffer[pos + i], &state, word,
					&length)) {
					do_output_word(out, word, ADD_SUB);
					do_update_word_count(&words,
						&words_in_line);
					do_clear(word, &length, &state);
				} else {
					++i;
				}
				break;

			/* catch a '*' or '%' */
			case 45: case 49:
				if (do_state_45_49_57_60_64_70_72(buffer[pos + i],
					&state, word, &length)) {
					do_output_word(out, word, MUL_DIV);
					do_update_word_count(&words,
						&words_in_line);
					do_clear(word, &length, &state);
				} else {
					++i;
				}
				break;

			/* catch a '/' */
			case 47:
				if (do_state_47(buffer[pos + i], &state, word,
					&length)) {
					do_output_word(out, word, MUL_DIV);
					do_update_word_count(&words,
						&words_in_line);
					do_clear(word, &length, &state);
				} else {
					++i;
				}
				break;

			/* catch a '&' */
			case 51:
				if (do_state_51(buffer[pos + i], &state, word,
					&length)) {
					do_output_word(out, word, BIT_AND);
					do_update_word_count(&words,
						&words_in_line);
					do_clear(word, &length, &state);
				} else {
					++i;
				}
				break;

			/* catch a '&&' */
			case 53:
				do_output_word(out, word, LOGIC_AND);
				do_update_word_count(&words, &words_in_line);
				do_clear(word, &length, &state);
				break;

			/* catch a '|' */
			case 54:
				if (do_state_54(buffer[pos + i], &state, word,
					&length)) {
					do_output_word(out, word, BIT_OR);
					do_update_word_count(&words,
						&words_in_line);
					do_clear(word, &length, &state);
				} else {
					++i;
				}
				break;

			/* catch a '||' */
			case 56:
				do_output_word(out, word, LOGIC_OR);
				do_update_word_count(&words, &words_in_line);
				do_clear(word, &length, &state);
				break;

			/* catch a '^' */
			case 57:
				if (do_state_45_49_57_60_64_70_72(buffer[pos + i],
					&state, word, &length)) {
					do_output_word(out, word, XOR);
					do_update_word_count(&words,
						&words_in_line);
					do_clear(word, &length, &state);
				} else {
					++i;
				}
				break;

			/* catch a '!' */
			case 60:
				if (do_state_45_49_57_60_64_70_72(buffer[pos + i],
					&state, word, &length)) {
					do_output_word(out, word, PLUSPLUS);
					do_update_word_count(&words,
						&words_in_line);
					do_clear(word, &length, &state);
				} else {
					++i;
				}
				break;

			/* catch a '!=' or '==' */
			case 61: case 73:
				do_output_word(out, word, EQUAL);
				do_update_word_count(&words, &words_in_line);
				do_clear(word, &length, &state);
				break;

			/* catch a '<' */
			case 62:
				if (do_state_62(buffer[pos + i], &state, word,
					&length)) {
					do_output_word(out, word, COMPARE);
					do_update_word_count(&words,
						&words_in_line);
					do_clear(word, &length, &state);
				} else {
					++i;
				}
				break;

			/* catch a '<=' or '>=' */
			case 63: case 67:
				do_output_word(out, word, COMPARE);
				do_update_word_count(&words, &words_in_line);
				do_clear(word, &length, &state);
				break;

			/* catch a '<<' or '>>>' */
			case 64: case 70:
				if (do_state_45_49_57_60_64_70_72(buffer[pos + i],
					&state, word, &length)) {
					do_output_word(out, word, SHIFT);
					do_update_word_count(&words,
						&words_in_line);
					do_clear(word, &length, &state);
				} else {
					++i;
				}
				break;

			/* catch a '>' */
			case 66:
				if (do_state_66_68(buffer[pos + i], &state,
					word, &length)) {
					do_output_word(out, word, COMPARE);
					do_update_word_count(&words,
						&words_in_line);
					do_clear(word, &length, &state);
				} else {
					++i;
				}
				break;

			/* catch a '>>' */
			case 68:
				if (do_state_66_68(buffer[pos + i], &state,
					word, &length)) {
					do_output_word(out, word, SHIFT);
					do_update_word_count(&words,
						&words_in_line);
					do_clear(word, &length, &state);
				} else {
					++i;
				}
				break;

			/* catch a '=' */
			case 72:
				if (do_state_45_49_57_60_64_70_72(buffer[pos + i],
					&state, word, &length)) {
					do_output_word(out, word, ASSIGN);
					do_update_word_count(&words,
						&words_in_line);
					do_clear(word, &length, &state);
				} else {
					++i;
				}
				break;

			/* catch a '/*', block comment start */
			case 74:
				if (do_state_74(buffer[pos + i], &state, word,
					&length)) {
					do_update_line_count(out, &lines,
						&words_in_line);
				}
				++i;
				break;

			/* catch a '*' in block comment */
			case 75:
				if (do_state_75(buffer[pos + i], &state, word,
					&length)) {
					do_update_line_count(out, &lines,
						&words_in_line);
				}
				++i;
				break;

			// catch a '*/' in block comment, block comment end
			case 76:
				do_clear(word, &length, &state);
				break;

			/* catch a '//', line comment start */
			case 77:
				if (do_state_77(buffer[pos + i], &state, word,
					&length)) {
					do_update_line_count(out, &lines,
						&words_in_line);
				}
				++i;
				break;

			/* catch a '\n' in line comment, line comment end */
			case 78:
				do_clear(word, &length, &state);
				break;

			/* catch a ' ', '\t' or '\r' */
			case 79:
				do_output_word(out, word, SPACE);
				do_update_word_count(&words, &words_in_line);
				do_clear(word, &length, &state);
				break;

			/* catch a '\n' */
			case 80:
				do_output_word(out, word, SPACE);
				do_update_word_count(&words, &words_in_line);
				do_update_line_count(out, &lines,
					&words_in_line);
				do_clear(word, &length, &state);
				break;

			/* catch a ':' after '?' */
			case 81:
				if (condition_flag) {
					do_output_word(out, "?:", CONDITION);
					do_update_word_count(&words,
						&words_in_line);
					do_clear(word, &length, &state);
				} else {
					do_output_word(out, word, COLON);
					do_update_word_count(&words,
						&words_in_line);
					do_clear(word, &length, &state);
				}
				break;

			default:
				fprintf(stderr, "illegal state %d\n", state);
				return;
			}
		}
		pos = (pos + BUF_SIZE) % (BUF_SIZE << 1);
	}
	do_output_word_count(out, words);
}

/*
 * print a word to output file
 *
 * @out: a FILE pointer of output file
 * @word: word to print
 * @type: word type, see attribute list at line 35
 */
static inline void do_output_word(FILE * out, const char * word, int type)
{
	fprintf(out, "0x%x\t%s\n", type, word);
}

/*
 * print a wrong word with line number to output file
 *
 * @out: a FILE pointer of output file
 * @word: wrong word to print
 * @lines: current line number
 */
static inline void do_output_wrong_word(FILE * out, const char * word,
	int lines)
{
	fprintf(out, "0x%x\t%s at line %d\n", WRONG, word, lines);
}

/*
 * determine if a word is a boolean value, a keyword or an identifier
 *
 * @word: word to judge
 *
 * return: word type, see attribute list at line 35
 */
static inline int do_judgement(const char * word)
{
	int i;

	if (strcmp(word, "true") == 0 || strcmp(word, "false") == 0) {
		return BOOLEAN;
	}

	for (i = 0; i < ARRAY_SIZE(keywords); ++i) {
		if (strcmp(word, keywords[i]) == 0) {
			return KEYWORD;
		}
	}

	return IDENTIFIER;
}

/*
 * update word count
 *
 * @words: total word count
 * @words_in_line: current line word count
 */
static inline void do_update_word_count(int * words, int * words_in_line)
{
	++*words;
	++*words_in_line;
}

/*
 * update and print line count and in-line word count
 *
 * @out: a FILE pointer of output file
 * @lines: line count
 * @words_in_line: current line word count
 */
static inline void do_update_line_count(FILE * out, int * lines,
	int * words_in_line)
{
	++*lines;
	if (*words_in_line < 2) {
		fprintf(out, "line %d has %d word\n", *lines, *words_in_line);
	} else {
		fprintf(out, "line %d has %d words\n", *lines, *words_in_line);
	}
	*words_in_line = 0;
}

/*
 * clear current word and set state to 0
 *
 * @word: current word
 * @length: length of current word
 * @state: DFA state
 */
static inline void do_clear(char * word, int * length, int * state)
{
	memset(word, 0, BUF_SIZE * sizeof(char));
	*length = 0;
	*state = 0;
}

/*
 * print total word count to output file
 *
 * @out: a FILE pointer of output file
 * @words: total word count
 */
static inline void do_output_word_count(FILE * out, int words)
{
	fprintf(out, "total %d words\n", words);
}

/***************************** DFA state handlers *****************************/
/*
 * most handlers do not have a return value, or have a return value of int
 * indicating whether it should accept the current word or move on scanning
 *
 * there are also comment handlers returning an int indicating whether it meets
 * a newline
 *
 * the only special case is state 0 handler, whose return value indicates
 * whether it meets a '?'
 */

/* finish */
DEFINE_DO_STATE_RETURN(m1)
{
	switch (c) {
	/* reach the end of a wrong word when it meets a space or a delimiter */
	case ' ': case '\t': case '\r': case '\n':
	case '{': case '}': case '[': case ']': case '(': case ')':
	case ',': case '.': case ';':
		*state = -2;
		return 1;

	default:
		word[(*length)++] = c;
		return 0;
	}
}

/* finish */
DEFINE_DO_STATE_RETURN(0)
{
	word[(*length)++] = c;

	if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		c == '$' || c == '_') {
		*state = 1;
		return 0;
	} else if (c >= '1' && c <= '9') {
		*state = 23;
		return 0;
	}

	switch (c) {
	case '\"':
		*state = 3;
		break;

	case '\'':
		*state = 12;
		break;

	case '.':
		*state = 22;
		break;

	case '0':
		*state = 29;
		break;

	case '[': case ']': case '(': case ')':
		*state = 35;
		break;

	case ',':
		*state = 36;
		break;

	case '{': case '}':
		*state = 37;
		break;

	case ';':
		*state = 38;
		break;

	case '+':
		*state = 39;
		break;

	case '-':
		*state = 42;
		break;

	case '*':
		*state = 45;
		break;

	case '/':
		*state = 47;
		break;

	case '%':
		*state = 49;
		break;

	case '&':
		*state = 51;
		break;

	case '|':
		*state = 54;
		break;

	case '^':
		*state = 57;
		break;

	case '~':
		*state = 59;
		break;

	case '!':
		*state = 60;
		break;

	case '<':
		*state = 62;
		break;

	case '>':
		*state = 66;
		break;

	case '=':
		*state = 72;
		break;

	case ' ':
		*state = 79;
		break;

	case '\t':
		*state = 79;
		word[*length - 1] = '\\';
		word[(*length)++] = 't';
		break;

	case '\r':
		*state = 79;
		word[*length - 1] = '\\';
		word[(*length)++] = 'r';
		break;

	case '\n':
		*state = 80;
		word[*length - 1] = '\\';
		word[(*length)++] = 'n';
		break;

	case '?':
		return 1;

	case ':':
		*state = 81;
		break;

	default:
		*state = -1;
		break;
	}

	return 0;
}

/* finish */
DEFINE_DO_STATE_RETURN(1)
{
	if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		(c >= '0' && c <= '9') || c == '$' || c == '_') {
		word[(*length)++] = c;
		return 0;
	} else {
		*state = 2;
		return 1;
	}
}

/* finish */
DEFINE_DO_STATE(3)
{
	word[(*length)++] = c;

	if (c == '\"') {
		*state = 4;
	} else if (c == '\\') {
		*state = 5;
	}
}

/* finish */
DEFINE_DO_STATE(5)
{
	word[(*length)++] = c;

	if (c >= '0' && c <= '7') {
		*state = 6;
		return;
	}

	switch (c) {
	case '\\': case '\'': case '\"': case 'r': case 'n': case 'f': case 't':
	case 'b':
		*state = 3;
		break;

	case 'u':
		*state = 8;
		break;

	default:
		*state = -1;
		break;
	}
}

/* finish */
DEFINE_DO_STATE(6_16)
{
	word[(*length)++] = c;

	if (c >= '0' && c <= '7') {
		++*state;
	} else {
		*state = -1;
	}
}

/* finish */
DEFINE_DO_STATE(7)
{
	word[(*length)++] = c;

	if (c >= '0' && c <= '7') {
		*state = 3;
	} else {
		*state = -1;
	}
}

/* finish */
DEFINE_DO_STATE(8_9_10_18_19_20)
{
	word[(*length)++] = c;

	if ((c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f') ||
		(c >= '0' && c <= '9')) {
		++*state;
	} else {
		*state = -1;
	}
}

/* finish */
DEFINE_DO_STATE(11)
{
	word[(*length)++] = c;

	if ((c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f') ||
		(c >= '0' && c <= '9')) {
		*state = 3;
	} else {
		*state = -1;
	}
}

/* finish */
DEFINE_DO_STATE(12)
{
	word[(*length)++] = c;

	if (c == '\\') {
		*state = 15;
	} else {
		*state = 13;
	}
}

/* finish */
DEFINE_DO_STATE(13)
{
	word[(*length)++] = c;

	if (c == '\'') {
		*state = 14;
	} else {
		*state = -1;
	}
}

/* finish */
DEFINE_DO_STATE(15)
{
	word[(*length)++] = c;

	if (c >= '0' && c <= '7') {
		*state = 16;
		return;
	}

	switch (c) {
	case '\\': case '\'': case '\"': case 'r': case 'n': case 'f': case 't':
	case 'b':
		*state = 13;
		break;

	case 'u':
		*state = 18;
		break;

	default:
		*state = -1;
		break;
	}
}

/* finish */
DEFINE_DO_STATE(17)
{
	word[(*length)++] = c;

	if (c >= '0' && c <= '7') {
		*state = 13;
	} else {
		*state = -1;
	}
}

/* finish */
DEFINE_DO_STATE(21)
{
	word[(*length)++] = c;

	if ((c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f') ||
		(c >= '0' && c <= '9')) {
		*state = 13;
	} else {
		*state = -1;
	}
}

/* finish */
DEFINE_DO_STATE_RETURN(22)
{
	if (c >= '0' && c <= '9') {
		word[(*length)++] = c;
		*state = 24;
		return 0;
	} else {
		return 1;
	}
}

/* finish */
DEFINE_DO_STATE_RETURN(23)
{
	if (c >= '0' && c <= '9') {
		word[(*length)++] = c;
		return 0;
	}

	switch (c) {
	case '.':
		word[(*length)++] = c;
		*state = 24;
		return 0;

	case 'f': case 'F': case 'd': case 'D':
		word[(*length)++] = c;
		*state = 25;
		return 0;

	case 'e': case 'E':
		word[(*length)++] = c;
		*state = 26;
		return 0;

	case 'l': case 'L':
		word[(*length)++] = c;
		*state = 33;
		return 0;

	default:
		return 1;
	}
}

/* finish */
DEFINE_DO_STATE_RETURN(24)
{
	if (c >= '0' && c <= '9') {
		word[(*length)++] = c;
		return 0;
	}

	switch (c) {
	case 'f': case 'F': case 'd': case 'D':
		word[(*length)++] = c;
		*state = 25;
		return 0;

	case 'e': case 'E':
		word[(*length)++] = c;
		*state = 26;
		return 0;

	default:
		return 1;
	}
}

/* finish */
DEFINE_DO_STATE(26)
{
	word[(*length)++] = c;

	if (c >= '0' && c <= '9') {
		*state = 28;
	} else if (c == '-' || c == '+') {
		*state = 27;
	} else {
		*state = -1;
	}
}

/* finish */
DEFINE_DO_STATE(27)
{
	word[(*length)++] = c;

	if (c >= '0' && c <= '9') {
		*state = 28;
	} else {
		*state = -1;
	}
}

/* finish */
DEFINE_DO_STATE_RETURN(28)
{
	if (c >= '0' && c <= '9') {
		word[(*length)++] = c;
		return 0;
	}

	switch (c) {
	case 'f': case 'F': case 'd': case 'D':
		word[(*length)++] = c;
		*state = 25;
		return 0;

	default:
		return 1;
	}
}

/* finish */
DEFINE_DO_STATE_RETURN(29)
{
	if (c >= '0' && c <= '7') {
		word[(*length)++] = c;
		*state = 33;
		return 0;
	}

	switch (c) {
	case 'x': case 'X':
		word[(*length)++] = c;
		*state = 30;
		return 0;

	case 'l': case 'L':
		word[(*length)++] = c;
		*state = 32;
		return 0;

	case 'e': case 'E':
		word[(*length)++] = c;
		*state = 26;
		return 0;

	case 'f': case 'F': case 'd': case 'D':
		word[(*length)++] = c;
		*state = 25;
		return 0;

	case '8': case '9':
		word[(*length)++] = c;
		*state = 34;
		return 0;

	case '.':
		word[(*length)++] = c;
		*state = 24;
		return 0;

	default:
		return 1;
	}
}

/* finish */
DEFINE_DO_STATE(30)
{
	word[(*length)++] = c;

	if ((c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f') ||
		(c >= '0' && c <= '9')) {
		*state = 31;
	} else {
		*state = -1;
	}
}

/* finish */
DEFINE_DO_STATE_RETURN(31)
{
	if ((c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f') ||
		(c >= '0' && c <= '9')) {
		word[(*length)++] = c;
		return 0;
	} else if (c == 'l' || c == 'L') {
		word[(*length)++] = c;
		*state = 32;
		return 0;
	} else {
		return 1;
	}
}

/* finish */
DEFINE_DO_STATE_RETURN(33)
{
	if (c >= '0' && c <= '7') {
		word[(*length)++] = c;
		return 0;
	}

	switch (c) {
	case '8': case '9':
		word[(*length)++] = c;
		*state = 34;
		return 0;

	case 'l': case 'L':
		word[(*length)++] = c;
		*state = 32;
		return 0;

	case 'e': case 'E':
		word[(*length)++] = c;
		*state = 26;
		return 0;

	case 'f': case 'F': case 'd': case 'D':
		word[(*length)++] = c;
		*state = 25;
		return 0;

	default:
		return 1;
	}
}

/* finish */
DEFINE_DO_STATE(34)
{
	word[(*length)++] = c;

	if (c >= '0' && c <= '9') {
		return;
	}

	switch (c) {
	case 'e': case 'E':
		*state = 26;
		break;

	case 'f': case 'F': case 'd': case 'D':
		*state = 25;
		break;

	default:
		*state = -1;
		break;
	}
}

/* finish */
DEFINE_DO_STATE_RETURN(39)
{
	if (c == '=') {
		word[(*length)++] = c;
		*state = 40;
		return 0;
	} else if (c == '+') {
		word[(*length)++] = c;
		*state = 41;
		return 0;
	} else {
		return 1;
	}
}

/* finish */
DEFINE_DO_STATE_RETURN(42)
{
	if (c == '=') {
		word[(*length)++] = c;
		*state = 43;
		return 0;
	} else if (c == '-') {
		word[(*length)++] = c;
		*state = 44;
		return 0;
	} else {
		return 1;
	}
}

/* finish */
DEFINE_DO_STATE_RETURN(45_49_57_60_64_70_72)
{
	if (c == '=') {
		word[(*length)++] = c;
		++*state;
		return 0;
	} else {
		return 1;
	}
}

/* finish */
DEFINE_DO_STATE_RETURN(51)
{
	if (c == '=') {
		word[(*length)++] = c;
		*state = 52;
		return 0;
	} else if (c == '&') {
		word[*length] = c;
		*state = 53;
		return 0;
	} else {
		return 1;
	}
}

/* finish */
DEFINE_DO_STATE_RETURN(54)
{
	if (c == '=') {
		word[(*length)++] = c;
		*state = 55;
		return 0;
	} else if (c == '|') {
		word[*length] = c;
		*state = 56;
		return 0;
	} else {
		return 1;
	}
}

/* finish */
DEFINE_DO_STATE_RETURN(62)
{
	if (c == '=') {
		word[(*length)++] = c;
		*state = 63;
		return 0;
	} else if (c == '<') {
		word[*length] = c;
		*state = 64;
		return 0;
	} else {
		return 1;
	}
}

/* finish */
DEFINE_DO_STATE_RETURN(66_68)
{
	if (c == '=') {
		word[(*length)++] = c;
		++*state;
		return 0;
	} else if (c == '>') {
		word[*length] = c;
		*state += 2;
		return 0;
	} else {
		return 1;
	}
}

/* finish */
DEFINE_DO_STATE_RETURN(47)
{
	if (c == '=') {
		word[(*length)++] = c;
		*state = 48;
		return 0;
	} else if (c == '*') {
		do_clear(word, length, state);
		*state = 74;
		return 0;
	} else if (c == '/') {
		do_clear(word, length, state);
		*state = 77;
		return 0;
	} else {
		return 1;
	}
}

/* finish */
DEFINE_DO_STATE_RETURN(74)
{
	if (c == '*') {
		*state = 75;
	} else if (c == '\n') {
		return 1;
	}

	return 0;
}

/* finish */
DEFINE_DO_STATE_RETURN(75)
{
	switch (c) {
	case '/':
		*state = 76;
		break;

	case '*':
		break;

	case '\n':
		return 1;

	default:
		*state = 74;
		break;
	}

	return 0;
}

/* finish */
DEFINE_DO_STATE_RETURN(77)
{
	if (c == '\n') {
		*state = 78;
		return 1;
	}

	return 0;
}
