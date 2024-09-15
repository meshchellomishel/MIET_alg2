#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_FN_LEN 10

#define MIN(X, Y)			\
	({ __typeof__(X) _X = (X);	\
	__typeof__(Y) _Y = (Y);	\
	_X < _Y ? _X : _Y; })
#define ARRAY_SIZE(arr) \
	(sizeof((arr)) / sizeof((arr)[0]))

#define OP_EQ '='
#define OP_AD '+'
#define OP_MN '-'
#define OP_DT '*'
#define OP_FR '\\'
#define OP_L_BR '('
#define OP_R_BR ')'
#define OP_FN_LOG "log"


static const char ops[] = {
	OP_EQ, OP_AD, OP_MN,
	OP_DT, OP_FR,
	OP_L_BR, OP_R_BR
};

static double _log(double *args, unsigned int args_len)
{
	return 0;
}

struct func {
	char *name;

	double (*func)(double *args, unsigned int args_len);

	double *args;
	unsigned int args_len;
};

static const struct func allowed_functions[] = {
	{ "log", _log },
};

enum element_type {
	ELEMENT_UNSPEC = 0,
	ELEMENT_OPERATOR,
	ELEMENT_DIGIT,
	ELEMENT_FUNCTION,

	ELEMENT_L_BRACK,
	ELEMENT_R_BRACK,
};

enum op_type {
	OP_UNSPEC = 0,
	OP_ADD,
	OP_MINUS,
	OP_DOT,
	OP_FRAC,
};

struct element {
	enum element_type type;

	double value;
	enum op_type op;

	struct func *func;
};

struct ctx {
	char input[4096];
	unsigned int size;

	struct element *stack;
};

static bool str_equal(const char *s1, const char *s2)
{
	if (!s1 && !s2)
		return true;
	else if (s1 && !s2)
		return false;
	else if (!s1 && s2)
		return false;

	if (strcmp(s1, s2))
		return false;
	return true;
}

static bool other_validate(const char c)
{
	if (c == ' ')
		return true;
	return false;
}

static enum op_type op_validate(const char c)
{
	switch (c) {
	case OP_AD:
		return OP_ADD;
	case OP_MN:
		return OP_MINUS;
	case OP_DT:
		return OP_DOT;
	case OP_FR:
		return OP_FRAC;
	default:
		return OP_UNSPEC;
	}
}

static char *slice(const char *arr,
		   int start, int end)
{
	double result;
	char *buf = calloc(end - start + 2, sizeof(char));

	if (!buf) {
		printf("[CRIT]: Failed to allocate array");
		exit(1);
	}

	memcpy(buf, (char *)(arr + start), end + 1);
	return buf;
}

static int func_validate(const char *stack,
			 int start, int end,
			 struct func *func)
{
	int ret = -1;
	char *buf = slice(stack, start, end);

	for (int i = 0; i < 0; i++) {
		if (str_equal(buf, allowed_functions[i].name)) {
			func->name = allowed_functions[i].name;
			func->func = allowed_functions[i].func;
			ret = 0;
			break;
		}
	}

	free(buf);
	return ret;
}

static double double_validate(const char *stack,
			      int start, int end)
{
	double result;
	char *buf = slice(stack, start, end);

	result = atof(buf);

	free(buf);
	return result;
}

enum process_states {
	STACK_UNSPEC,
	STACK_DIGIT,
	STACK_FUNC,
	STACK_FUNC_ARGS,
};

#define ENOLEFTBRACK -1
#define ENORIGHTBRACK -2

