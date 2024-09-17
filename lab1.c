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

#define EUNKNOWN	1
#define EFUNCNOFOUND	2
#define ENOLEFTBRACK	3
#define ENORIGHTBRACK	4
#define EDOUBLE		5


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

	unsigned int args_len;
	double *args;
};

static const struct func allowed_functions[] = {
	{ "log", _log, 2 },
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

	unsigned int index_err;
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
	int ret = -EFUNCNOFOUND;
	char *buf = slice(stack, start, end);

	for (int i = 0; i < 0; i++) {
		if (str_equal(buf, allowed_functions[i].name)) {
			func->name = allowed_functions[i].name;
			func->func = allowed_functions[i].func;
			ret = 0;
			break;
		}
	}

	if (ret)
		printf("[ERROR]: Unexpected function '%s'\n", buf);

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

	STACK_L_BR,
	STACK_R_BR,
	STACK_OP,
	STACK_SKIP,
};

static int scan_digit(struct ctx *ctx, int stack_start,
		      struct element *el, int *error)
{
	bool delim_met = false;
	int stack_end = stack_start;

	for (int i = stack_start; i < ctx->size; i++) {
		char sym = ctx->input[i];

		if ((isdigit(sym) ||
		     sym == '.'))  {
			if (sym == '.' && !delim_met) {
				delim_met = true;
			} else if (sym == '.' && delim_met) {
				*error = i;
				return -EUNKNOWN;
			}
			continue;
		}

		stack_end = i - 1;
		el->type = ELEMENT_DIGIT;
		el->value = double_validate(ctx->input,
					    stack_start,
					    stack_end);
		printf("Parsed double: %f [%d, %d]\n",
		       el->value, stack_start,
		       stack_end);
		return stack_end;
	}
	stack_end = ctx->size - 1;
	el->type = ELEMENT_DIGIT;
	el->value = double_validate(ctx->input,
				    stack_start,
				    stack_end);
	printf("Parsed double: %f [%d, %d]\n",
	       el->value, stack_start, stack_end);

	return ctx->size - 1;
}

static int scan_func(struct ctx *ctx, int stack_start,
		     struct element *el)
{
	int ret;
	int stack_end = stack_start;
	enum process_states stack_state = STACK_FUNC;

	for (int i = stack_start; i < ctx->size; i++) {
		char sym = ctx->input[i];

		if (isalpha(sym) || isdigit(sym))
			continue;

		if (sym == OP_L_BR &&
		    stack_state == STACK_FUNC) {
			stack_end = i - 1;

			stack_state = STACK_FUNC_ARGS;
			el->type = ELEMENT_FUNCTION;
			ret = func_validate(ctx->input, stack_start,
					    stack_end, el->func);
			if (ret < 0)
				return ret;

			printf("Parsed function: %s [%d, %d]\n",
			       el->func->name, stack_start,
			       stack_end);
			continue;
		} else if (sym == OP_R_BR &&
			   stack_state == STACK_FUNC_ARGS) {
			stack_state = STACK_UNSPEC;
			return i;
		} else if (sym != ',' && sym != '.' && !isdigit(sym)) {
			printf("[ERROR]: Unexpected symbol '%c'\n", sym);
			return -EFUNCNOFOUND;
		}

		return func_validate(ctx->input, stack_start,
					    stack_end, el->func);
	}

	return ctx->size - 1;
}

static enum process_states get_sym_type(char sym)
{
	if (isdigit(sym) || sym == '.')
		return STACK_DIGIT;
	if (isalpha(sym))
		return STACK_FUNC;
	if (op_validate(sym))
		return STACK_OP;
	if (sym == OP_L_BR)
		return STACK_L_BR;
	if (sym == OP_R_BR)
		return STACK_R_BR;
	if (other_validate(sym))
		return STACK_SKIP;
	return STACK_UNSPEC;
}

