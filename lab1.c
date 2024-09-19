#include "linux/kernel.h"
#include <assert.h>
#include <linux/list.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_FN_LEN 10

#define EPSILON	0.0000001

#define MIN(X, Y)			\
	({ __typeof__(X) _X = (X);	\
	__typeof__(Y) _Y = (Y);	\
	_X < _Y ? _X : _Y; })

#define OP_EQ '='
#define OP_AD '+'
#define OP_MN '-'
#define OP_DT '*'
#define OP_FR ':'
#define L_BR '('
#define R_BR ')'

#define EUNKNOWN	1
#define EFUNCNOFOUND	2
#define ENOLEFTBRACK	3
#define ENORIGHTBRACK	4
#define EDOUBLE		5
#define EWRONGCOMMA	6
#define EZERODIVIZION	7
#define EFUNCARGSINVAL	8


static const char ops[] = {
	OP_EQ, OP_AD, OP_MN,
	OP_DT, OP_FR,
	L_BR, R_BR
};

static double _log(double *args, int *err)
{
	if (args[0] == 0 || args[1] == 0) {
		*err = -EFUNCARGSINVAL;
		return 0;
	}
	return (double)log(args[0]) / log(args[1]);
}

static double _pow(double *args, int *err)
{
	return (double)pow(args[2], args[1]) + args[0];
}

struct func {
	char *name;

	double (*func)(double *args, int *err);

	unsigned int args_len;
	double *args;
};

static const struct func allowed_functions[] = {
	{ "log", _log, 2 },
	{ "pow", _pow, 3 },

};

enum element_type {
	EL_UNSPEC = 0,
	EL_OPERATOR,
	EL_DIGIT,
	EL_FUNCTION,

	EL_L_BRACK,
	EL_R_BRACK,
	EL_UN_MINUS,
	EL_COMMA,
	EL_SKIP,
};

enum op_type {
	OP_UNSPEC = 0,

	OP_ADD,
	OP_MINUS,

	OP_DOT,
	OP_FRAC,

	OP_FUNC,

	OP_UN_MINUS,

	OP_L_BR,
	OP_R_BR,
};

static const int op_prio[] = {
	[OP_UNSPEC] = 0,

	[OP_ADD] = 1,
	[OP_MINUS] = 1,

	[OP_DOT] = 2,
	[OP_FRAC] = 2,

	[OP_FUNC] = 3,

	[OP_UN_MINUS] = 4,

	[OP_L_BR] = 5,
	[OP_R_BR] = 5,
};

struct element {
	enum element_type type;

	double value;
	enum op_type op;

	struct func func;
};

struct _stack {
	struct list_head el;

	struct element *value;
};

struct stack {
	struct _stack output;
	struct list_head output_start;

	struct _stack ops;
	struct list_head ops_start;

	struct list_head *last_l_br;
};

struct ctx {
	char input[4096];
	unsigned int size;

	struct stack stack;

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

static enum op_type op_char2type(const char c)
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

static char op_type2char(enum op_type op)
{
	switch (op) {
	case OP_ADD:
		return OP_AD;
	case OP_UN_MINUS:
	case OP_MINUS:
		return OP_MN;
	case OP_FRAC:
		return OP_FR;
	case OP_DOT:
		return OP_DT;
	default:
		return 'u';
	}
}

static char *slice(const char *arr,
		   int start, int end)
{
	int size = end - start + 2;
	char *buf = calloc(size, sizeof(*buf));

	if (!buf) {
		printf("[CRIT]: Failed to allocate array");
		exit(1);
	}

	memcpy(buf, (char *)(arr + start), size - 1);
	return buf;
}

static int func_validate(const char *stack,
			 int start, int end,
			 struct func *func)
{
	int ret = -EFUNCNOFOUND;
	char *buf = slice(stack, start, end);

