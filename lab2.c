#include "linux/kernel.h"
#include "linux/types.h"
#include <stdbool.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <linux/hashtable.h>

#define HASH_TABLE	head
#define NDA_BUF_SIZE	128


#define for_each_alphabet	for (uint16_t i = 0; i < a_counter; i++)

static char alphabet[512] = {};
static uint16_t a_counter = 0;

static inline void err_no_mem(void)
{
	printf("[CRIT]: failed to allocate\n");
	exit(ENOMEM);
}

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

struct hpoint {
	char name[128];
	uint32_t key;

	struct hlist_node node_point;
	DECLARE_HASHTABLE(head_jump, 10);

	bool dfp;
};

struct hjump {
	uint32_t key;
	char value;
	struct hlist_node node_jump;
	struct hpoint *hpoint;
};

struct func_matrix {
	DECLARE_HASHTABLE(head, 10);
};

struct ctx {
	FILE *file;

	struct func_matrix nfa;
	struct func_matrix dfa;
};

enum parsing_state {
	PARSED_UNSPEC = 0,
	PARSED_POINT,
	PARSING_VALUE,
	PARSED_VALUE,
};

struct parsed_line {
	char *point_from;
	char *point_to;

	char value;
};

static int scan_str(const char *str,
		    struct parsed_line *line)
{
	unsigned int i = 0, b_i = 0;
	enum parsing_state state = PARSED_UNSPEC;
	char buf[NDA_BUF_SIZE] = {};

	for (; i < strlen(str); i++) {
		char c = str[i];

		if (state == PARSED_POINT &&
		    (c == ',' || c == '=')) {
			buf[b_i] = c;
			b_i++;
			state = PARSING_VALUE;
			continue;
		}

		switch (c) {
		case ',':
			if (state) {
				printf("[ERROR]: Unexpected "
				       "','\n");
				return -i;
			}

			line->point_from = strdup(buf);
			if (!line->point_from)
				err_no_mem();
			state = PARSED_POINT;
			b_i = 0;
			memset(buf, 0, ARRAY_SIZE(buf));

			if (i + 3 >= strlen(str)) {
				printf("[ERROR]: Expected more "
				       "symbols\n");
			}
			line->value = str[i + 1];
			i = i + 2;
			state = PARSED_VALUE;
			break;
		case '=':
			printf("[ERROR]: Unexpected"
				"'='\n");
			return -i;
		case '\r':
		case '\n':
			break;
		default:
			buf[b_i] = c;
			b_i++;
			break;
		}
	}

	line->point_to = strdup(buf);
	if (!line->point_to)
		err_no_mem();

	printf("Parsed line:\n"
		"\tp_from: %s\n"
		"\tp_val: %c\n"
		"\tp_to: %s\n\n",
		line->point_from,
		line->value,
		line->point_to);

	return 0;
}

#define DEFAULT_POINT_SIZE	100
#define REALLOCATE_STEP		2

void ctx_destroy(struct ctx *ctx)
{
	if (ctx->file)
		fclose(ctx->file);
}

void func_graph_alloc(struct func_matrix *matrix)
{

}

static uint32_t name_hash(const char *name)
{
	uint32_t key = 0;
	uint8_t len = strlen(name);

	for (uint8_t i = 0; i < len; i++)
		key += (char)name[i] * 2;

	printf("[HASH]: '%s' -> '%d'\n",
	       name, key);
	return key;
}

void ctx_alloc(struct ctx *ctx, unsigned int points)
{
	func_graph_alloc(&ctx->nfa);
	func_graph_alloc(&ctx->dfa);
}

static struct hpoint *point_alloc(const char *name)
{
	struct hpoint *p;

	p = calloc(1, sizeof(*p));
	if (!p)
		err_no_mem();

	memcpy(p->name, name, ARRAY_SIZE(p->name));
	p->key = name_hash(p->name);
	return p;
}