static int ctx_process(struct ctx *ctx)
{
	int ret, buf;
	struct element current_el;
	char fn_stack[MAX_FN_LEN] = {};
	int l_br_count = 0, r_br_count = 0;
	int d_stack_start = -1, d_stack_end = -1;
	enum process_states stack_state = STACK_UNSPEC;
	enum process_states last_stack_state = stack_state;

	for (int i = 0; i < ctx->size; i++) {
		char sym = ctx->input[i];

		stack_state = get_sym_type(sym);
		switch (stack_state) {
		case STACK_DIGIT:
			printf("[INFO]: Scanning digit...\n");
			i = scan_digit(ctx, i, &current_el, &ret);
			if (ret < 0) {
				printf("[ERROR]: Failed to parse function\n");
				ctx->index_err = ret;
				return ret;
			}
			break;
		case STACK_FUNC:
			printf("[INFO]: Scanning func...\n");
			ret = scan_func(ctx, i, &current_el);
			if (ret < 0) {
				printf("[ERROR]: Failed to parse function\n");
				ctx->index_err = i;
				return ret;
			}
			break;
		case STACK_OP:
			buf = op_validate(sym);
			current_el.type = ELEMENT_OPERATOR;
			current_el.op = buf;

			printf("Parsed op: %d\n", current_el.op);
			break;
		case STACK_L_BR:
			l_br_count++;
			break;
		case STACK_R_BR:
			r_br_count++;
			if (r_br_count > l_br_count) {
				ctx->index_err = i;
				return -ENOLEFTBRACK;
			}
			break;
		case STACK_SKIP:
			continue;
		default:
			ctx->index_err = i;
			return -EUNKNOWN;
		}

		if (stack_state == last_stack_state) {
			switch (stack_state) {
			case STACK_OP:
				if (sym == '-')
					break;
				ctx->index_err = i;
				return -EDOUBLE;
			case STACK_SKIP:
				break;
			case STACK_L_BR:
				break;
			case STACK_R_BR:
				break;
			default:
				ctx->index_err = i;
				return -EDOUBLE;
			}
		} else if (stack_state == STACK_R_BR &&
			   last_stack_state == STACK_L_BR) {
			ctx->index_err = i;
			return -EUNKNOWN;
		}
		last_stack_state = stack_state;
	}

	if (l_br_count > r_br_count) {
		ctx->index_err = ctx->size - 1;
		return -ENORIGHTBRACK;
	}

	return 0;
}

static void print_err(struct ctx *ctx)
{
	char *error_msg;

	error_msg = calloc(ctx->size, sizeof(char));
	if (!error_msg) {
		printf("[CRIT]: Failed to allocate error msg\n");
		exit(1);
	}

	for (int i = 0; i < ctx->size - 1; i++)
		error_msg[i] = '_';
	error_msg[ctx->index_err] = '^';

	printf("Error was occured here:\n");
	printf("%s\n", ctx->input);
	printf("%s\n", error_msg);

	free(error_msg);
}

static int start(const char *msg)
{
	int ret;
	struct ctx _ctx;
	struct ctx *ctx = &_ctx;
	int msg_size = strlen(msg);

	memset(ctx->input, 0, ARRAY_SIZE(ctx->input));
	memcpy(ctx->input, msg, MIN(msg_size, ARRAY_SIZE(ctx->input)));
	ctx->size = strlen(ctx->input);
	ret = ctx_process(ctx);
	if (ret < 0) {
		switch (-ret) {
		case EFUNCNOFOUND:
			printf("[ERROR]: Failed to parse, "
			       "no such function found\n");
			break;
		case ENOLEFTBRACK:
			printf("[ERROR]: Failed to parse, "
			       "not enouph open bracket\n");
			break;
		case ENORIGHTBRACK:
			printf("[ERROR]: Failed to parse, "
			       "not enouph close bracket\n");
			break;
		case EDOUBLE:
			printf("[ERROR]: Failed to parse, "
			       "double elements found\n");
			break;
		default:
			break;
		}
		print_err(ctx);
	}

	return ret;
}

int main(void)
{
	int ret;
	char *buf;
	int test_failed = 0;

	buf = ".2 + (2.2 +- (2. * .1))";
	printf("\n[TEST1]: %s\n\n", buf);
	ret = start(buf);
	if (ret) {
		printf("TEST1 Failed: err = %d\n", ret);
		test_failed++;
	}

	buf = ".2 + (2.2 +* (2. * .1))";
	printf("\n[TEST1_1]: %s\n\n", buf);
	ret = start(buf);
	if (ret != -EDOUBLE) {
		printf("TEST1_1 Failed: err = %d\n", ret);
		test_failed++;
	}

	buf = ".2 + (2.2 + (2. 2. * .1))";
	printf("\n[TEST1_2]: %s\n\n", buf);
	ret = start(buf);
	if (ret != -EDOUBLE) {
		printf("TEST1_2 Failed: err = %d\n", ret);
		test_failed++;
	}

	buf = ".2 + 2.2 + (2.";
	printf("\n[TEST2]: %s\n\n", buf);
	ret = start(buf);
	if (ret != -ENORIGHTBRACK) {
		printf("TEST2 Failed: err = %d\n", ret);
		test_failed++;
	}

	buf = ".2 + 2.2 + )2.";
	printf("\n[TEST3]: %s\n\n", buf);
	ret = start(buf);
	if (ret != -ENOLEFTBRACK) {
		printf("TEST3 Failed: err = %d\n", ret);
		test_failed++;
	}

	buf = ".2 + 2.2 + log2.";
	printf("\n[TEST4]: %s\n\n", buf);
	ret = start(buf);
	if (ret == 0) {
		printf("TEST4 Failed: err = %d\n", ret);
		test_failed++;
	}

	buf = ".2 + 2.2 + log(2.)";
	printf("\n[TEST5]: %s\n\n", buf);
	ret = start(buf);
	if (ret) {
		printf("TEST5 Failed: err = %d\n", ret);
		test_failed++;
	}

	printf("\nTest failed: %d -> %s\n",
	       test_failed, test_failed ? "FAILED" : "OK");
	// fgets(ctx->input, 4096, stdin);

	exit(0);
}