static int ctx_process(struct ctx *ctx)
{
	int ret, buf;
	struct element current_el;
	char fn_stack[MAX_FN_LEN] = {};
	int l_br_count = 0, r_br_count = 0;
	int d_stack_start = -1, d_stack_end = -1;
	enum process_states stack_state = STACK_UNSPEC;

	for (int i = 0; i < ctx->size; i++) {
		char sym = ctx->input[i];

		if ((isdigit(sym) ||
		     sym == '.') &&
		    (stack_state == STACK_UNSPEC ||
		     stack_state == STACK_DIGIT))  {
			if (stack_state != STACK_DIGIT)
				d_stack_start = i;
			stack_state = STACK_DIGIT;
			continue;
		} else if (stack_state == STACK_DIGIT) {
			d_stack_end = i - 1;

			stack_state = STACK_UNSPEC;
			current_el.type = ELEMENT_DIGIT;
			current_el.value = double_validate(ctx->input,
							   d_stack_start,
							   d_stack_end);
			printf("Parsed double: %f [%d, %d]\n",
			       current_el.value, d_stack_start,
			       d_stack_end);

			d_stack_start = -1;
			d_stack_end = -1;
		}

		if ((isalpha(sym) && stack_state == STACK_UNSPEC) ||
		    (isdigit(sym) && stack_state == STACK_FUNC)) {
			if (stack_state == STACK_UNSPEC)
				d_stack_start = i;
			stack_state = STACK_FUNC;
			continue;
		} else if (sym == OP_L_BR &&
			   stack_state == STACK_FUNC) {
			d_stack_end = i - 1;

			stack_state = STACK_FUNC_ARGS;
			current_el.type = ELEMENT_FUNCTION;
			ret = func_validate(ctx->input, d_stack_start,
					    d_stack_end, current_el.func);
			if (ret < 0)
				return ret;

			printf("Parsed function: %s [%d, %d]\n",
			       current_el.func->name, d_stack_start,
			       d_stack_end);

			d_stack_start = -1;
			d_stack_end = -1;
		} else if (sym == OP_R_BR &&
			   stack_state == STACK_FUNC_ARGS) {
			stack_state = STACK_UNSPEC;
		}

		buf = op_validate(sym);
		if (buf) {
			current_el.type = ELEMENT_OPERATOR;
			current_el.op = buf;

			printf("Parsed op: %d\n", current_el.op);
			continue;
		}

		if (sym == OP_L_BR) {
			l_br_count++;
			continue;
		}

		if (sym == OP_R_BR) {
			r_br_count++;
			continue;
		}

		if (other_validate(sym))
			continue;

		return i + 1;
	}

	if (stack_state != STACK_UNSPEC) {
		if (stack_state == STACK_DIGIT) {
			d_stack_end = ctx->size - 1;

			stack_state = STACK_UNSPEC;
			current_el.type = ELEMENT_DIGIT;
			current_el.value = double_validate(ctx->input,
							   d_stack_start,
							   d_stack_end);
			printf("Parsed double: %f [%d, %d]\n",
			       current_el.value, d_stack_start,
			       d_stack_end);
		}
	}

	if (l_br_count != r_br_count)
		return (r_br_count > l_br_count ?
				ENOLEFTBRACK : ENORIGHTBRACK);

	return 0;
}

static int start(const char *msg)
{
	int ret;
	char *error_msg;
	struct ctx _ctx;
	struct ctx *ctx = &_ctx;
	int msg_size = strlen(msg);

	memcpy(ctx->input, msg, MIN(msg_size, ARRAY_SIZE(ctx->input)));
	ctx->size = strlen(ctx->input) - 1;
	ret = ctx_process(ctx);
	if (ret > 0) {
		printf("[ERROR]: Failed to process on '%d'\n",
		       ret - 1);

		error_msg = calloc(1, ret);
		if (!error_msg) {
			printf("[CRIT]: Failed to allocate error msg\n");
			exit(1);
		}

		for (int i = 0; i < ctx->size - 1; i++)
			error_msg[i] = '_';
		error_msg[ret - 1] = '^';

		printf("Error was occured here:\n");
		printf("%s\n", ctx->input);
		printf("%s\n", error_msg);

		free(error_msg);
	} else if (ret < 0) {
		switch (ret) {
		case ENOLEFTBRACK:
			printf("[ERROR]: Failed to parse, "
			       "not enouph open bracket\n");
			break;
		case ENORIGHTBRACK:
			printf("[ERROR]: Failed to parse, "
			       "not enouph close bracket\n");
			break;
		default:
			break;
		}
	}

	return ret;
}

int main(void)
{
	int ret;
	int test_failed = 0;

	printf("\n[TEST1]: .2 + (2.2 + (2. * .1))\n\n");
	ret = start(".2 + 2.2 + 2.");
	if (ret) {
		printf("TEST1 Failed: err = %d\n", ret);
		test_failed++;
	}

	printf("\n[TEST2]: .2 + 2.2 + (2.\n\n");
	ret = start(".2 + 2.2 + (2.");
	if (ret != ENORIGHTBRACK) {
		printf("TEST2 Failed: err = %d\n", ret);
		test_failed++;
	}

	printf("\n[TEST3]: .2 + 2.2 + )2.\n\n");
	ret = start(".2 + 2.2 + )2.");
	if (ret != ENOLEFTBRACK) {
		printf("TEST3 Failed: err = %d\n", ret);
		test_failed++;
	}

	printf("\n[TEST4]: .2 + 2.2 + log2.\n\n");
	ret = start(".2 + 2.2 + log2.");
	if (ret == 0) {
		printf("TEST4 Failed: err = %d\n", ret);
		test_failed++;
	}

	printf("\nTest failed: %d -> %s\n",
	       test_failed, test_failed ? "FAILED" : "OK");
	// fgets(ctx->input, 4096, stdin);

	exit(0);
}