static struct hpoint *point_find(struct func_matrix *matrix,
				 const char *name)
{
	struct hpoint *res;
	uint32_t key = name_hash(name);

	hash_for_each_possible(matrix->head, res, node_point, key) {
		if (str_equal(name, res->name)) {
			printf("Found point '%s'\n", name);
			return res;
		}
	}

	return NULL;
}

static struct hpoint *point_find_or_create(struct func_matrix *matrix,
					   const char *name)
{
	struct hpoint *res;

	res = point_find(matrix, name);
	if (res)
		return res;

	res = point_alloc(name);
	hash_add(matrix->head, &res->node_point, res->key);
	return res;
}

static struct hjump *jump_create(const char c)
{
	struct hjump *res;

	res = calloc(1, sizeof(*res));
	if (!res)
		err_no_mem();

	res->value = c;
	res->key = c;
	return res;
}

static void alphabet_put(const char c)
{
	for (uint16_t i = 0; i < a_counter; i++) {
		if (alphabet[i] == c)
			return;
	}

	if (a_counter == ARRAY_SIZE(alphabet))
		err_no_mem();

	alphabet[a_counter] = c;
	a_counter++;
}

int ctx_fill_nda_from_file(struct ctx *ctx)
{
	assert(ctx->file);
	int ret;
	struct hpoint *p1, *p2;
	struct hjump *j;
	struct parsed_line line;
	char buf[NDA_BUF_SIZE] = {};
	char *buf_ptr;

	buf_ptr = fgets(buf, NDA_BUF_SIZE, ctx->file);
	while (buf_ptr) {
		if (!buf_ptr || buf_ptr[0] == '\r') {
			printf("File was ended\n");
			break;
		}

		printf("line: %s", buf_ptr);

		ret = scan_str(buf_ptr, &line);
		if (ret < 0) {
			printf("[ERROR]: Failed to parse line\n");
			return ret;
		}
		p1 = point_find_or_create(&ctx->nfa, line.point_from);
		j = jump_create(line.value);
		alphabet_put(j->value);
		p2 = point_find_or_create(&ctx->nfa, line.point_to);

		j->hpoint = p2;
		hash_add(p1->head_jump, &j->node_jump, j->key);

		buf_ptr = fgets(buf, NDA_BUF_SIZE, ctx->file);
	}

	return 0;
}

static struct hpoint *ctx_concat_points(struct ctx *ctx,
					struct hpoint *p,
					char key,
					char *new_point_name)
{
	struct hpoint *new_point, *p1, *p2;
	struct hjump *j, *j2, *j3;
	struct hlist_node *h_tmp;
	uint32_t c_l = 0;
	uint32_t bkt;
	bool point_created = false;
	bool need_continue = false;
	DECLARE_HASHTABLE(set, 10);

	hash_init(set);
	hash_for_each_possible(p->head_jump,
			       j, node_jump, key) {
		printf("p: '%s'\n", p->name);
		memcpy((char *)(new_point_name + c_l),
		       j->hpoint->name,
		       strlen(j->hpoint->name));
		c_l = strlen(new_point_name);
		new_point_name[c_l] = '-';
		c_l++;

		hash_for_each(j->hpoint->head_jump,
			      bkt, j2, node_jump) {
			j3 = jump_create(j2->key);
			j3->hpoint = j2->hpoint;
			hash_add(set, &j3->node_jump, j3->key);
		}
	}
	new_point_name[c_l - 1] = '\0';
	printf("p2: '%s'\n", new_point_name);

	new_point = point_find(&ctx->nfa, new_point_name);
	if (!new_point) {
		point_created = true;
		new_point = point_alloc(new_point_name);
		hash_add(ctx->nfa.head, &new_point->node_point, new_point->key);
	}

	hash_for_each_possible_safe(p->head_jump, j,
				    h_tmp, node_jump, key) {
		hash_del(&j->node_jump);
	}

	j = jump_create(key);
	j->hpoint = new_point;
	hash_add(p->head_jump, &j->node_jump, key);

	if (!point_created)
		return new_point;

	bkt = 0;
	hash_for_each(set, bkt, j2, node_jump) {
		char c = j2->key;

		need_continue = false;
		hash_for_each_possible(new_point->head_jump, j3, node_jump, c) {
			if (j3->value == j2->value &&
			    j2->hpoint == j3->hpoint)
				need_continue = true;
		}

		if (need_continue)
			continue;

		j = jump_create(c);
		memcpy(j, j2, sizeof(*j2));
		hash_add(new_point->head_jump, &j->node_jump, c);
	}

	return new_point;
}