	for (int i = 0; i < ARRAY_SIZE(allowed_functions); i++) {
		if (str_equal(buf, allowed_functions[i].name)) {
			func->name = allowed_functions[i].name;
			func->func = allowed_functions[i].func;
			func->args_len = allowed_functions[i].args_len;
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

static int cmp_op(enum op_type op1, enum op_type op2)
{
	if (op1 == OP_L_BR)
		return 1;

	return op_prio[op2] - op_prio[op1];
}

static void stack_print(struct ctx *ctx)
{
	struct _stack *buf;

	printf("\n[STACK]\n");
	printf("Output: ");
	list_for_each_entry_reverse(buf, &ctx->stack.output_start, el) {
		switch (buf->value->type) {
		case EL_DIGIT:
			printf("%f ", buf->value->value);
			break;
		case EL_OPERATOR:
			printf("%c ", op_type2char(buf->value->op));
			break;
		case EL_FUNCTION:
			printf("%s ", buf->value->func.name);
			break;
		default:
			printf("'%d' ", buf->value->type);
			break;
		}
	}
	printf("\n");

	if (list_empty(&ctx->stack.ops_start))
		return;

	printf("Stack: ");
	list_for_each_entry_reverse(buf, &ctx->stack.ops_start, el) {
		switch (buf->value->type) {
		case EL_L_BRACK:
			printf("( ");
			break;
		case EL_OPERATOR:
			printf("%c ", op_type2char(buf->value->op));
			break;
		case EL_FUNCTION:
			printf("%s ", buf->value->func.name);
			break;
		default:
			break;
		}
	}
	printf("\n");
}

static int stack_insert(struct ctx *ctx, struct element *el)
{
	int ret = 0;
	struct _stack *stack_el;
	struct _stack *buf, *buf_next;

	stack_el = calloc(1, sizeof(*stack_el));
	if (!stack_el) {
		printf("[CRIT]: Failed to allocate stack el\n");
		exit(1);
	}

	stack_el->value = el;
	switch (el->type) {
	case EL_DIGIT:
		printf("Adding digit '%f' in output stack\n",
		       el->value);

		list_add(&stack_el->el, &ctx->stack.output_start);
		break;
	case EL_UN_MINUS:
	case EL_FUNCTION:
	case EL_OPERATOR:
		struct _stack *last_op;

		if (el->type == EL_FUNCTION)
			printf("Adding func '%s' in stack\n",
			       el->func.name);
		else if (el->type == EL_OPERATOR)
			printf("Adding op '%c' in stack\n",
			       op_type2char(el->op));

		last_op = list_first_entry_or_null(&ctx->stack.ops_start,
						   struct _stack, el);
		if (!last_op || cmp_op(last_op->value->op, el->op) > 0) {
			list_add(&stack_el->el, &ctx->stack.ops_start);
		} else {
			list_del(&last_op->el);
			list_add(&last_op->el, &ctx->stack.output_start);
			list_add(&stack_el->el, &ctx->stack.ops_start);
		}
		break;
	case EL_L_BRACK:
		list_add(&stack_el->el, &ctx->stack.ops_start);
		break;
	case EL_COMMA:
	case EL_R_BRACK:
		list_for_each_entry_safe(buf, buf_next,
					 &ctx->stack.ops_start, el) {
			if (buf->value->type == EL_L_BRACK) {
				if (el->type == EL_R_BRACK)
					list_del(&buf->el);
				break;
			}

			list_del(&buf->el);
			list_add(&buf->el, &ctx->stack.output_start);
		}
		fallthrough;
	default:
		free(stack_el);
		ret = 1;
		break;
	}
	stack_print(ctx);
	return ret;
}

static double *get_func_args(struct ctx *ctx, struct _stack *func)
{
	double *args;
	struct _stack *buf, *buf_next;
	unsigned int i = 0;
	unsigned int args_len = func->value->func.args_len;

	args = calloc(args_len, sizeof(double));
	if (!args) {
		printf("[CRIT]: Failed to allocate args\n");
		exit(1);
	}

	list_for_each_entry_safe(buf, buf_next,
				 &func->el, el) {
		if (i == args_len)
			break;

		args[i] = buf->value->value;
		list_del(&buf->el);
		i++;
	}

	return args;
}

static int stack_calc(struct ctx *ctx)
{
	int ret = 0;
	struct _stack *buf, *buf_next;
	struct _stack *buf_d1, *buf_d2;
	double res = 0, *args;

	list_for_each_entry_safe_reverse(buf, buf_next,
					 &ctx->stack.output_start, el) {
		if (buf->value->type != EL_OPERATOR &&
		    buf->value->type != EL_FUNCTION &&
		    buf->value->type != EL_UN_MINUS)
			continue;

		switch (buf->value->type) {
		case EL_OPERATOR:
			buf_d1 = list_next_entry(buf, el);
			buf_d2 = list_next_entry(buf_d1, el);
			assert(buf_d1->value->type == EL_DIGIT);
			assert(buf_d2->value->type == EL_DIGIT);

			list_del(&buf_d2->el);
			list_del(&buf_d1->el);
			break;
		case EL_FUNCTION:
			args = get_func_args(ctx, buf);
			assert(args);
			break;
		case EL_UN_MINUS:
			buf_d1 = list_next_entry(buf, el);
			assert(buf_d1->value->type == EL_DIGIT);

			list_del(&buf_d1->el);
			break;
		default:
			assert(0);
		}

		switch (buf->value->op) {
		case OP_ADD:
			res = buf_d2->value->value + buf_d1->value->value;
			break;
		case OP_MINUS:
			res = buf_d2->value->value - buf_d1->value->value;
			break;
		case OP_DOT:
			res = buf_d2->value->value * buf_d1->value->value;
			break;
		case OP_FRAC:
			if (!buf_d1->value->value)
				return -EZERODIVIZION;

			res = (double)
				buf_d2->value->value / buf_d1->value->value;
			break;
		case OP_FUNC:
			res = buf->value->func.func(args, &ret);
			if (ret != 0)
				return ret;
			break;
		case OP_UN_MINUS:
			res = -buf_d1->value->value;
			break;
		default:
			assert(0);
			break;
		}

		buf->value->value = res;
		buf->value->type = EL_DIGIT;
		buf->value->op = OP_UNSPEC;

		stack_print(ctx);
	}

	return 0;
}

static double ctx_calc(struct ctx *ctx, int *err)
{
	int ret;
	struct _stack *res;

	ret = stack_calc(ctx);
	if (ret < 0) {
		*err = ret;
		return ret;
	}

	res = list_first_entry_or_null(&ctx->stack.output_start,
				       struct _stack, el);
	if (!res->value)
		*err = -1;
	return res->value->value;
}

static void stack_collect(struct ctx *ctx)
{
	struct _stack *buf, *buf_next;

	list_for_each_entry_safe(buf, buf_next,
				 &ctx->stack.ops_start, el) {
		list_del(&buf->el);
		list_add(&buf->el, &ctx->stack.output_start);
	}
}

static void stack_free(struct ctx *ctx)
{
	struct _stack *buf, *buf_next;

	list_for_each_entry_safe(buf, buf_next, &ctx->stack.output_start, el) {
		free(buf->value);
		free(buf);
	}
}

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
		el->type = EL_DIGIT;
		el->value = double_validate(ctx->input,
					    stack_start,
					    stack_end);
		printf("Parsed double: %f [%d, %d]\n",
		       el->value, stack_start,
		       stack_end);
		return stack_end;
	}
	stack_end = ctx->size - 1;
	el->type = EL_DIGIT;
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

	for (int i = stack_start; i < ctx->size; i++) {
		char sym = ctx->input[i];

		if (isalpha(sym) || isdigit(sym))
			continue;

		if (sym == L_BR) {
			stack_end = i - 1;

			el->op = OP_FUNC;
			el->type = EL_FUNCTION;
			ret = func_validate(ctx->input, stack_start,
					    stack_end, &el->func);
			if (ret < 0)
				return ret;

			printf("Parsed function: %s [%d, %d]\n",
			       el->func.name, stack_start,
			       stack_end);
			return stack_end;
		}

		return func_validate(ctx->input, stack_start,
					    i, &el->func);
	}

	return ctx->size - 1;
}

static enum element_type get_sym_type(char sym)
{
	if (isdigit(sym) || sym == '.')
		return EL_DIGIT;
	if (isalpha(sym))
		return EL_FUNCTION;
	if (op_char2type(sym))
		return EL_OPERATOR;
	if (sym == L_BR)
		return EL_L_BRACK;
	if (sym == R_BR)
		return EL_R_BRACK;
	if (sym == ',')
		return EL_COMMA;
	if (other_validate(sym))
		return EL_SKIP;
	return EL_UNSPEC;
}

static int ctx_process(struct ctx *ctx)
{
	int ret, buf;
	struct element *el;
	int br_deep = 0, fn_br_deep = 0;
	int l_br_count = 0, r_br_count = 0;
	int comma_count = 0, fn_comma_count = 0;
	enum element_type stack_state = EL_UNSPEC;
	enum element_type last_stack_state = stack_state;

	for (int i = 0; i < ctx->size; i++) {
		el = calloc(1, sizeof(*el));
		if (!el) {
			printf("[CRIT]: Failed to allocate el");
			exit(1);
		}

		char sym = ctx->input[i];

		stack_state = get_sym_type(sym);
		switch (stack_state) {
		case EL_DIGIT:
			i = scan_digit(ctx, i, el, &ret);
			if (ret < 0) {
				printf("[ERROR]: Failed to parse function\n");
				ctx->index_err = ret;
				return ret;
			}

			if (stack_insert(ctx, el))
				free(el);
			break;
		case EL_FUNCTION:
			ret = scan_func(ctx, i, el);
			if (ret < 0) {
				printf("[ERROR]: Failed to parse function\n");
				ctx->index_err = i;
				return ret;
			}

			i = ret;
			fn_comma_count += el->func.args_len - 1;
			if (stack_insert(ctx, el))
				free(el);
			break;
		case EL_OPERATOR:
			buf = op_char2type(sym);
			el->type = EL_OPERATOR;
			el->op = buf;

			if ((last_stack_state == EL_UNSPEC ||
			     last_stack_state == EL_L_BRACK ||
			     last_stack_state == EL_OPERATOR ||
			     last_stack_state == EL_COMMA) &&
			    el->op == OP_MINUS) {
				el->op = OP_UN_MINUS;
				el->type = EL_UN_MINUS;
				last_stack_state = EL_UN_MINUS;
			} else if ((last_stack_state == EL_UNSPEC ||
				    last_stack_state == EL_L_BRACK ||
				    last_stack_state == EL_OPERATOR ||
				    last_stack_state == EL_COMMA) &&
				   el->op != OP_MINUS) {
				ctx->index_err = i;
				return -EDOUBLE;
			}

			if (stack_insert(ctx, el))
				free(el);
			printf("Parsed op: %d\n", el->op);
			break;
		case EL_L_BRACK:
			l_br_count++;

			el->op = OP_L_BR;
			el->type = EL_L_BRACK;

			if (stack_insert(ctx, el))
				free(el);
			break;
		case EL_R_BRACK:
			r_br_count++;
			if (r_br_count > l_br_count) {
				ctx->index_err = i;
				return -ENOLEFTBRACK;
			}

			el->type = EL_R_BRACK;

			if (stack_insert(ctx, el))
				free(el);
			break;
		case EL_COMMA:
			comma_count++;
			el->type = EL_COMMA;

			if (stack_insert(ctx, el))
				free(el);
			break;
		case EL_SKIP:
			free(el);
			continue;
		default:
			free(el);
			printf("[ERROR]: Unexpected stack: %d\n", stack_state);
			ctx->index_err = i;
			return -EUNKNOWN;
		}

		if (stack_state == last_stack_state) {
			switch (stack_state) {
			case EL_SKIP:
				break;
			case EL_L_BRACK:
				break;
			case EL_R_BRACK:
				break;
			default:
				printf("[ERROR]: Unexpected double of %d\n",
				       stack_state);
				ctx->index_err = i;
				return -EDOUBLE;
			}
		} else if (stack_state == EL_R_BRACK &&
			   last_stack_state == EL_L_BRACK) {
			ctx->index_err = i;
			return -EUNKNOWN;
		}
		last_stack_state = stack_state;
	}

	if (l_br_count > r_br_count) {
		ctx->index_err = ctx->size - 1;
		return -ENORIGHTBRACK;
	}

	if (comma_count != fn_comma_count) {
		ctx->index_err = ctx->size - 1;
		return -EWRONGCOMMA;
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

static int start(const char *msg, double ans)
{
	int ret;
	double test;
	struct ctx _ctx;
	struct ctx *ctx = &_ctx;
	int msg_size = strlen(msg);

	INIT_LIST_HEAD(&ctx->stack.output_start);
	INIT_LIST_HEAD(&ctx->stack.ops_start);

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
		goto on_exit;
	}

	stack_collect(ctx);
	stack_print(ctx);
	test = ctx_calc(ctx, &ret);
	if (ret < 0) {
		switch (-ret) {
		case EFUNCARGSINVAL:
			printf("[ERROR]: Failed to calc, "
			       "func invalid args\n");
			break;
		case EZERODIVIZION:
			printf("[ERROR]: Failed to calc, "
			       "zero division\n");
			break;
		default:
			break;
		}
		goto on_exit;
	}
	printf("\nResult: %f\n", test);
	if (test - ans > EPSILON) {
		printf("[ERROR]: Wrong answer: %f != %f\n",
		       test, ans);
		return -EUNKNOWN;
	}

on_exit:
	stack_free(ctx);
	return ret;
}

int main(void)
{
	int ret;
	char *buf;
	int test_failed = 0;

	// simplify: pow(-5, -2, -1)
	buf = "pow(2 + log(2, log(2, 4)) - 4 * (log(1 + 7, 16 * log(log(2, 4), 16))), -2, -1 - 0 * (2 - 1))";
	printf("\n[TEST1]: %s\n\n", buf);
	ret = start(buf, -0.96);
	if (ret) {
		printf("TEST1 Failed: err = %d\n", ret);
		test_failed++;
	}

	buf = "-(1 + (-2)) * -3 + 2";
	printf("\n[TEST1]: %s\n\n", buf);
	ret = start(buf, -1);
	if (ret) {
		printf("TEST1 Failed: err = %d\n", ret);
		test_failed++;
	}

	buf = "(.2 + 2.2) * (1 - 2) : (2. * .1)";
	printf("\n[TEST1]: %s\n\n", buf);
	ret = start(buf, -12);
	if (ret) {
		printf("TEST1 Failed: err = %d\n", ret);
		test_failed++;
	}

	buf = ".2 + (2.2 +* (2. * .1))";
	printf("\n[TEST1_1]: %s\n\n", buf);
	ret = start(buf, 0);
	if (ret != -EDOUBLE) {
		printf("TEST1_1 Failed: err = %d\n", ret);
		test_failed++;
	}

	buf = ".2 + (2.2 + (2. 2. * .1))";
	printf("\n[TEST1_2]: %s\n\n", buf);
	ret = start(buf, 0);
	if (ret != -EDOUBLE) {
		printf("TEST1_2 Failed: err = %d\n", ret);
		test_failed++;
	}

	buf = ".2 + 2.2 + (2.";
	printf("\n[TEST2]: %s\n\n", buf);
	ret = start(buf, 0);
	if (ret != -ENORIGHTBRACK) {
		printf("TEST2 Failed: err = %d\n", ret);
		test_failed++;
	}

	buf = ".2 + 2.2 + )2.";
	printf("\n[TEST3]: %s\n\n", buf);
	ret = start(buf, 0);
	if (ret != -ENOLEFTBRACK) {
		printf("TEST3 Failed: err = %d\n", ret);
		test_failed++;
	}

	buf = ".2 + 2.2 + log2.";
	printf("\n[TEST4]: %s\n\n", buf);
	ret = start(buf, 0);
	if (ret == 0) {
		printf("TEST4 Failed: err = %d\n", ret);
		test_failed++;
	}

	buf = ".2 + 2.2 + log(2.)";
	printf("\n[TEST5]: %s\n\n", buf);
	ret = start(buf, 0);
	if (ret != -EWRONGCOMMA) {
		printf("TEST5 Failed: err = %d\n", ret);
		test_failed++;
	}

	printf("\nTest failed: %d -> %s\n",
	       test_failed, test_failed ? "FAILED" : "OK");
	// fgets(ctx->input, 4096, stdin);

	exit(0);
}
