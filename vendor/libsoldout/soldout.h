/* Amalgamation version of libsoldout. See https://github.com/faelys/libsoldout */

/*
 * Copyright (c) 2009, Natacha Porté
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef SOLDOUT_H
#define SOLDOUT_H




#ifndef LITHIUM_ARRAY_H
#define LITHIUM_ARRAY_H

#include <stdlib.h>




struct array {
	void*	base;
	int	size;
	int	asize;
	size_t	unit; };



struct parray {
	void **	item;
	int	size;
	int	asize; };



typedef int (*array_cmp_fn)(void *key, void *array_entry);





int
arr_adjust(struct array *);


void
arr_free(struct array *);


int
arr_grow(struct array *, int);


void
arr_init(struct array *, size_t);


int
arr_insert(struct array *, int nb, int n);


void *
arr_item(struct array *, int);


int
arr_newitem(struct array *);


void
arr_remove(struct array *, int);



void *
arr_sorted_find(struct array *, void *key, array_cmp_fn cmp);

int
arr_sorted_find_i(struct array *, void *key, array_cmp_fn cmp);




int
parr_adjust(struct parray *);


void
parr_free(struct parray *);


int
parr_grow(struct parray *, int);


void
parr_init(struct parray *);


int
parr_insert(struct parray *, int nb, int n);


void *
parr_pop(struct parray *);


int
parr_push(struct parray *, void *);


void *
parr_remove(struct parray *, int);


void *
parr_sorted_find(struct parray *, void *key, array_cmp_fn cmp);

int
parr_sorted_find_i(struct parray *, void *key, array_cmp_fn cmp);


void *
parr_top(struct parray *);


#endif





#ifndef LITHIUM_BUFFER_H
#define LITHIUM_BUFFER_H

#include <stdarg.h>
#include <stddef.h>




struct buf {
	char *	data;
	size_t	size;
	size_t	asize;
	size_t	unit;
	int	ref; };





#define CONST_BUF(name, string) \
	static struct buf name = { string, sizeof string -1, sizeof string }



#define VOLATILE_BUF(name, strname) \
	struct buf name = { strname, strlen(strname) }



#define BUFPUTSL(output, literal) \
	bufput(output, literal, sizeof literal - 1)




#ifdef __GNUC__
#define BUF_ALLOCATOR \
	__attribute__ ((malloc))
#else
#define BUF_ALLOCATOR
#endif



#ifdef __GNUC__
#define BUF_PRINTF_LIKE(format_index, first_variadic_index) \
	__attribute__ ((format (printf, format_index, first_variadic_index)))
#else
#define BUF_PRINTF_LIKE(format_index, first_variadic_index)
#endif




int
bufcasecmp(const struct buf *, const struct buf *);


int
bufcmp(const struct buf *, const struct buf *);


int
bufcmps(const struct buf *, const char *);


struct buf *
bufdup(const struct buf *, size_t)
	BUF_ALLOCATOR;


int
bufgrow(struct buf *, size_t);


struct buf *
bufnew(size_t)
	BUF_ALLOCATOR;


void
bufnullterm(struct buf *);


void
bufprintf(struct buf *, const char *, ...)
	BUF_PRINTF_LIKE(2, 3);


void
bufput(struct buf *, const void*, size_t);


void
bufputs(struct buf *, const char*);


void
bufputc(struct buf *, char);


void
bufrelease(struct buf *);


void
bufreset(struct buf *);


void
bufset(struct buf **, struct buf *);


void
bufslurp(struct buf *, size_t);


int
buftoi(struct buf *, size_t, size_t *);


void
vbufprintf(struct buf *, const char*, va_list);



#ifdef BUFFER_STATS

extern long buffer_stat_nb;
extern size_t buffer_stat_alloc_bytes;

#endif


#endif





#ifndef LITHIUM_MARKDOWN_H
#define LITHIUM_MARKDOWN_H





enum mkd_autolink {
	MKDA_NOT_AUTOLINK,
	MKDA_NORMAL,
	MKDA_EXPLICIT_EMAIL,
	MKDA_IMPLICIT_EMAIL
};


struct mkd_renderer {

	void (*prolog)(struct buf *ob, void *opaque);
	void (*epilog)(struct buf *ob, void *opaque);


	void (*blockcode)(struct buf *ob, struct buf *text, void *opaque);
	void (*blockquote)(struct buf *ob, struct buf *text, void *opaque);
	void (*blockhtml)(struct buf *ob, struct buf *text, void *opaque);
	void (*header)(struct buf *ob, struct buf *text,
						int level, void *opaque);
	void (*hrule)(struct buf *ob, void *opaque);
	void (*list)(struct buf *ob, struct buf *text, int flags, void *opaque);
	void (*listitem)(struct buf *ob, struct buf *text,
						int flags, void *opaque);
	void (*paragraph)(struct buf *ob, struct buf *text, void *opaque);
	void (*table)(struct buf *ob, struct buf *head_row, struct buf *rows,
							void *opaque);
	void (*table_cell)(struct buf *ob, struct buf *text, int flags,
							void *opaque);
	void (*table_row)(struct buf *ob, struct buf *cells, int flags,
							void *opaque);


	int (*autolink)(struct buf *ob, struct buf *link,
					enum mkd_autolink type, void *opaque);
	int (*codespan)(struct buf *ob, struct buf *text, void *opaque);
	int (*double_emphasis)(struct buf *ob, struct buf *text,
						char c, void *opaque);
	int (*emphasis)(struct buf *ob, struct buf *text, char c,void*opaque);
	int (*image)(struct buf *ob, struct buf *link, struct buf *title,
						struct buf *alt, void *opaque);
	int (*linebreak)(struct buf *ob, void *opaque);
	int (*link)(struct buf *ob, struct buf *link, struct buf *title,
					struct buf *content, void *opaque);
	int (*raw_html_tag)(struct buf *ob, struct buf *tag, void *opaque);
	int (*triple_emphasis)(struct buf *ob, struct buf *text,
						char c, void *opaque);


	void (*entity)(struct buf *ob, struct buf *entity, void *opaque);
	void (*normal_text)(struct buf *ob, struct buf *text, void *opaque);


	int max_work_stack;
	const char *emph_chars;
	void *opaque;
};





#define MKD_LIST_ORDERED	1
#define MKD_LI_BLOCK		2


#define MKD_CELL_ALIGN_DEFAULT	0
#define MKD_CELL_ALIGN_LEFT	1
#define MKD_CELL_ALIGN_RIGHT	2
#define MKD_CELL_ALIGN_CENTER	3
#define MKD_CELL_ALIGN_MASK	3
#define MKD_CELL_HEAD		4





void
markdown(struct buf *ob, struct buf *ib, const struct mkd_renderer *rndr);


#endif





#ifndef MARKDOWN_RENDERERS_H
#define MARKDOWN_RENDERERS_H





void
lus_attr_escape(struct buf *ob, const char *src, size_t size);


void
lus_body_escape(struct buf *ob, const char *src, size_t size);





extern const struct mkd_renderer mkd_html;
extern const struct mkd_renderer mkd_xhtml;


extern const struct mkd_renderer discount_html;
extern const struct mkd_renderer discount_xhtml;


extern const struct mkd_renderer nat_html;
extern const struct mkd_renderer nat_xhtml;

#endif

#endif /* SOLDOUT_H */