static bool validate_point(struct hpoint *p,
			   char c,
			   uint32_t *j_len,
			   uint32_t *s_len)
{
	struct hjump *j;

	hash_for_each_possible(p->head_jump,
			       j, node_jump, c) {
		*j_len += 1;
		*s_len += strlen(j->hpoint->name);
	}

	if (*j_len <= 1) {
		p->dfp = true;
		return true;
	}

	printf("Point '%s' is NFP: %d\n",
	       p->name, *j_len);
	p->dfp = false;
	return false;
}

static int ctx_calc_dfa(struct ctx *ctx)
{
	uint32_t bkt;
	uint32_t j_len = 0, s_len = 0;
	struct hpoint *p1, *p2, *new_point;
	struct hjump *j;
	char *new_point_name;
	bool valid = false;
	bool begin = true;
	bool nfp_exists = false;

	while (begin || nfp_exists) {
		begin = false;
		nfp_exists = false;

		hash_for_each(ctx->nfa.head, bkt, p1, node_point) {
			if (p1->dfp) {
				printf("Skip %s\n", p1->name);
				continue;
			}

			for (uint16_t i = 0; i < a_counter; i++) {
				char c = alphabet[i];
				uint32_t c_l = 0;

				j_len = 0;
				s_len = 0;
				valid = validate_point(p1, c, &j_len, &s_len);
				if (valid)
					continue;
				else
					nfp_exists = true;

				new_point_name = calloc(j_len + s_len,
							sizeof(char));
				if (!new_point_name)
					err_no_mem();

				new_point = ctx_concat_points(ctx, p1, c,
							      new_point_name);
				j_len = 0;
				s_len = 0;
				valid = validate_point(new_point, c,
						       &j_len, &s_len);
				printf("new point is %d\n", valid);
				if (!valid)
					nfp_exists = true;

				free(new_point_name);
			}
		}

		printf("valid: %d\n", nfp_exists);
	}

	return 0;
}

static void matrix_print(struct func_matrix *matrix)
{
	uint32_t bkt;
	struct hpoint *p1, *p2;
	struct hjump *j;

	printf("[TABLE]:\n");
	printf("\t");
	for (uint16_t i = 0; i < a_counter; i++)
		printf("'%c'\t\t", alphabet[i]);
	printf("\n");

	hash_for_each(matrix->head, bkt, p1, node_point) {
		printf("%s\t", p1->name);
		for (uint16_t i = 0; i < a_counter; i++) {
			char c = alphabet[i];

			hash_for_each_possible(p1->head_jump,
					       j, node_jump, c) {
				printf("'%s',", j->hpoint->name);
			}
			printf("\t\t");
		}
		printf("\n");
	}
}

int main(int argc, char **argv)
{
	struct ctx _ctx;
	struct ctx *ctx = &_ctx;
	char *filename;

	if (argc == 1) {
		printf("Using default file 'lab2.txt'\n");
		filename = "lab2.txt";
	} else {
		printf("Using file '%s'\n", argv[1]);
		filename = argv[1];
	}

	ctx->file = fopen(filename, "r");
	if (!ctx->file ||
	    access(filename, F_OK) ||
	    access(filename, R_OK)) {
		printf("Cannot open file '%s',"
		       "err = %s(%d)\n", filename,
		       strerror(errno), errno);
		exit(errno);
	}

	ctx_alloc(ctx, DEFAULT_POINT_SIZE);
	ctx_fill_nda_from_file(ctx);
	matrix_print(&ctx->nfa);
	ctx_calc_dfa(ctx);
	matrix_print(&ctx->nfa);

	ctx_destroy(ctx);
	return 0;
}
