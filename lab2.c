#include "linux/kernel.h"
#include "linux/list.h"
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

struct func_matrix {
	DECLARE_HASHTABLE(head, 10);
};

static void matrix_print(struct func_matrix *matrix);

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
	bool start;
};

struct hjump {
	uint32_t key;
	char value;
	struct hlist_node node_jump;
	struct hpoint *hpoint;
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

static uint32_t name_hash(const char *name)
{
	uint32_t key = 0;
	uint8_t len = strlen(name);

	for (uint8_t i = 0; i < len; i++)
		key += (char)name[i] * 2;

	return key;
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

static int fill_point(struct ctx *ctx, const char *buf,
		      bool begin)
{
	if (!buf || buf[0] == '\r') {
		printf("File was ended\n");
		return -1;
	}

	int ret;
	struct hpoint *p1, *p2;
	struct hjump *j;
	struct parsed_line line;

	ret = scan_str(buf, &line);
	if (ret < 0) {
		printf("[ERROR]: Failed to parse line\n");
		return ret;
	}
	p1 = point_find_or_create(&ctx->nfa, line.point_from);
	p1->start |= begin;
	j = jump_create(line.value);
	alphabet_put(j->value);
	p2 = point_find_or_create(&ctx->nfa, line.point_to);

	j->hpoint = p2;
	hash_add(p1->head_jump, &j->node_jump, j->key);

	return 0;
}

int ctx_fill_nda_from_file(struct ctx *ctx)
{
	assert(ctx->file);
	int ret;
	char buf[NDA_BUF_SIZE] = {};
	char *buf_ptr;

	buf_ptr = fgets(buf, NDA_BUF_SIZE, ctx->file);
	printf("line: %s", buf_ptr);
	ret = fill_point(ctx, buf_ptr, true);
	if (ret < 0)
		return ret;

	while (buf_ptr) {
		buf_ptr = fgets(buf, NDA_BUF_SIZE, ctx->file);

		printf("line: %s", buf_ptr);
		ret = fill_point(ctx, buf_ptr, false);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static struct hpoint *ctx_concat_points(struct ctx *ctx,
					struct hpoint *p,
					char key,
					char *new_point_name)
{
	struct hpoint *new_point;
	struct hjump *j, *j2, *j3;
	struct hlist_node *h_tmp;
	uint32_t c_l = 0;
	uint32_t bkt;
	bool point_created = false;
	bool need_continue = false;
	bool begin = false;
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
		begin |= j->hpoint->start;

		hash_for_each(j->hpoint->head_jump,
			      bkt, j2, node_jump) {
			j3 = jump_create(j2->key);
			j3->hpoint = j2->hpoint;
			hash_add(set, &j3->node_jump, j3->key);
		}
	}
	new_point_name[c_l - 1] = '\0';

	new_point = point_find(&ctx->nfa, new_point_name);
	if (!new_point) {
		point_created = true;
		new_point = point_alloc(new_point_name);
		hash_add(ctx->nfa.head, &new_point->node_point, new_point->key);
	}
	new_point->start = begin;

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
			   uint32_t *j_len,
			   uint32_t *s_len)
{
	char c;
	struct hjump *j;

	printf("P-name: %s\n", p->name);

	for_each_alphabet {
		c = alphabet[i];
		*j_len = 0;
		*s_len = 0;

		hash_for_each_possible(p->head_jump,
				       j, node_jump, c) {
			*j_len += 1;
			*s_len += strlen(j->hpoint->name);
			printf("P: %s, %c = %s\n",
			       p->name, j->value, j->hpoint->name);
		}

		if (*j_len > 1) {
			printf("Point '%s' is NFP: %d\n",
			       p->name, *j_len);
			p->dfp = false;
			return false;
		}
	}

	p->dfp = true;
	return true;
}

static bool ctx_dfa_validate(struct ctx *ctx,
			     const char *test)
{
	char c;
	uint32_t bkt;
	bool point_exists = false;
	struct hjump *j1, *jf;
	struct hpoint *p1, *p2, *pn;
	DECLARE_HASHTABLE(externs, 5);

	hash_init(externs);
	hash_for_each(ctx->nfa.head, bkt, p1, node_point) {
		if (!p1->start)
			continue;

		point_exists = false;
		hash_for_each_possible(externs, p2, node_point,
				       p1->key) {
			if (str_equal(p1->name, p2->name)) {
				point_exists = true;
				break;
			}
		}

		if (point_exists)
			continue;

		pn = point_alloc(p1->name);
		printf("Hash added : %s\n", pn->name);
		hash_add(externs, &pn->node_point, pn->key);
	}

	hash_for_each(externs, bkt, p1, node_point) {
		uint32_t i = 0, len = strlen(test);

		p2 = point_find(&ctx->nfa, p1->name);
		while (i < len && p2->name[0] != 'f') {
			c = test[i];
			jf = NULL;

			hash_for_each_possible(p2->head_jump, j1,
					       node_jump, c) {
				printf("val: %c\n", j1->value);
				jf = j1;
				break;
			}

			if (!jf) {
				printf("No jumps found for Q(%s, %c)\n",
				       p2->name, c);
				return false;
			}

			printf("Q(%s, %c) = %s\n",
			       p2->name, c, jf->hpoint->name);
			p2 = jf->hpoint;
			i++;
		}

		if (p2->name[0] == 'f')
			return true;
	}

	return false;
}

static void delete_useless_on_start(struct ctx *ctx)
{
	uint32_t bkt1, bkt2, bkt3;
	struct hpoint *p1, *p2, *pn;
	struct hlist_node *s1, *s2;
	struct hjump *j1;
	char *cmp_name;
	bool point_have_act = false;
	bool point_exists = false;
	bool useless_exists = true;
	DECLARE_HASHTABLE(useless, 10);

	hash_init(useless);
	while (useless_exists) {
		printf("[INFO]: Find useless loop...\n");
		useless_exists = false;

		hash_for_each_safe(ctx->nfa.head, bkt1, s1, p1, node_point) {
			cmp_name = p1->name;
			point_have_act = false;
			point_exists = false;
			hash_for_each_safe(p1->head_jump, bkt2, s2, j1,
					   node_jump) {
				if (!str_equal(cmp_name, j1->hpoint->name)) {
					printf("[INFO]: Point '%s' "
					       "not useless\n", cmp_name);
					point_have_act = true;
					break;
				}
			}

			if (point_have_act || p1->name[0] == 'f')
				continue;

			hash_for_each_possible(useless, p2, node_point,
					       p1->key) {
				if (str_equal(p2->name, p1->name)) {
					point_exists = true;
					break;
				}
			}

			if (point_exists)
				continue;

			pn = point_alloc(p1->name);

			hash_add(useless, &pn->node_point, pn->key);
			hash_del(&p1->node_point);

			printf("[INFO]: Point '%s' is useless\n",
			       p1->name);
			useless_exists = true;
		}

		hash_for_each_safe(useless, bkt1, s1, p1, node_point) {
			hash_for_each(ctx->nfa.head, bkt2, p2, node_point) {
				hash_for_each_safe(p2->head_jump, bkt3, s2,
						   j1, node_jump) {
					if (str_equal(p1->name,
						      j1->hpoint->name))
						hash_del(&j1->node_jump);
				}
			}
		}
	}

	printf("[INFO]: Deleting useless done\n");
}

static void delete_usless(struct ctx *ctx)
{
	uint32_t bkt1, bkt2;
	struct hpoint *p1, *p2, *pn, *jp;
	struct hlist_node *s1, *s2;
	struct hjump *j1;
	bool point_exists = false;
	DECLARE_HASHTABLE(head, 10);

	hash_for_each_safe(ctx->nfa.head, bkt1, s1, p1, node_point) {
		hash_for_each_safe(p1->head_jump, bkt2, s2, j1, node_jump) {
			point_exists = false;

			jp = j1->hpoint;

			hash_for_each_possible(head, p2, node_point, jp->key) {
				if (str_equal(p2->name, jp->name)) {
					point_exists = true;
					break;
				}
			}

			if (point_exists)
				continue;

			pn = point_alloc(jp->name);

			hash_add(head, &pn->node_point, pn->key);
		}

		if (p1->start) {
			printf("Add start: %s\n", p1->name);
			pn = point_alloc(p1->name);

			hash_add(head, &pn->node_point, pn->key);
		}
	}

	hash_for_each(ctx->nfa.head, bkt1, p1, node_point) {
		point_exists = false;

		hash_for_each_possible(head, p2, node_point, p1->key) {
			if (str_equal(p2->name, p1->name)) {
				point_exists = true;
				break;
			}
		}

		if (!point_exists)
			hash_del(&p1->node_point);
	}
}

static int ctx_calc_dfa(struct ctx *ctx)
{
	uint32_t bkt, bkt1;
	uint32_t j_len = 0, s_len = 0;
	struct hjump *j1;
	struct hpoint *p1, *new_point;
	char *new_point_name;
	bool valid = false;
	bool begin = true;
	bool nfp_exists = false;

	while (begin || nfp_exists) {
		begin = false;
		nfp_exists = false;

		hash_for_each(ctx->nfa.head, bkt, p1, node_point) {
			printf("[DFP CHECK]: %s\n", p1->name);
			if (p1->dfp) {
				printf("Skip %s\n", p1->name);
				continue;
			}

			hash_for_each(p1->head_jump, bkt1, j1, node_jump) {
				char c = j1->value;

				j_len = 0;
				s_len = 0;
				valid = validate_point(p1, &j_len, &s_len);
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
				valid = validate_point(new_point,
						       &j_len, &s_len);
				printf("%s: new point is %d\n",
				       new_point->name, valid);
				matrix_print(&ctx->nfa);
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
	struct hpoint *p1;
	struct hjump *j;

	printf("[TABLE]:\n");
	printf("\t");
	for (uint16_t i = 0; i < a_counter; i++)
		printf("'%c'\t\t", alphabet[i]);
	printf("\n");

	hash_for_each(matrix->head, bkt, p1, node_point) {
		if (p1->start)
			printf("->%s\t", p1->name);
		else
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

	ctx_fill_nda_from_file(ctx);
	matrix_print(&ctx->nfa);
	printf("[TABLE DELETED USELESS]\n");
	delete_useless_on_start(ctx);
	matrix_print(&ctx->nfa);
	ctx_calc_dfa(ctx);
	matrix_print(&ctx->nfa);
	delete_usless(ctx);
	matrix_print(&ctx->nfa);
	if (ctx_dfa_validate(ctx, "ahm"))
		printf("Valid for dfa\n");
	else
		printf("Not valid for dfa\n");

	ctx_destroy(ctx);
	return 0;
}
