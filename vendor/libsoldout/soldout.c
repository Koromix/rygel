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

#include "soldout.h"





#include <string.h>




static int
arr_realloc(struct array* arr, int neosz) {
	void* neo;
	neo = realloc(arr->base, neosz * arr->unit);
	if (neo == 0) return 0;
	arr->base = neo;
	arr->asize = neosz;
	if (arr->size > neosz) arr->size = neosz;
	return 1; }



static int
parr_realloc(struct parray* arr, int neosz) {
	void* neo;
	neo = realloc(arr->item, neosz * sizeof (void*));
	if (neo == 0) return 0;
	arr->item = neo;
	arr->asize = neosz;
	if (arr->size > neosz) arr->size = neosz;
	return 1; }





int
arr_adjust(struct array *arr) {
	return arr_realloc(arr, arr->size); }



void
arr_free(struct array *arr) {
	if (!arr) return;
	free(arr->base);
	arr->base = 0;
	arr->size = arr->asize = 0; }



int
arr_grow(struct array *arr, int need) {
	if (arr->asize >= need) return 1;
	else return arr_realloc(arr, need); }



void
arr_init(struct array *arr, size_t unit) {
	arr->base = 0;
	arr->size = arr->asize = 0;
	arr->unit = unit; }



int
arr_insert(struct array *arr, int nb, int n) {
	char *src, *dst;
	size_t len;
	if (!arr || nb <= 0 || n < 0
	|| !arr_grow(arr, arr->size + nb))
		return 0;
	if (n < arr->size) {
		src = arr->base;
		src += n * arr->unit;
		dst = src + nb * arr->unit;
		len = (arr->size - n) * arr->unit;
		memmove(dst, src, len); }
	arr->size += nb;
	return 1; }



void *
arr_item(struct array *arr, int no) {
	char *ptr;
	if (!arr || no < 0 || no >= arr->size) return 0;
	ptr = arr->base;
	ptr += no * arr->unit;
	return ptr; }



int
arr_newitem(struct array *arr) {
	if (!arr_grow(arr, arr->size + 1)) return -1;
	arr->size += 1;
	return arr->size - 1; }



void
arr_remove(struct array *arr, int idx) {
	if (!arr || idx < 0 || idx >= arr->size) return;
	arr->size -= 1;
	if (idx < arr->size) {
		char *dst = arr->base;
		char *src;
		dst += idx * arr->unit;
		src = dst + arr->unit;
		memmove(dst, src, (arr->size - idx) * arr->unit); } }



void *
arr_sorted_find(struct array *arr, void *key, array_cmp_fn cmp) {
	int mi, ma, cu, ret;
	char *ptr = arr->base;
	mi = -1;
	ma = arr->size;
	while (mi < ma - 1) {
		cu = mi + (ma - mi) / 2;
		ret = cmp(key, ptr + cu * arr->unit);
		if (ret == 0) return ptr + cu * arr->unit;
		else if (ret < 0) ma = cu;
		else  mi = cu; }
	return 0; }


int
arr_sorted_find_i(struct array *arr, void *key, array_cmp_fn cmp) {
	int mi, ma, cu, ret;
	char *ptr = arr->base;
	mi = -1;
	ma = arr->size;
	while (mi < ma - 1) {
		cu = mi + (ma - mi) / 2;
		ret = cmp(key, ptr + cu * arr->unit);
		if (ret == 0) {
			while (cu < arr->size && ret == 0) {
				cu += 1;
				ret = cmp(key, ptr + cu * arr->unit); }
			return cu; }
		else if (ret < 0) ma = cu;
		else  mi = cu; }
	return ma; }





int
parr_adjust(struct parray* arr) {
	return parr_realloc (arr, arr->size); }



void
parr_free(struct parray *arr) {
	if (!arr) return;
	free (arr->item);
	arr->item = 0;
	arr->size = 0;
	arr->asize = 0; }



int
parr_grow(struct parray *arr, int need) {
	if (arr->asize >= need) return 1;
	else return parr_realloc (arr, need); }



void
parr_init(struct parray *arr) {
	arr->item = 0;
	arr->size = 0;
	arr->asize = 0; }



int
parr_insert(struct parray *parr, int nb, int n) {
	char *src, *dst;
	size_t len, i;
	if (!parr || nb <= 0 || n < 0
	|| !parr_grow(parr, parr->size + nb))
		return 0;
	if (n < parr->size) {
		src = (void *)parr->item;
		src += n * sizeof (void *);
		dst = src + nb * sizeof (void *);
		len = (parr->size - n) * sizeof (void *);
		memmove(dst, src, len);
		for (i = 0; i < nb; ++i)
			parr->item[n + i] = 0; }
	parr->size += nb;
	return 1; }



void *
parr_pop(struct parray *arr) {
	if (arr->size <= 0) return 0;
	arr->size -= 1;
	return arr->item[arr->size]; }



int
parr_push(struct parray *arr, void *i) {
	if (!parr_grow(arr, arr->size + 1)) return 0;
	arr->item[arr->size] = i;
	arr->size += 1;
	return 1; }



void *
parr_remove(struct parray *arr, int idx) {
	void* ret;
	int i;
	if (!arr || idx < 0 || idx >= arr->size) return 0;
	ret = arr->item[idx];
	for (i = idx+1; i < arr->size; ++i)
		arr->item[i - 1] = arr->item[i];
	arr->size -= 1;
	return ret; }



void *
parr_sorted_find(struct parray *arr, void *key, array_cmp_fn cmp) {
	int mi, ma, cu, ret;
	mi = -1;
	ma = arr->size;
	while (mi < ma - 1) {
		cu = mi + (ma - mi) / 2;
		ret = cmp(key, arr->item[cu]);
		if (ret == 0) return arr->item[cu];
		else if (ret < 0) ma = cu;
		else  mi = cu; }
	return 0; }


int
parr_sorted_find_i(struct parray *arr, void *key, array_cmp_fn cmp) {
	int mi, ma, cu, ret;
	mi = -1;
	ma = arr->size;
	while (mi < ma - 1) {
		cu = mi + (ma - mi) / 2;
		ret = cmp(key, arr->item[cu]);
		if (ret == 0) {
			while (cu < arr->size && ret == 0) {
				cu += 1;
				ret = cmp(key, arr->item[cu]); }
			return cu; }
		else if (ret < 0) ma = cu;
		else  mi = cu; }
	return ma; }



void *
parr_top(struct parray *arr) {
	if (arr == 0 || arr->size <= 0) return 0;
	else return arr->item[arr->size - 1]; }






#include <stdio.h>
#include <stdlib.h>
#include <string.h>




#ifdef BUFFER_STATS
long buffer_stat_nb = 0;
size_t buffer_stat_alloc_bytes = 0;
#endif




static char
lower(char c) {
	return (c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : c; }





int
bufcasecmp(const struct buf *a, const struct buf *b) {
	size_t i = 0;
	size_t cmplen;
	if (a == b) return 0;
	if (!a) return -1; else if (!b) return 1;
	cmplen = (a->size < b->size) ? a->size : b->size;
	while (i < cmplen && lower(a->data[i]) == lower(b->data[i])) ++i;
	if (i < a->size) {
		if (i < b->size)  return lower(a->data[i]) - lower(b->data[i]);
		else		  return 1; }
	else {	if (i < b->size)  return -1;
		else		  return 0; } }



int
bufcmp(const struct buf *a, const struct buf *b) {
	size_t i = 0;
	size_t cmplen;
	if (a == b) return 0;
	if (!a) return -1; else if (!b) return 1;
	cmplen = (a->size < b->size) ? a->size : b->size;
	while (i < cmplen && a->data[i] == b->data[i]) ++i;
	if (i < a->size) {
		if (i < b->size)	return a->data[i] - b->data[i];
		else			return 1; }
	else {	if (i < b->size)	return -1;
		else			return 0; } }



int
bufcmps(const struct buf *a, const char *b) {
	const size_t len = strlen(b);
	size_t cmplen = len;
	int r;
	if (!a || !a->size) return b ? 0 : -1;
	if (len < a->size) cmplen = a->size;
	r = strncmp(a->data, b, cmplen);
	if (r) return r;
	else if (a->size == len) return 0;
	else if (a->size < len) return -1;
	else return 1; }



struct buf *
bufdup(const struct buf *src, size_t dupunit) {
	size_t blocks;
	struct buf *ret;
	if (src == 0) return 0;
	ret = malloc(sizeof (struct buf));
	if (ret == 0) return 0;
	ret->unit = dupunit;
	ret->size = src->size;
	ret->ref = 1;
	if (!src->size) {
		ret->asize = 0;
		ret->data = 0;
		return ret; }
	blocks = (src->size + dupunit - 1) / dupunit;
	ret->asize = blocks * dupunit;
	ret->data = malloc(ret->asize);
	if (ret->data == 0) {
		free(ret);
		return 0; }
	memcpy(ret->data, src->data, src->size);
#ifdef BUFFER_STATS
	buffer_stat_nb += 1;
	buffer_stat_alloc_bytes += ret->asize;
#endif
	return ret; }



int
bufgrow(struct buf *buf, size_t neosz) {
	size_t neoasz;
	void *neodata;
	if (!buf || !buf->unit) return 0;
	if (buf->asize >= neosz) return 1;
	neoasz = buf->asize + buf->unit;
	while (neoasz < neosz) neoasz += buf->unit;
	neodata = realloc(buf->data, neoasz);
	if (!neodata) return 0;
#ifdef BUFFER_STATS
	buffer_stat_alloc_bytes += (neoasz - buf->asize);
#endif
	buf->data = neodata;
	buf->asize = neoasz;
	return 1; }



struct buf *
bufnew(size_t unit) {
	struct buf *ret;
	ret = malloc(sizeof (struct buf));
	if (ret) {
#ifdef BUFFER_STATS
		buffer_stat_nb += 1;
#endif
		ret->data = 0;
		ret->size = ret->asize = 0;
		ret->ref = 1;
		ret->unit = unit; }
	return ret; }



void
bufnullterm(struct buf *buf) {
	if (!buf || !buf->unit) return;
	if (buf->size + 1 <= buf->asize || bufgrow(buf, buf->size + 1))
		buf->data[buf->size] = 0; }



void
bufprintf(struct buf *buf, const char *fmt, ...) {
	va_list ap;
	if (!buf || !buf->unit) return;
	va_start(ap, fmt);
	vbufprintf(buf, fmt, ap);
	va_end(ap); }



void
bufput(struct buf *buf, const void *data, size_t len) {
	if (!buf) return;
	if (buf->size + len > buf->asize && !bufgrow(buf, buf->size + len))
		return;
	memcpy(buf->data + buf->size, data, len);
	buf->size += len; }



void
bufputs(struct buf *buf, const char *str) {
	bufput(buf, str, strlen (str)); }



void
bufputc(struct buf *buf, char c) {
	if (!buf) return;
	if (buf->size + 1 > buf->asize && !bufgrow(buf, buf->size + 1))
		return;
	buf->data[buf->size] = c;
	buf->size += 1; }



void
bufrelease(struct buf *buf) {
	if (!buf || !buf->unit) return;
	buf->ref -= 1;
	if (buf->ref == 0) {
#ifdef BUFFER_STATS
		buffer_stat_nb -= 1;
		buffer_stat_alloc_bytes -= buf->asize;
#endif
		free(buf->data);
		free(buf); } }



void
bufreset(struct buf *buf) {
	if (!buf || !buf->unit || !buf->asize) return;
#ifdef BUFFER_STATS
	buffer_stat_alloc_bytes -= buf->asize;
#endif
	free(buf->data);
	buf->data = 0;
	buf->size = buf->asize = 0; }



void
bufset(struct buf **dest, struct buf *src) {
	if (src) {
		if (!src->asize) src = bufdup(src, 1);
		else src->ref += 1; }
	bufrelease(*dest);
	*dest = src; }



void
bufslurp(struct buf *buf, size_t len) {
	if (!buf || !buf->unit || !len) return;
	if (len >= buf->size) {
		buf->size = 0;
		return; }
	buf->size -= len;
	memmove(buf->data, buf->data + len, buf->size); }



int
buftoi(struct buf *buf, size_t offset_i, size_t *offset_o) {
	int r = 0, neg = 0;
	size_t i = offset_i;
	if (!buf || !buf->size) return 0;
	if (buf->data[i] == '+') i += 1;
	else if (buf->data[i] == '-') {
		neg = 1;
		i += 1; }
	while (i < buf->size && buf->data[i] >= '0' && buf->data[i] <= '9') {
		r = (r * 10) + buf->data[i] - '0';
		i += 1; }
	if (offset_o) *offset_o = i;
	return neg ? -r : r; }




void
vbufprintf(struct buf *buf, const char *fmt, va_list ap) {
	int n;
	va_list ap_save;
	if (buf == 0
	|| (buf->size >= buf->asize && !bufgrow (buf, buf->size + 1)))
		return;
	va_copy(ap_save, ap);
	n = vsnprintf(buf->data + buf->size, buf->asize - buf->size, fmt, ap);
	if (n >= buf->asize - buf->size) {
		if (buf->size + n + 1 > buf->asize
		&& !bufgrow (buf, buf->size + n + 1))
			return;
		n = vsnprintf (buf->data + buf->size,
				buf->asize - buf->size, fmt, ap_save); }
	va_end(ap_save);
	if (n < 0) return;
	buf->size += n; }







#include <assert.h>
#include <string.h>
#ifdef _MSC_VER
    #define strncasecmp _strnicmp
    #define strcasecmp _stricmp
#else
    #include <strings.h>
#endif

#define TEXT_UNIT 64
#define WORK_UNIT 64

#define MKD_LI_END 8




struct link_ref {
	struct buf *	id;
	struct buf *	link;
	struct buf *	title; };






struct render;
typedef size_t
(*char_trigger)(struct buf *ob, struct render *rndr,
		char *data, size_t offset, size_t size);



struct render {
	struct mkd_renderer	make;
	struct array		refs;
	char_trigger		active_char[256];
	struct parray		work; };



struct html_tag {
	char *	text;
	int	size; };





static struct html_tag block_tags[] = {
	{ "p",		1 },
	{ "dl",		2 },
	{ "h1",		2 },
	{ "h2",		2 },
	{ "h3",		2 },
	{ "h4",		2 },
	{ "h5",		2 },
	{ "h6",		2 },
	{ "ol",		2 },
	{ "ul",		2 },
	{ "del",	3 },
	{ "div",	3 },
	{ "ins",	3 },
	{ "pre",	3 },
	{ "form",	4 },
	{ "math",	4 },
	{ "table",	5 },
	{ "iframe",	6 },
	{ "script",	6 },
	{ "fieldset",	8 },
	{ "noscript",	8 },
	{ "blockquote",	10 } };

#define INS_TAG (block_tags + 12)
#define DEL_TAG (block_tags + 10)





static int
build_ref_id(struct buf *id, const char *data, size_t size) {
	size_t beg, i;


	while (size > 0
	&& (data[0] == ' ' || data[0] == '\t' || data[0] == '\n')) {
		data += 1;
		size -= 1; }


	while (size > 0
	&& (data[size - 1] == ' ' || data[size - 1] == '\t'
	 || data[size - 1] == '\n'))
		size -= 1;
	if (size == 0) return -1;


	i = 0;
	id->size = 0;
	while (i < size) {

		beg = i;
		while (i < size
		&& !(data[i] == ' ' || data[i] == '\t' || data[i] == '\n'))
			i += 1;
		bufput(id, data + beg, i - beg);


		if (i < size) bufputc(id, ' ');
		while (i < size
		&& (data[i] == ' ' || data[i] == '\t' || data[i] == '\n'))
			i += 1; }
	return 0; }



static int
cmp_link_ref(void *key, void *array_entry) {
	struct link_ref *lr = array_entry;
	return bufcasecmp(key, lr->id); }



static int
cmp_link_ref_sort(const void *a, const void *b) {
	const struct link_ref *lra = a;
	const struct link_ref *lrb = b;
	return bufcasecmp(lra->id, lrb->id); }



static int
cmp_html_tag(const void *a, const void *b) {
	const struct html_tag *hta = a;
	const struct html_tag *htb = b;
	if (hta->size != htb->size) return hta->size - htb->size;
	return strncasecmp(hta->text, htb->text, hta->size); }



static struct html_tag *
find_block_tag(char *data, size_t size) {
	size_t i = 0;
	struct html_tag key;


	while (i < size && ((data[i] >= '0' && data[i] <= '9')
				|| (data[i] >= 'A' && data[i] <= 'Z')
				|| (data[i] >= 'a' && data[i] <= 'z')))
		i += 1;
	if (i >= size) return 0;


	key.text = data;
	key.size = i;
	return bsearch(&key, block_tags,
				sizeof block_tags / sizeof block_tags[0],
				sizeof block_tags[0], cmp_html_tag); }



static struct buf *
new_work_buffer(struct render *rndr) {
	struct buf *ret = 0;

	if (rndr->work.size < rndr->work.asize) {
		ret = rndr->work.item[rndr->work.size ++];
		ret->size = 0; }
	else {
		ret = bufnew(WORK_UNIT);
		parr_push(&rndr->work, ret); }
	return ret; }



static void
release_work_buffer(struct render *rndr, struct buf *buf) {
	assert(rndr->work.size > 0
	&& rndr->work.item[rndr->work.size - 1] == buf);
	rndr->work.size -= 1; }






static size_t
is_mail_autolink(char *data, size_t size) {
	size_t i = 0, nb = 0;

	while (i < size && (data[i] == '-' || data[i] == '.'
	|| data[i] == '_' || data[i] == '@'
	|| (data[i] >= 'a' && data[i] <= 'z')
	|| (data[i] >= 'A' && data[i] <= 'Z')
	|| (data[i] >= '0' && data[i] <= '9'))) {
		if (data[i] == '@') nb += 1;
		i += 1; }
	if (i >= size || data[i] != '>' || nb != 1) return 0;
	return i + 1; }



static size_t
tag_length(char *data, size_t size, enum mkd_autolink *autolink) {
	size_t i, j;


	if (size < 3) return 0;


	if (data[0] != '<') return 0;
	i = (data[1] == '/') ? 2 : 1;
	if ((data[i] < 'a' || data[i] > 'z')
	&&  (data[i] < 'A' || data[i] > 'Z')) return 0;


	*autolink = MKDA_NOT_AUTOLINK;
	if (size > 6 && strncasecmp(data + 1, "http", 4) == 0 && (data[5] == ':'
	|| ((data[5] == 's' || data[5] == 'S') && data[6] == ':'))) {
		i = data[5] == ':' ? 6 : 7;
		*autolink = MKDA_NORMAL; }
	else if (size > 5 && strncasecmp(data + 1, "ftp:", 4) == 0) {
		i = 5;
		*autolink = MKDA_NORMAL; }
	else if (size > 7 && strncasecmp(data + 1, "mailto:", 7) == 0) {
		i = 8;
		 }


	if (i >= size || i == '>')
		*autolink = MKDA_NOT_AUTOLINK;
	else if (*autolink) {
		j = i;
		while (i < size && data[i] != '>' && data[i] != '\''
		&& data[i] != '"' && data[i] != ' ' && data[i] != '\t'
		&& data[i] != '\n')
			i += 1;
		if (i >= size) return 0;
		if (i > j && data[i] == '>') return i + 1;

		*autolink = MKDA_NOT_AUTOLINK; }
	else if ((j = is_mail_autolink(data + i, size - i)) != 0) {
		*autolink = (i == 8)
				? MKDA_EXPLICIT_EMAIL : MKDA_IMPLICIT_EMAIL;
		return i + j; }


	while (i < size && data[i] != '>') i += 1;
	if (i >= size) return 0;
	return i + 1; }



static void
parse_inline(struct buf *ob, struct render *rndr, char *data, size_t size) {
	size_t i = 0, end = 0;
	char_trigger action = 0;
	struct buf work = { 0, 0, 0, 0, 0 };

	if (rndr->work.size > rndr->make.max_work_stack) {
		if (size) bufput(ob, data, size);
		return; }

	while (i < size) {

		while (end < size
		&& (action = rndr->active_char[(unsigned char)data[end]]) == 0)
			end += 1;
		if (rndr->make.normal_text) {
			work.data = data + i;
			work.size = end - i;
			rndr->make.normal_text(ob, &work, rndr->make.opaque); }
		else
			bufput(ob, data + i, end - i);
		if (end >= size) break;
		i = end;


		end = action(ob, rndr, data + i, i, size - i);
		if (!end)
			end = i + 1;
		else {
			i += end;
			end = i; } } }



static size_t
find_emph_char(char *data, size_t size, char c) {
	size_t i = 1;

	while (i < size) {
		while (i < size && data[i] != c
		&& data[i] != '`' && data[i] != '[')
			i += 1;
		if (i >= size) return 0;
		if (data[i] == c) return i;


		if (i && data[i - 1] == '\\') { i += 1; continue; }


		if (data[i] == '`') {
			size_t span_nb = 0, bt;
			size_t tmp_i = 0;


			while (i < size && data[i] == '`') {
				i += 1;
				span_nb += 1; }
			if (i >= size) return 0;


			bt = 0;
			while (i < size && bt < span_nb) {
				if (!tmp_i && data[i] == c) tmp_i = i;
				if (data[i] == '`') bt += 1;
				else bt = 0;
				i += 1; }
			if (i >= size) return tmp_i;
			i += 1; }


		else if (data[i] == '[') {
			size_t tmp_i = 0;
			char cc;
			i += 1;
			while (i < size && data[i] != ']') {
				if (!tmp_i && data[i] == c) tmp_i = i;
				i += 1; }
			i += 1;
			while (i < size && (data[i] == ' '
			|| data[i] == '\t' || data[i] == '\n'))
				i += 1;
			if (i >= size) return tmp_i;
			if (data[i] != '[' && data[i] != '(') {
				if (tmp_i) return tmp_i;
				else continue; }
			cc = data[i];
			i += 1;
			while (i < size && data[i] != cc) {
				if (!tmp_i && data[i] == c) tmp_i = i;
				i += 1; }
			if (i >= size) return tmp_i;
			i += 1; } }
	return 0; }




static size_t
parse_emph1(struct buf *ob, struct render *rndr,
			char *data, size_t size, char c) {
	size_t i = 0, len;
	struct buf *work = 0;
	int r;

	if (!rndr->make.emphasis) return 0;


	if (size > 1 && data[0] == c && data[1] == c) i = 1;

	while (i < size) {
		len = find_emph_char(data + i, size - i, c);
		if (!len) return 0;
		i += len;
		if (i >= size) return 0;

		if (i + 1 < size && data[i + 1] == c) {
			i += 1;
			continue; }
		if (data[i] == c && data[i - 1] != ' '
		&& data[i - 1] != '\t' && data[i - 1] != '\n') {
			work = new_work_buffer(rndr);
			parse_inline(work, rndr, data, i);
			r = rndr->make.emphasis(ob, work, c, rndr->make.opaque);
			release_work_buffer(rndr, work);
			return r ? i + 1 : 0; } }
	return 0; }



static size_t
parse_emph2(struct buf *ob, struct render *rndr,
			char *data, size_t size, char c) {
	size_t i = 0, len;
	struct buf *work = 0;
	int r;

	if (!rndr->make.double_emphasis) return 0;

	while (i < size) {
		len = find_emph_char(data + i, size - i, c);
		if (!len) return 0;
		i += len;
		if (i + 1 < size && data[i] == c && data[i + 1] == c
		&& i && data[i - 1] != ' '
		&& data[i - 1] != '\t' && data[i - 1] != '\n') {
			work = new_work_buffer(rndr);
			parse_inline(work, rndr, data, i);
			r = rndr->make.double_emphasis(ob, work, c,
				rndr->make.opaque);
			release_work_buffer(rndr, work);
			return r ? i + 2 : 0; }
		i += 1; }
	return 0; }




static size_t
parse_emph3(struct buf *ob, struct render *rndr,
			char *data, size_t size, char c) {
	size_t i = 0, len;
	int r;

	while (i < size) {
		len = find_emph_char(data + i, size - i, c);
		if (!len) return 0;
		i += len;


		if (data[i] != c || data[i - 1] == ' '
		|| data[i - 1] == '\t' || data[i - 1] == '\n')
			continue;

		if (i + 2 < size && data[i + 1] == c && data[i + 2] == c
		&& rndr->make.triple_emphasis) {

			struct buf *work = new_work_buffer(rndr);
			parse_inline(work, rndr, data, i);
			r = rndr->make.triple_emphasis(ob, work, c,
							rndr->make.opaque);
			release_work_buffer(rndr, work);
			return r ? i + 3 : 0; }
		else if (i + 1 < size && data[i + 1] == c) {

			len = parse_emph1(ob, rndr, data - 2, size + 2, c);
			if (!len) return 0;
			else return len - 2; }
		else {

			len = parse_emph2(ob, rndr, data - 1, size + 1, c);
			if (!len) return 0;
			else return len - 1; } }
	return 0; }



static size_t
char_emphasis(struct buf *ob, struct render *rndr,
				char *data, size_t offset, size_t size) {
	char c = data[0];
	size_t ret;
	if (size > 2 && data[1] != c) {

		if (data[1] == ' ' || data[1] == '\t' || data[1] == '\n'
		|| (ret = parse_emph1(ob, rndr, data + 1, size - 1, c)) == 0)
			return 0;
		return ret + 1; }
	if (size > 3 && data[1] == c && data[2] != c) {
		if (data[2] == ' ' || data[2] == '\t' || data[2] == '\n'
		|| (ret = parse_emph2(ob, rndr, data + 2, size - 2, c)) == 0)
			return 0;
		return ret + 2; }
	if (size > 4 && data[1] == c && data[2] == c && data[3] != c) {
		if (data[3] == ' ' || data[3] == '\t' || data[3] == '\n'
		|| (ret = parse_emph3(ob, rndr, data + 3, size - 3, c)) == 0)
			return 0;
		return ret + 3; }
	return 0; }



static size_t
char_linebreak(struct buf *ob, struct render *rndr,
				char *data, size_t offset, size_t size) {
	if (offset < 2 || data[-1] != ' ' || data[-2] != ' ') return 0;

	if (ob->size && ob->data[ob->size - 1] == ' ') ob->size -= 1;
	return rndr->make.linebreak(ob, rndr->make.opaque) ? 1 : 0; }



static size_t
char_codespan(struct buf *ob, struct render *rndr,
				char *data, size_t offset, size_t size) {
	size_t end, nb = 0, i, f_begin, f_end;


	while (nb < size && data[nb] == '`') nb += 1;


	i = 0;
	for (end = nb; end < size && i < nb; end += 1)
		if (data[end] == '`') i += 1;
		else i = 0;
	if (i < nb && end >= size) return 0;


	f_begin = nb;
	while (f_begin < end && (data[f_begin] == ' ' || data[f_begin] == '\t'))
		f_begin += 1;
	f_end = end - nb;
	while (f_end > nb && (data[f_end-1] == ' ' || data[f_end-1] == '\t'))
		f_end -= 1;


	if (f_begin < f_end) {
		struct buf work = { data + f_begin, f_end - f_begin, 0, 0, 0 };
		if (!rndr->make.codespan(ob, &work, rndr->make.opaque))
			end = 0; }
	else {
		if (!rndr->make.codespan(ob, 0, rndr->make.opaque))
			end = 0; }
	return end; }



static size_t
char_escape(struct buf *ob, struct render *rndr,
				char *data, size_t offset, size_t size) {
	struct buf work = { 0, 0, 0, 0, 0 };
	if (size > 1) {
		if (rndr->make.normal_text) {
			work.data = data + 1;
			work.size = 1;
			rndr->make.normal_text(ob, &work, rndr->make.opaque); }
		else bufputc(ob, data[1]); }
	return 2; }




static size_t
char_entity(struct buf *ob, struct render *rndr,
				char *data, size_t offset, size_t size) {
	size_t end = 1;
	struct buf work;
	if (end < size && data[end] == '#') end += 1;
	while (end < size
	&& ((data[end] >= '0' && data[end] <= '9')
	||  (data[end] >= 'a' && data[end] <= 'z')
	||  (data[end] >= 'A' && data[end] <= 'Z')))
		end += 1;
	if (end < size && data[end] == ';') {

		end += 1; }
	else {

		return 0; }
	if (rndr->make.entity) {
		work.data = data;
		work.size = end;
		rndr->make.entity(ob, &work, rndr->make.opaque); }
	else bufput(ob, data, end);
	return end; }



static size_t
char_langle_tag(struct buf *ob, struct render *rndr,
				char *data, size_t offset, size_t size) {
	enum mkd_autolink altype = MKDA_NOT_AUTOLINK;
	size_t end = tag_length(data, size, &altype);
	struct buf work = { data, end, 0, 0, 0 };
	int ret = 0;
	if (end) {
		if (rndr->make.autolink && altype != MKDA_NOT_AUTOLINK) {
			work.data = data + 1;
			work.size = end - 2;
			ret = rndr->make.autolink(ob, &work, altype,
							rndr->make.opaque); }
		else if (rndr->make.raw_html_tag)
			ret = rndr->make.raw_html_tag(ob, &work,
							rndr->make.opaque); }
	if (!ret) return 0;
	else return end; }



static int
get_link_inline(struct buf *link, struct buf *title, char *data, size_t size) {
	size_t i = 0, mark;
	size_t link_b, link_e;
	size_t title_b = 0, title_e = 0;

	link->size = title->size = 0;


	while (i < size
	&& (data[i] == ' ' || data[i] == '\t' || data[i] == '\n'))
		i += 1;
	link_b = i;


	while (i < size && data[i] != '\'' && data[i] != '"')
		i += 1;
	link_e = i;


	if (data[i] == '\'' || data[i] == '"') {
		i += 1;
		title_b = i;


		title_e = size - 1;
		while (title_e > title_b && (data[title_e] == ' '
		|| data[title_e] == '\t' || data[title_e] == '\n'))
			title_e -= 1;


		if (data[title_e] != '\'' &&  data[title_e] != '"') {
			title_b = title_e = 0;
			link_e = i; } }


	while (link_e > link_b
	&& (data[link_e - 1] == ' ' || data[link_e - 1] == '\t'
	 || data[link_e - 1] == '\n'))
		link_e -= 1;


	if (data[link_b] == '<') link_b += 1;
	if (data[link_e - 1] == '>') link_e -= 1;


	link->size = 0;
	i = link_b;
	while (i < link_e) {
		mark = i;
		while (i < link_e && data[i] != '\\') i += 1;
		bufput(link, data + mark, i - mark);
		while (i < link_e && data[i] == '\\') i += 1; }


	title->size = 0;
	if (title_e > title_b)
		bufput(title, data + title_b, title_e - title_b);


	return 0; }



static int
get_link_ref(struct render *rndr, struct buf *link, struct buf *title,
				char * data, size_t size) {
	struct link_ref *lr;


	link->size = 0;
	if (build_ref_id(link, data, size) < 0)
		return -1;
	lr = arr_sorted_find(&rndr->refs, link, cmp_link_ref);
	if (!lr) return -1;


	link->size = 0;
	if (lr->link)
		bufput(link, lr->link->data, lr->link->size);
	title->size = 0;
	if (lr->title)
		bufput(title, lr->title->data, lr->title->size);
	return 0; }



static size_t
char_link(struct buf *ob, struct render *rndr,
				char *data, size_t offset, size_t size) {
	int is_img = (offset && data[-1] == '!'), level;
	size_t i = 1, txt_e;
	struct buf *content = 0;
	struct buf *link = 0;
	struct buf *title = 0;
	int ret;


	if ((is_img && !rndr->make.image) || (!is_img && !rndr->make.link))
		return 0;


	for (level = 1; i < size; i += 1)
		if (data[i - 1] == '\\') continue;
		else if (data[i] == '[') level += 1;
		else if (data[i] == ']') {
			level -= 1;
			if (level <= 0) break; }
	if (i >= size) return 0;
	txt_e = i;
	i += 1;



	while (i < size
	&& (data[i] == ' ' || data[i] == '\t' || data[i] == '\n'))
		i += 1;


	content = new_work_buffer(rndr);
	link = new_work_buffer(rndr);
	title = new_work_buffer(rndr);
	ret = 0;


	if (i < size && data[i] == '(') {
		size_t span_end = i;
		while (span_end < size
		&& !(data[span_end] == ')'
		 && (span_end == i || data[span_end - 1] != '\\')))
			span_end += 1;

		if (span_end >= size
		|| get_link_inline(link, title,
					data + i+1, span_end - (i+1)) < 0)
			goto char_link_cleanup;

		i = span_end + 1; }


	else if (i < size && data[i] == '[') {
		char *id_data;
		size_t id_size, id_end = i;
		while (id_end < size && data[id_end] != ']')
			id_end += 1;

		if (id_end >= size)
			goto char_link_cleanup;

		if (i + 1 == id_end) {

			id_data = data + 1;
			id_size = txt_e - 1; }
		else {

			id_data = data + i + 1;
			id_size = id_end - (i + 1); }

		if (get_link_ref(rndr, link, title, id_data, id_size) < 0)
			goto char_link_cleanup;

		i = id_end + 1; }


	else {
		if (get_link_ref(rndr, link, title, data + 1, txt_e - 1) < 0)
			goto char_link_cleanup;


		i = txt_e + 1; }


	if (txt_e > 1) {
		if (is_img) bufput(content, data + 1, txt_e - 1);
		else parse_inline(content, rndr, data + 1, txt_e - 1); }


	if (is_img) {
		if (ob->size && ob->data[ob->size - 1] == '!') ob->size -= 1;
		ret = rndr->make.image(ob, link, title, content,
							rndr->make.opaque); }
	else ret = rndr->make.link(ob, link, title, content, rndr->make.opaque);


char_link_cleanup:
	release_work_buffer(rndr, title);
	release_work_buffer(rndr, link);
	release_work_buffer(rndr, content);
	return ret ? i : 0; }





static size_t
is_empty(char *data, size_t size) {
	size_t i;
	for (i = 0; i < size && data[i] != '\n'; i += 1)
		if (data[i] != ' ' && data[i] != '\t') return 0;
	return i + 1; }



static int
is_hrule(char *data, size_t size) {
	size_t i = 0, n = 0;
	char c;


	if (size < 3) return 0;
	if (data[0] == ' ') { i += 1;
	if (data[1] == ' ') { i += 1;
	if (data[2] == ' ') { i += 1; } } }


	if (i + 2 >= size
	|| (data[i] != '*' && data[i] != '-' && data[i] != '_'))
		return 0;
	c = data[i];


	while (i < size && data[i] != '\n') {
		if (data[i] == c) n += 1;
		else if (data[i] != ' ' && data[i] != '\t')
			return 0;
		i += 1; }

	return n >= 3; }



static int
is_headerline(char *data, size_t size) {
	size_t i = 0;


	if (data[i] == '=') {
		for (i = 1; i < size && data[i] == '='; i += 1);
		while (i < size && (data[i] == ' ' || data[i] == '\t')) i += 1;
		return (i >= size || data[i] == '\n') ? 1 : 0; }


	if (data[i] == '-') {
		for (i = 1; i < size && data[i] == '-'; i += 1);
		while (i < size && (data[i] == ' ' || data[i] == '\t')) i += 1;
		return (i >= size || data[i] == '\n') ? 2 : 0; }

	return 0; }



static int
is_table_sep(char *data, size_t pos) {
	return data[pos] == '|' && (pos == 0 || data[pos - 1] != '\\'); }



static int
is_tableline(char *data, size_t size) {
	size_t i = 0;
	int n_sep = 0, outer_sep = 0;


	while (i < size && (data[i] == ' ' || data[i] == '\t'))
		i += 1;


	if (i < size && data[i] == '|')
		outer_sep += 1;


	for (n_sep = 0; i < size && data[i] != '\n'; i += 1)
		if (is_table_sep(data, i))
			n_sep += 1;


	while (i
	&& (data[i - 1] == ' ' || data[i - 1] == '\t' || data[i - 1] == '\n'))
		i -= 1;
	if (i && is_table_sep(data, i - 1))
		outer_sep += 1;


	return (n_sep > 0) ? (n_sep - outer_sep + 1) : 0; }



static size_t
prefix_quote(char *data, size_t size) {
	size_t i = 0;
	if (i < size && data[i] == ' ') i += 1;
	if (i < size && data[i] == ' ') i += 1;
	if (i < size && data[i] == ' ') i += 1;
	if (i < size && data[i] == '>') {
		if (i + 1 < size && (data[i + 1] == ' ' || data[i+1] == '\t'))
			return i + 2;
		else return i + 1; }
	else return 0; }



static size_t
prefix_code(char *data, size_t size) {
	if (size > 0 && data[0] == '\t') return 1;
	if (size > 3 && data[0] == ' ' && data[1] == ' '
			&& data[2] == ' ' && data[3] == ' ') return 4;
	return 0; }


static size_t
prefix_oli(char *data, size_t size) {
	size_t i = 0;
	if (i < size && data[i] == ' ') i += 1;
	if (i < size && data[i] == ' ') i += 1;
	if (i < size && data[i] == ' ') i += 1;
	if (i >= size || data[i] < '0' || data[i] > '9') return 0;
	while (i < size && data[i] >= '0' && data[i] <= '9') i += 1;
	if (i + 1 >= size || data[i] != '.'
	|| (data[i + 1] != ' ' && data[i + 1] != '\t')) return 0;
	i = i + 2;
	while (i < size && (data[i] == ' ' || data[i] == '\t')) i += 1;
	return i; }



static size_t
prefix_uli(char *data, size_t size) {
	size_t i = 0;
	if (i < size && data[i] == ' ') i += 1;
	if (i < size && data[i] == ' ') i += 1;
	if (i < size && data[i] == ' ') i += 1;
	if (i + 1 >= size
	|| (data[i] != '*' && data[i] != '+' && data[i] != '-')
	|| (data[i + 1] != ' ' && data[i + 1] != '\t'))
		return 0;
	i = i + 2;
	while (i < size && (data[i] == ' ' || data[i] == '\t')) i += 1;
	return i; }



static void parse_block(struct buf *ob, struct render *rndr,
			char *data, size_t size);



static size_t
parse_blockquote(struct buf *ob, struct render *rndr,
			char *data, size_t size) {
	size_t beg, end = 0, pre, work_size = 0;
	char *work_data = 0;
	struct buf *out = new_work_buffer(rndr);

	beg = 0;
	while (beg < size) {
		for (end = beg + 1; end < size && data[end - 1] != '\n';
							end += 1);
		pre = prefix_quote(data + beg, end - beg);
		if (pre) beg += pre;
		else if (is_empty(data + beg, end - beg)
		&& (end >= size || (prefix_quote(data + end, size - end) == 0
					&& !is_empty(data + end, size - end))))

			break;
		if (beg < end) {

			if (!work_data)
				work_data = data + beg;
			else if (data + beg != work_data + work_size)
				memmove(work_data + work_size, data + beg,
						end - beg);
			work_size += end - beg; }
		beg = end; }

	parse_block(out, rndr, work_data, work_size);
	if (rndr->make.blockquote)
		rndr->make.blockquote(ob, out, rndr->make.opaque);
	release_work_buffer(rndr, out);
	return end; }



static size_t
parse_paragraph(struct buf *ob, struct render *rndr,
			char *data, size_t size) {
	size_t i = 0, end = 0;
	int level = 0;
	struct buf work = { data, 0, 0, 0, 0 };

	while (i < size) {
		for (end = i + 1; end < size && data[end - 1] != '\n';
								end += 1);
		if (is_empty(data + i, size - i)
		|| (level = is_headerline(data + i, size - i)) != 0)
			break;
		if ((i && data[i] == '#')
		|| is_hrule(data + i, size - i)) {
			end = i;
			break; }
		i = end; }

	work.size = i;
	while (work.size && data[work.size - 1] == '\n')
		work.size -= 1;
	if (!level) {
		struct buf *tmp = new_work_buffer(rndr);
		parse_inline(tmp, rndr, work.data, work.size);
		if (rndr->make.paragraph)
			rndr->make.paragraph(ob, tmp, rndr->make.opaque);
		release_work_buffer(rndr, tmp); }
	else {
		if (work.size) {
			size_t beg;
			i = work.size;
			work.size -= 1;
			while (work.size && data[work.size] != '\n')
				work.size -= 1;
			beg = work.size + 1;
			while (work.size && data[work.size - 1] == '\n')
				work.size -= 1;
			if (work.size) {
				struct buf *tmp = new_work_buffer(rndr);
				parse_inline(tmp, rndr, work.data, work.size);
				if (rndr->make.paragraph)
					rndr->make.paragraph(ob, tmp,
							rndr->make.opaque);
				release_work_buffer(rndr, tmp);
				work.data += beg;
				work.size = i - beg; }
			else work.size = i; }
		if (rndr->make.header) {
			struct buf *span = new_work_buffer(rndr);
			parse_inline(span, rndr, work.data, work.size);
			rndr->make.header(ob, span, level,rndr->make.opaque);
			release_work_buffer(rndr, span); } }
	return end; }



static size_t
parse_blockcode(struct buf *ob, struct render *rndr,
			char *data, size_t size) {
	size_t beg, end, pre;
	struct buf *work = new_work_buffer(rndr);

	beg = 0;
	while (beg < size) {
		for (end = beg + 1; end < size && data[end - 1] != '\n';
							end += 1);
		pre = prefix_code(data + beg, end - beg);
		if (pre) beg += pre;
		else if (!is_empty(data + beg, end - beg))

			break;
		if (beg < end) {
			if (is_empty(data + beg, end - beg))
				bufputc(work, '\n');
			else bufput(work, data + beg, end - beg); }
		beg = end; }

	while (work->size && work->data[work->size - 1] == '\n')
		work->size -= 1;
	bufputc(work, '\n');
	if (rndr->make.blockcode)
		rndr->make.blockcode(ob, work, rndr->make.opaque);
	release_work_buffer(rndr, work);
	return beg; }




static size_t
parse_listitem(struct buf *ob, struct render *rndr,
			char *data, size_t size, int *flags) {
	struct buf *work = 0, *inter = 0;
	size_t beg = 0, end, pre, sublist = 0, orgpre = 0, i;
	int in_empty = 0, has_inside_empty = 0;


	if (size > 1 && data[0] == ' ') { orgpre = 1;
	if (size > 2 && data[1] == ' ') { orgpre = 2;
	if (size > 3 && data[2] == ' ') { orgpre = 3; } } }
	beg = prefix_uli(data, size);
	if (!beg) beg = prefix_oli(data, size);
	if (!beg) return 0;

	end = beg;
	while (end < size && data[end - 1] != '\n') end += 1;


	work = new_work_buffer(rndr);
	inter = new_work_buffer(rndr);


	bufput(work, data + beg, end - beg);
	beg = end;


	while (beg < size) {
		end += 1;
		while (end < size && data[end - 1] != '\n') end += 1;


		if (is_empty(data + beg, end - beg)) {
			in_empty = 1;
			beg = end;
			continue; }


		i = 0;
		if (end - beg > 1 && data[beg] == ' ') { i = 1;
		if (end - beg > 2 && data[beg + 1] == ' ') { i = 2;
		if (end - beg > 3 && data[beg + 2] == ' ') { i = 3;
		if (end - beg > 3 && data[beg + 3] == ' ') { i = 4; } } } }
		pre = i;
		if (data[beg] == '\t') { i = 1; pre = 8; }


		if ((prefix_uli(data + beg + i, end - beg - i)
			&& !is_hrule(data + beg + i, end - beg - i))
		||  prefix_oli(data + beg + i, end - beg - i)) {
			if (in_empty) has_inside_empty = 1;
			if (pre == orgpre)
				break;
			if (!sublist) sublist = work->size;
			else if (in_empty) bufputc(work, '\n'); }


		else if (in_empty && i < 4 && data[beg] != '\t') {
				*flags |= MKD_LI_END;
				break; }
		else if (in_empty) {
			bufputc(work, '\n');
			has_inside_empty = 1; }
		in_empty = 0;


		bufput(work, data + beg + i, end - beg - i);
		beg = end; }


	if (has_inside_empty) *flags |= MKD_LI_BLOCK;
	if (*flags & MKD_LI_BLOCK) {

		if (sublist && sublist < work->size) {
			parse_block(inter, rndr, work->data, sublist);
			parse_block(inter, rndr, work->data + sublist,
						work->size - sublist); }
		else
			parse_block(inter, rndr, work->data, work->size); }
	else {

		if (sublist && sublist < work->size) {
			parse_inline(inter, rndr, work->data, sublist);
			parse_block(inter, rndr, work->data + sublist,
						work->size - sublist); }
		else
			parse_inline(inter, rndr, work->data, work->size); }


	if (rndr->make.listitem)
		rndr->make.listitem(ob, inter, *flags, rndr->make.opaque);
	release_work_buffer(rndr, inter);
	release_work_buffer(rndr, work);
	return beg; }



static size_t
parse_list(struct buf *ob, struct render *rndr,
			char *data, size_t size, int flags) {
	struct buf *work = new_work_buffer(rndr);
	size_t i = 0, j;

	while (i < size) {
		j = parse_listitem(work, rndr, data + i, size - i, &flags);
		i += j;
		if (!j || (flags & MKD_LI_END)) break; }

	if (rndr->make.list)
		rndr->make.list(ob, work, flags, rndr->make.opaque);
	release_work_buffer(rndr, work);
	return i; }



static size_t
parse_atxheader(struct buf *ob, struct render *rndr,
			char *data, size_t size) {
	int level = 0;
	size_t i, end, skip, span_beg, span_size;

	if (!size || data[0] != '#') return 0;

	while (level < size && level < 6 && data[level] == '#') level += 1;
	for (i = level; i < size && (data[i] == ' ' || data[i] == '\t');
							i += 1);
	span_beg = i;

	for (end = i; end < size && data[end] != '\n'; end += 1);
	skip = end;
	if (end <= i)
		return parse_paragraph(ob, rndr, data, size);
	while (end && data[end - 1] == '#') end -= 1;
	while (end && (data[end - 1] == ' ' || data[end - 1] == '\t')) end -= 1;
	if (end <= i)
		return parse_paragraph(ob, rndr, data, size);

	span_size = end - span_beg;
	if (rndr->make.header) {
		struct buf *span = new_work_buffer(rndr);
		parse_inline(span, rndr, data + span_beg, span_size);
		rndr->make.header(ob, span, level, rndr->make.opaque);
		release_work_buffer(rndr, span); }
	return skip; }




static size_t
htmlblock_end(struct html_tag *tag, char *data, size_t size) {
	size_t i, w;




	if (tag->size + 3 >= size
	|| strncasecmp(data + 2, tag->text, tag->size)
	|| data[tag->size + 2] != '>')
		return 0;


	i = tag->size + 3;
	w = 0;
	if (i < size && (w = is_empty(data + i, size - i)) == 0)
		return 0;
	i += w;
	w = 0;
	if (i < size && (w = is_empty(data + i, size - i)) == 0)
		return 0;
	return i + w; }



static size_t
parse_htmlblock(struct buf *ob, struct render *rndr,
			char *data, size_t size) {
	size_t i, j = 0;
	struct html_tag *curtag;
	int found;
	struct buf work = { data, 0, 0, 0, 0 };


	if (size < 2 || data[0] != '<') return 0;
	curtag = find_block_tag(data + 1, size - 1);


	if (!curtag) {

		if (size > 5 && data[1] == '!'
		&& data[2] == '-' && data[3] == '-') {
			i = 5;
			while (i < size
			&& !(data[i - 2] == '-' && data[i - 1] == '-'
						&& data[i] == '>'))
				i += 1;
			i += 1;
			if (i < size) {
				j = is_empty(data + i, size - i);
				if (j) {
					work.size = i + j;
					if (rndr->make.blockhtml)
						rndr->make.blockhtml(ob, &work,
							rndr->make.opaque);
					return work.size; } } }


		if (size > 4
		&& (data[1] == 'h' || data[1] == 'H')
		&& (data[2] == 'r' || data[2] == 'R')) {
			i = 3;
			while (i < size && data[i] != '>')
				i += 1;
			if (i + 1 < size) {
				i += 1;
				j = is_empty(data + i, size - i);
				if (j) {
					work.size = i + j;
					if (rndr->make.blockhtml)
						rndr->make.blockhtml(ob, &work,
							rndr->make.opaque);
					return work.size; } } }


		return 0; }



	i = 1;
	found = 0;
#if 0
	while (i < size) {
		i += 1;
		while (i < size && !(data[i - 2] == '\n'
		&& data[i - 1] == '<' && data[i] == '/'))
			i += 1;
		if (i + 2 + curtag->size >= size) break;
		j = htmlblock_end(curtag, data + i - 1, size - i + 1);
		if (j) {
			i += j - 1;
			found = 1;
			break; } }
#endif



	if (!found && curtag != INS_TAG && curtag != DEL_TAG) {
		i = 1;
		while (i < size) {
			i += 1;
			while (i < size
			&& !(data[i - 1] == '<' && data[i] == '/'))
				i += 1;
		if (i + 2 + curtag->size >= size) break;
		j = htmlblock_end(curtag, data + i - 1, size - i + 1);
		if (j) {
			i += j - 1;
			found = 1;
			break; } } }

	if (!found) return 0;


	work.size = i;
	if (rndr->make.blockhtml)
		rndr->make.blockhtml(ob, &work, rndr->make.opaque);
	return i; }



static void
parse_table_cell(struct buf *ob, struct render *rndr, char *data, size_t size,
				int flags) {
	struct buf *span = new_work_buffer(rndr);
	parse_inline(span, rndr, data, size);
	rndr->make.table_cell(ob, span, flags, rndr->make.opaque);
	release_work_buffer(rndr, span); }



static size_t
parse_table_row(struct buf *ob, struct render *rndr, char *data, size_t size,
				int *aligns, size_t align_size, int flags) {
	size_t i = 0, col = 0;
	size_t beg, end, total = 0;
	struct buf *cells = new_work_buffer(rndr);
	int align;


	while (i < size && (data[i] == ' ' || data[i] == '\t'))
		i += 1;
	if (i < size && data[i] == '|')
		i += 1;


	while (i < size && total == 0) {

		align = 0;
		if (data[i] == ':') {
			align |= MKD_CELL_ALIGN_LEFT;
			i += 1; }


		while (i < size && (data[i] == ' ' || data[i] == '\t'))
			i += 1;
		beg = i;


		while (i < size && !is_table_sep(data, i) && data[i] != '\n')
			i += 1;
		end = i;
		if (i < size) {
			i += 1;
			if (data[i - 1] == '\n')
				total = i; }


		if (i > beg && data[end - 1] == ':') {
			align |= MKD_CELL_ALIGN_RIGHT;
			end -= 1; }


		while (end > beg
		&& (data[end - 1] == ' ' || data[end - 1] == '\t'))
			end -= 1;



		if (total && end <= beg) continue;


		if (align == 0 && aligns && col < align_size)
			align = aligns[col];


		parse_table_cell(cells, rndr, data + beg, end - beg,
		    align | flags);

		col += 1; }


	rndr->make.table_row(ob, cells, flags, rndr->make.opaque);
	release_work_buffer(rndr, cells);
	return total ? total : size; }



static size_t
parse_table(struct buf *ob, struct render *rndr, char *data, size_t size) {
	size_t i = 0, head_end, col;
	size_t align_size = 0;
	int *aligns = 0;
	struct buf *head = 0;
	struct buf *rows = new_work_buffer(rndr);


	while (i < size && data[i] != '\n')
		i += 1;
	head_end = i;


	if (i >= size) {
		parse_table_row(rows, rndr, data, size, 0, 0, 0);
		rndr->make.table(ob, 0, rows, rndr->make.opaque);
		release_work_buffer(rndr, rows);
		return i; }


	i += 1;
	col = 0;
	while (i < size && (data[i] == ' ' || data[i] == '\t' || data[i] == '-'
			 || data[i] == ':' || data[i] == '|')) {
		if (data[i] == '|') align_size += 1;
		if (data[i] == ':') col = 1;
		i += 1; }

	if (i < size && data[i] == '\n') {
		align_size += 1;


		head = new_work_buffer(rndr);
		parse_table_row(head, rndr, data, head_end, 0, 0,
		    MKD_CELL_HEAD);


		if (col && (aligns = malloc(align_size * sizeof *aligns)) != 0){
			for (i = 0; i < align_size; i += 1)
				aligns[i] = 0;
			col = 0;
			i = head_end + 1;


			while (i < size && (data[i] == ' ' || data[i] == '\t'))
				i += 1;
			if (data[i] == '|') i += 1;


			while (i < size && data[i] != '\n') {
				if (data[i] == ':')
					aligns[col] |= MKD_CELL_ALIGN_LEFT;
				while (i < size
				&& data[i] != '|' && data[i] != '\n')
					i += 1;
				if (data[i - 1] == ':')
					aligns[col] |= MKD_CELL_ALIGN_RIGHT;
				if (i < size && data[i] == '|')
					i += 1;
				col += 1; } }


		i += 1; }

	else {

		i = 0; }


	while (i < size && is_tableline(data + i, size - i))
		i += parse_table_row(rows, rndr, data + i, size - i,
		    aligns, align_size, 0);


	rndr->make.table(ob, head, rows, rndr->make.opaque);


	if (head) release_work_buffer(rndr, head);
	release_work_buffer(rndr, rows);
	free(aligns);
	return i; }



static void
parse_block(struct buf *ob, struct render *rndr,
			char *data, size_t size) {
	size_t beg, end, i;
	char *txt_data;
	int has_table = (rndr->make.table && rndr->make.table_row
	    && rndr->make.table_cell);

	if (rndr->work.size > rndr->make.max_work_stack) {
		if (size) bufput(ob, data, size);
		return; }

	beg = 0;
	while (beg < size) {
		txt_data = data + beg;
		end = size - beg;
		if (data[beg] == '#')
			beg += parse_atxheader(ob, rndr, txt_data, end);
		else if (data[beg] == '<' && rndr->make.blockhtml
			&& (i = parse_htmlblock(ob, rndr, txt_data, end)) != 0)
			beg += i;
		else if ((i = is_empty(txt_data, end)) != 0)
			beg += i;
		else if (is_hrule(txt_data, end)) {
			if (rndr->make.hrule)
				rndr->make.hrule(ob, rndr->make.opaque);
			while (beg < size && data[beg] != '\n') beg += 1;
			beg += 1; }
		else if (prefix_quote(txt_data, end))
			beg += parse_blockquote(ob, rndr, txt_data, end);
		else if (prefix_code(txt_data, end))
			beg += parse_blockcode(ob, rndr, txt_data, end);
		else if (prefix_uli(txt_data, end))
			beg += parse_list(ob, rndr, txt_data, end, 0);
		else if (prefix_oli(txt_data, end))
			beg += parse_list(ob, rndr, txt_data, end,
						MKD_LIST_ORDERED);
		else if (has_table && is_tableline(txt_data, end))
			beg += parse_table(ob, rndr, txt_data, end);
		else
			beg += parse_paragraph(ob, rndr, txt_data, end); } }





static int
is_ref(char *data, size_t beg, size_t end, size_t *last, struct array *refs) {
	size_t i = 0;
	size_t id_offset, id_end;
	size_t link_offset, link_end;
	size_t title_offset, title_end;
	size_t line_end;
	struct link_ref *lr;
	struct buf *id;


	if (beg + 3 >= end) return 0;
	if (data[beg] == ' ') { i = 1;
	if (data[beg + 1] == ' ') { i = 2;
	if (data[beg + 2] == ' ') { i = 3;
	if (data[beg + 3] == ' ') return 0; } } }
	i += beg;


	if (data[i] != '[') return 0;
	i += 1;
	id_offset = i;
	while (i < end && data[i] != '\n' && data[i] != '\r' && data[i] != ']')
		i += 1;
	if (i >= end || data[i] != ']') return 0;
	id_end = i;


	i += 1;
	if (i >= end || data[i] != ':') return 0;
	i += 1;
	while (i < end && (data[i] == ' ' || data[i] == '\t')) i += 1;
	if (i < end && (data[i] == '\n' || data[i] == '\r')) {
		i += 1;
		if (i < end && data[i] == '\r' && data[i - 1] == '\n') i += 1; }
	while (i < end && (data[i] == ' ' || data[i] == '\t')) i += 1;
	if (i >= end) return 0;


	if (data[i] == '<') i += 1;
	link_offset = i;
	while (i < end && data[i] != ' ' && data[i] != '\t'
			&& data[i] != '\n' && data[i] != '\r') i += 1;
	if (data[i - 1] == '>') link_end = i - 1;
	else link_end = i;


	while (i < end && (data[i] == ' ' || data[i] == '\t')) i += 1;
	if (i < end && data[i] != '\n' && data[i] != '\r'
			&& data[i] != '\'' && data[i] != '"' && data[i] != '(')
		return 0;
	line_end = 0;

	if (i >= end || data[i] == '\r' || data[i] == '\n') line_end = i;
	if (i + 1 < end && data[i] == '\n' && data[i + 1] == '\r')
		line_end = i + 1;


	if (line_end) {
		i = line_end + 1;
		while (i < end && (data[i] == ' ' || data[i] == '\t')) i += 1; }

	title_offset = title_end = 0;
	if (i + 1 < end
	&& (data[i] == '\'' || data[i] == '"' || data[i] == '(')) {
		i += 1;
		title_offset = i;

		while (i < end && data[i] != '\n' && data[i] != '\r') i += 1;
		if (i + 1 < end && data[i] == '\n' && data[i + 1] == '\r')
			title_end = i + 1;
		else	title_end = i;

		i -= 1;
		while (i > title_offset && (data[i] == ' ' || data[i] == '\t'))
			i -= 1;
		if (i > title_offset
		&& (data[i] == '\'' || data[i] == '"' || data[i] == ')')) {
			line_end = title_end;
			title_end = i; } }
	if (!line_end) return 0;


	if (last) *last = line_end;
	if (!refs) return 1;
	id = bufnew(WORK_UNIT);
	if (build_ref_id(id, data + id_offset, id_end - id_offset) < 0) {
		bufrelease(id);
		return 0; }
	lr = arr_item(refs, arr_newitem(refs));
	lr->id = id;
	lr->link = bufnew(link_end - link_offset);
	bufput(lr->link, data + link_offset, link_end - link_offset);
	if (title_end > title_offset) {
		lr->title = bufnew(title_end - title_offset);
		bufput(lr->title, data + title_offset,
					title_end - title_offset); }
	else lr->title = 0;
	return 1; }





void
markdown(struct buf *ob, struct buf *ib, const struct mkd_renderer *rndrer) {
	struct link_ref *lr;
	struct buf *text;
	size_t i, beg, end;
	struct render rndr;


	if (!rndrer) return;
	rndr.make = *rndrer;
	if (rndr.make.max_work_stack < 1)
		rndr.make.max_work_stack = 1;
	arr_init(&rndr.refs, sizeof (struct link_ref));
	parr_init(&rndr.work);
	for (i = 0; i < 256; i += 1) rndr.active_char[i] = 0;
	if ((rndr.make.emphasis || rndr.make.double_emphasis
						|| rndr.make.triple_emphasis)
	&& rndr.make.emph_chars)
		for (i = 0; rndr.make.emph_chars[i]; i += 1)
			rndr.active_char[(unsigned char)rndr.make.emph_chars[i]]
				= char_emphasis;
	if (rndr.make.codespan) rndr.active_char['`'] = char_codespan;
	if (rndr.make.linebreak) rndr.active_char['\n'] = char_linebreak;
	if (rndr.make.image || rndr.make.link)
		rndr.active_char['['] = char_link;
	rndr.active_char['<'] = char_langle_tag;
	rndr.active_char['\\'] = char_escape;
	rndr.active_char['&'] = char_entity;


	text = bufnew(TEXT_UNIT);
	beg = 0;
	while (beg < ib->size)
		if (is_ref(ib->data, beg, ib->size, &end, &rndr.refs))
			beg = end;
		else {
			end = beg;
			while (end < ib->size
			&& ib->data[end] != '\n' && ib->data[end] != '\r')
				end += 1;

			if (end > beg) bufput(text, ib->data + beg, end - beg);
			while (end < ib->size
			&& (ib->data[end] == '\n' || ib->data[end] == '\r')) {

				if (ib->data[end] == '\n'
				|| (end + 1 < ib->size
						&& ib->data[end + 1] != '\n'))
					bufputc(text, '\n');
				end += 1; }
			beg = end; }


	if (rndr.refs.size)
		qsort(rndr.refs.base, rndr.refs.size, rndr.refs.unit,
					cmp_link_ref_sort);


	if (text->size
	&&  text->data[text->size - 1] != '\n'
	&&  text->data[text->size - 1] != '\r')
		bufputc(text, '\n');


	if (rndr.make.prolog)
		rndr.make.prolog(ob, rndr.make.opaque);
	parse_block(ob, &rndr, text->data, text->size);
	if (rndr.make.epilog)
		rndr.make.epilog(ob, rndr.make.opaque);


	bufrelease(text);
	lr = rndr.refs.base;
	for (i = 0; i < rndr.refs.size; i += 1) {
		bufrelease(lr[i].id);
		bufrelease(lr[i].link);
		bufrelease(lr[i].title); }
	arr_free(&rndr.refs);
	assert(rndr.work.size == 0);
	for (i = 0; i < rndr.work.asize; i += 1)
		bufrelease(rndr.work.item[i]);
	parr_free(&rndr.work); }










void
lus_attr_escape(struct buf *ob, const char *src, size_t size) {
	size_t  i = 0, org;
	while (i < size) {

		org = i;
		while (i < size && src[i] != '<' && src[i] != '>'
		&& src[i] != '&' && src[i] != '"')
			i += 1;
		if (i > org) bufput(ob, src + org, i - org);


		if (i >= size) break;
		else if (src[i] == '<') BUFPUTSL(ob, "&lt;");
		else if (src[i] == '>') BUFPUTSL(ob, "&gt;");
		else if (src[i] == '&') BUFPUTSL(ob, "&amp;");
		else if (src[i] == '"') BUFPUTSL(ob, "&quot;");
		i += 1; } }



void
lus_body_escape(struct buf *ob, const char *src, size_t size) {
	size_t  i = 0, org;
	while (i < size) {

		org = i;
		while (i < size && src[i] != '<' && src[i] != '>'
		&& src[i] != '&')
			i += 1;
		if (i > org) bufput(ob, src + org, i - org);


		if (i >= size) break;
		else if (src[i] == '<') BUFPUTSL(ob, "&lt;");
		else if (src[i] == '>') BUFPUTSL(ob, "&gt;");
		else if (src[i] == '&') BUFPUTSL(ob, "&amp;");
		i += 1; } }




static int
rndr_autolink(struct buf *ob, struct buf *link, enum mkd_autolink type,
						void *opaque) {
	if (!link || !link->size) return 0;
	BUFPUTSL(ob, "<a href=\"");
	if (type == MKDA_IMPLICIT_EMAIL) BUFPUTSL(ob, "mailto:");
	lus_attr_escape(ob, link->data, link->size);
	BUFPUTSL(ob, "\">");
	if (type == MKDA_EXPLICIT_EMAIL && link->size > 7)
		lus_body_escape(ob, link->data + 7, link->size - 7);
	else	lus_body_escape(ob, link->data, link->size);
	BUFPUTSL(ob, "</a>");
	return 1; }

static void
rndr_blockcode(struct buf *ob, struct buf *text, void *opaque) {
	if (ob->size) bufputc(ob, '\n');
	BUFPUTSL(ob, "<pre><code>");
	if (text) lus_body_escape(ob, text->data, text->size);
	BUFPUTSL(ob, "</code></pre>\n"); }

static void
rndr_blockquote(struct buf *ob, struct buf *text, void *opaque) {
	if (ob->size) bufputc(ob, '\n');
	BUFPUTSL(ob, "<blockquote>\n");
	if (text) bufput(ob, text->data, text->size);
	BUFPUTSL(ob, "</blockquote>\n"); }

static int
rndr_codespan(struct buf *ob, struct buf *text, void *opaque) {
	BUFPUTSL(ob, "<code>");
	if (text) lus_body_escape(ob, text->data, text->size);
	BUFPUTSL(ob, "</code>");
	return 1; }

static int
rndr_double_emphasis(struct buf *ob, struct buf *text, char c, void *opaque) {
	if (!text || !text->size) return 0;
	BUFPUTSL(ob, "<strong>");
	bufput(ob, text->data, text->size);
	BUFPUTSL(ob, "</strong>");
	return 1; }

static int
rndr_emphasis(struct buf *ob, struct buf *text, char c, void *opaque) {
	if (!text || !text->size) return 0;
	BUFPUTSL(ob, "<em>");
	if (text) bufput(ob, text->data, text->size);
	BUFPUTSL(ob, "</em>");
	return 1; }

static void
rndr_header(struct buf *ob, struct buf *text, int level, void *opaque) {
	if (ob->size) bufputc(ob, '\n');
	bufprintf(ob, "<h%d>", level);
	if (text) bufput(ob, text->data, text->size);
	bufprintf(ob, "</h%d>\n", level); }

static int
rndr_link(struct buf *ob, struct buf *link, struct buf *title,
			struct buf *content, void *opaque) {
	BUFPUTSL(ob, "<a href=\"");
	if (link && link->size) lus_attr_escape(ob, link->data, link->size);
	if (title && title->size) {
		BUFPUTSL(ob, "\" title=\"");
		lus_attr_escape(ob, title->data, title->size); }
	BUFPUTSL(ob, "\">");
	if (content && content->size) bufput(ob, content->data, content->size);
	BUFPUTSL(ob, "</a>");
	return 1; }

static void
rndr_list(struct buf *ob, struct buf *text, int flags, void *opaque) {
	if (ob->size) bufputc(ob, '\n');
	bufput(ob, (flags & MKD_LIST_ORDERED) ? "<ol>\n" : "<ul>\n", 5);
	if (text) bufput(ob, text->data, text->size);
	bufput(ob, (flags & MKD_LIST_ORDERED) ? "</ol>\n" : "</ul>\n", 6); }

static void
rndr_listitem(struct buf *ob, struct buf *text, int flags, void *opaque) {
	BUFPUTSL(ob, "<li>");
	if (text) {
		while (text->size && text->data[text->size - 1] == '\n')
			text->size -= 1;
		bufput(ob, text->data, text->size); }
	BUFPUTSL(ob, "</li>\n"); }

static void
rndr_normal_text(struct buf *ob, struct buf *text, void *opaque) {
	if (text) lus_body_escape(ob, text->data, text->size); }

static void
rndr_paragraph(struct buf *ob, struct buf *text, void *opaque) {
	if (ob->size) bufputc(ob, '\n');
	BUFPUTSL(ob, "<p>");
	if (text) bufput(ob, text->data, text->size);
	BUFPUTSL(ob, "</p>\n"); }

static void
rndr_raw_block(struct buf *ob, struct buf *text, void *opaque) {
	size_t org, sz;
	if (!text) return;
	sz = text->size;
	while (sz > 0 && text->data[sz - 1] == '\n') sz -= 1;
	org = 0;
	while (org < sz && text->data[org] == '\n') org += 1;
	if (org >= sz) return;
	if (ob->size) bufputc(ob, '\n');
	bufput(ob, text->data + org, sz - org);
	bufputc(ob, '\n'); }

static int
rndr_raw_inline(struct buf *ob, struct buf *text, void *opaque) {
	bufput(ob, text->data, text->size);
	return 1; }

static int
rndr_triple_emphasis(struct buf *ob, struct buf *text, char c, void *opaque) {
	if (!text || !text->size) return 0;
	BUFPUTSL(ob, "<strong><em>");
	bufput(ob, text->data, text->size);
	BUFPUTSL(ob, "</em></strong>");
	return 1; }




static void
html_hrule(struct buf *ob, void *opaque) {
	if (ob->size) bufputc(ob, '\n');
	BUFPUTSL(ob, "<hr>\n"); }

static int
html_image(struct buf *ob, struct buf *link, struct buf *title,
			struct buf *alt, void *opaque) {
	if (!link || !link->size) return 0;
	BUFPUTSL(ob, "<img src=\"");
	lus_attr_escape(ob, link->data, link->size);
	BUFPUTSL(ob, "\" alt=\"");
	if (alt && alt->size)
		lus_attr_escape(ob, alt->data, alt->size);
	if (title && title->size) {
		BUFPUTSL(ob, "\" title=\"");
		lus_attr_escape(ob, title->data, title->size); }
	BUFPUTSL(ob, "\">");
	return 1; }

static int
html_linebreak(struct buf *ob, void *opaque) {
	BUFPUTSL(ob, "<br>\n");
	return 1; }



const struct mkd_renderer mkd_html = {
	NULL,
	NULL,

	rndr_blockcode,
	rndr_blockquote,
	rndr_raw_block,
	rndr_header,
	html_hrule,
	rndr_list,
	rndr_listitem,
	rndr_paragraph,
	NULL,
	NULL,
	NULL,

	rndr_autolink,
	rndr_codespan,
	rndr_double_emphasis,
	rndr_emphasis,
	html_image,
	html_linebreak,
	rndr_link,
	rndr_raw_inline,
	rndr_triple_emphasis,

	NULL,
	rndr_normal_text,

	64,
	"*_",
	NULL };




static void
xhtml_hrule(struct buf *ob, void *opaque) {
	if (ob->size) bufputc(ob, '\n');
	BUFPUTSL(ob, "<hr />\n"); }

static int
xhtml_image(struct buf *ob, struct buf *link, struct buf *title,
			struct buf *alt, void *opaque) {
	if (!link || !link->size) return 0;
	BUFPUTSL(ob, "<img src=\"");
	lus_attr_escape(ob, link->data, link->size);
	BUFPUTSL(ob, "\" alt=\"");
	if (alt && alt->size)
		lus_attr_escape(ob, alt->data, alt->size);
	if (title && title->size) {
		BUFPUTSL(ob, "\" title=\"");
		lus_attr_escape(ob, title->data, title->size); }
	BUFPUTSL(ob, "\" />");
	return 1; }

static int
xhtml_linebreak(struct buf *ob, void *opaque) {
	BUFPUTSL(ob, "<br />\n");
	return 1; }



const struct mkd_renderer mkd_xhtml = {
	NULL,
	NULL,

	rndr_blockcode,
	rndr_blockquote,
	rndr_raw_block,
	rndr_header,
	xhtml_hrule,
	rndr_list,
	rndr_listitem,
	rndr_paragraph,
	NULL,
	NULL,
	NULL,

	rndr_autolink,
	rndr_codespan,
	rndr_double_emphasis,
	rndr_emphasis,
	xhtml_image,
	xhtml_linebreak,
	rndr_link,
	rndr_raw_inline,
	rndr_triple_emphasis,

	NULL,
	rndr_normal_text,

	64,
	"*_",
	NULL };




static int
print_link_wxh(struct buf *ob, struct buf *link) {
	size_t eq, ex, end;
	if (link->size < 1) return 0;
	eq = link->size - 1;
	while (eq > 0 && (link->data[eq - 1] != ' ' || link->data[eq] != '='))
		eq -= 1;
	if (!eq) return 0;
	ex = eq + 1;
	while (ex < link->size
	&& link->data[ex] >= '0' && link->data[ex] <= '9')
		ex += 1;
	if (ex >= link->size || ex == eq + 1 || link->data[ex] != 'x') return 0;
	end = ex + 1;
	while (end < link->size
	&& link->data[end] >= '0' && link->data[end] <= '9')
		end += 1;
	if (end == ex + 1) return 0;

	lus_attr_escape(ob, link->data, eq - 1);
	BUFPUTSL(ob, "\" width=");
	bufput(ob, link->data + eq + 1, ex - eq - 1);
	BUFPUTSL(ob, " height=");
	bufput(ob, link->data + ex + 1, end - ex - 1);
	return 1; }

static int
discount_image(struct buf *ob, struct buf *link, struct buf *title,
			struct buf *alt, int xhtml) {
	if (!link || !link->size) return 0;
	BUFPUTSL(ob, "<img src=\"");
	if (!print_link_wxh(ob, link)) {
		lus_attr_escape(ob, link->data, link->size);
		bufputc(ob, '"'); }
	BUFPUTSL(ob, " alt=\"");
	if (alt && alt->size)
		lus_attr_escape(ob, alt->data, alt->size);
	if (title && title->size) {
		BUFPUTSL(ob, "\" title=\"");
		lus_attr_escape(ob, title->data, title->size); }
	bufputs(ob, xhtml ? "\" />" : "\">");
	return 1; }

static int
html_discount_image(struct buf *ob, struct buf *link, struct buf *title,
			struct buf *alt, void *opaque) {
	return discount_image(ob, link, title, alt, 0); }

static int
xhtml_discount_image(struct buf *ob, struct buf *link, struct buf *title,
			struct buf *alt, void *opaque) {
	return discount_image(ob, link, title, alt, 1); }

static int
discount_link(struct buf *ob, struct buf *link, struct buf *title,
			struct buf *content, void *opaque) {
	if (!link) return rndr_link(ob, link, title, content, opaque);
	else if (link->size > 5 && !strncasecmp(link->data, "abbr:", 5)) {
		BUFPUTSL(ob, "<abbr title=\"");
		lus_attr_escape(ob, link->data + 5, link->size - 5);
		BUFPUTSL(ob, "\">");
		bufput(ob, content->data, content->size);
		BUFPUTSL(ob, "</abbr>");
		return 1; }
	else if (link->size > 6 && !strncasecmp(link->data, "class:", 6)) {
		BUFPUTSL(ob, "<span class=\"");
		lus_attr_escape(ob, link->data + 6, link->size - 6);
		BUFPUTSL(ob, "\">");
		bufput(ob, content->data, content->size);
		BUFPUTSL(ob, "</span>");
		return 1; }
	else if (link->size > 3 && !strncasecmp(link->data, "id:", 3)) {
		BUFPUTSL(ob, "<span id=\"");
		lus_attr_escape(ob, link->data + 3, link->size - 3);
		BUFPUTSL(ob, "\">");
		bufput(ob, content->data, content->size);
		BUFPUTSL(ob, "</span>");
		return 1; }
	else if (link->size > 4 && !strncasecmp(link->data, "raw:", 4)) {
		bufput(ob, link->data + 4, link->size - 4);
		return 1; }
	return rndr_link(ob, link, title, content, opaque); }

static void
discount_blockquote(struct buf *ob, struct buf *text, void *opaque) {
	size_t i = 5, size = text->size;
	char *data = text->data;
	if (text->size < 5 || strncasecmp(text->data, "<p>%", 4)) {
		rndr_blockquote(ob, text, opaque);
		return; }
	while (i < size && data[i] != '\n' && data[i] != '%')
		i += 1;
	if (i >= size || data[i] != '%') {
		rndr_blockquote(ob, text, opaque);
		return; }
	BUFPUTSL(ob, "<div class=\"");
	bufput(ob, text->data + 4, i - 4);
	BUFPUTSL(ob, "\"><p>");
	i += 1;
	if (i + 4 >= text->size && !strncasecmp(text->data + i, "</p>", 4)) {
		size_t old_i = i;
		i += 4;
		while (i + 3 < text->size
		&& (data[i] != '<' || data[i + 1] != 'p' || data[i + 2] != '>'))
			i += 1;
		if (i + 3 >= text->size) i = old_i; }
	bufput(ob, text->data + i, text->size - i);
	BUFPUTSL(ob, "</div>\n"); }

static void
discount_table(struct buf *ob, struct buf *head_row, struct buf *rows,
					void *opaque) {
	if (ob->size) bufputc(ob, '\n');
	BUFPUTSL(ob, "<table>\n");
	if (head_row) {
		BUFPUTSL(ob, "<thead>\n");
		bufput(ob, head_row->data, head_row->size);
		BUFPUTSL(ob, "</thead>\n<tbody>\n"); }
	if (rows)
		bufput(ob, rows->data, rows->size);
	if (head_row)
		BUFPUTSL(ob, "</tbody>\n");
	BUFPUTSL(ob, "</table>\n"); }

static void
discount_table_row(struct buf *ob, struct buf *cells, int flags, void *opaque){
	(void)flags;
	BUFPUTSL(ob, "  <tr>\n");
	if (cells) bufput(ob, cells->data, cells->size);
	BUFPUTSL(ob, "  </tr>\n"); }

static void
discount_table_cell(struct buf *ob, struct buf *text, int flags, void *opaque){
	if (flags & MKD_CELL_HEAD)
		BUFPUTSL(ob, "    <th");
	else
		BUFPUTSL(ob, "    <td");
	switch (flags & MKD_CELL_ALIGN_MASK) {
		case MKD_CELL_ALIGN_LEFT:
			BUFPUTSL(ob, " align=\"left\"");
			break;
		case MKD_CELL_ALIGN_RIGHT:
			BUFPUTSL(ob, " align=\"right\"");
			break;
		case MKD_CELL_ALIGN_CENTER:
			BUFPUTSL(ob, " align=\"center\"");
			break; }
	bufputc(ob, '>');
	if (text) bufput(ob, text->data, text->size);
	if (flags & MKD_CELL_HEAD)
		BUFPUTSL(ob, "</th>\n");
	else
		BUFPUTSL(ob, "</td>\n"); }


const struct mkd_renderer discount_html = {
	NULL,
	NULL,

	rndr_blockcode,
	discount_blockquote,
	rndr_raw_block,
	rndr_header,
	html_hrule,
	rndr_list,
	rndr_listitem,
	rndr_paragraph,
	discount_table,
	discount_table_cell,
	discount_table_row,

	rndr_autolink,
	rndr_codespan,
	rndr_double_emphasis,
	rndr_emphasis,
	html_discount_image,
	html_linebreak,
	discount_link,
	rndr_raw_inline,
	rndr_triple_emphasis,

	NULL,
	rndr_normal_text,

	64,
	"*_",
	NULL };
const struct mkd_renderer discount_xhtml = {
	NULL,
	NULL,

	rndr_blockcode,
	discount_blockquote,
	rndr_raw_block,
	rndr_header,
	xhtml_hrule,
	rndr_list,
	rndr_listitem,
	rndr_paragraph,
	discount_table,
	discount_table_cell,
	discount_table_row,

	rndr_autolink,
	rndr_codespan,
	rndr_double_emphasis,
	rndr_emphasis,
	xhtml_discount_image,
	xhtml_linebreak,
	discount_link,
	rndr_raw_inline,
	rndr_triple_emphasis,

	NULL,
	rndr_normal_text,

	64,
	"*_",
	NULL };



static void
nat_span(struct buf *ob, struct buf *text, char *tag) {
	bufprintf(ob, "<%s>", tag);
	bufput(ob, text->data, text->size);
	bufprintf(ob, "</%s>", tag); }

static int
nat_emphasis(struct buf *ob, struct buf *text, char c, void *opaque) {
	if (!text || !text->size || c == '+' || c == '-') return 0;
	if (c == '|') nat_span(ob, text, "span");
	else nat_span(ob, text, "em");
	return 1; }

static int
nat_double_emphasis(struct buf *ob, struct buf *text, char c, void *opaque) {
	if (!text || !text->size || c == '|') return 0;
	if (c == '+') nat_span(ob, text, "ins");
	else if (c == '-') nat_span(ob, text, "del");
	else nat_span(ob, text, "strong");
	return 1; }

static int
nat_triple_emphasis(struct buf *ob, struct buf *text, char c, void *opaque) {
	if (!text || !text->size || c == '+' || c == '-' || c == '|') return 0;
	BUFPUTSL(ob, "<strong><em>");
	bufput(ob, text->data, text->size);
	BUFPUTSL(ob, "</em></strong>");
	return 1; }

static void
nat_header(struct buf *ob, struct buf *text, int level, void *opaque) {
	size_t i = 0;
	if (ob->size) bufputc(ob, '\n');
	while (i < text->size && (text->data[i] == '-' || text->data[i] == '_'
			 ||  text->data[i] == '.' || text->data[i] == ':'
			 || (text->data[i] >= 'a' && text->data[i] <= 'z')
			 || (text->data[i] >= 'A' && text->data[i] <= 'Z')
			 || (text->data[i] >= '0' && text->data[i] <= '9')))
		i += 1;
	bufprintf(ob, "<h%d", level);
	if (i < text->size && text->data[i] == '#') {
		bufprintf(ob, " id=\"%.*s\">", (int)i, text->data);
		i += 1; }
	else {
		bufputc(ob, '>');
		i = 0; }
	bufput(ob, text->data + i, text->size - i);
	bufprintf(ob, "</h%d>\n", level); }

static void
nat_paragraph(struct buf *ob, struct buf *text, void *opaque) {
	size_t i = 0;
	if (ob->size) bufputc(ob, '\n');
	BUFPUTSL(ob, "<p");
	if (text && text->size && text->data[0] == '(') {
		i = 1;
		while (i < text->size && (text->data[i] == ' '


			 || (text->data[i] >= 'a' && text->data[i] <= 'z')
			 || (text->data[i] >= 'A' && text->data[i] <= 'Z')
			 || (text->data[i] >= '0' && text->data[i] <= '9')))
			i += 1;
		if (i < text->size && text->data[i] == ')') {
			bufprintf(ob, " class=\"%.*s\"",
						(int)(i - 1), text->data + 1);
			i += 1; }
		else i = 0; }
	bufputc(ob, '>');
	if (text) bufput(ob, text->data + i, text->size - i);
	BUFPUTSL(ob, "</p>\n"); }



const struct mkd_renderer nat_html = {
	NULL,
	NULL,

	rndr_blockcode,
	discount_blockquote,
	rndr_raw_block,
	nat_header,
	html_hrule,
	rndr_list,
	rndr_listitem,
	nat_paragraph,
	NULL,
	NULL,
	NULL,

	rndr_autolink,
	rndr_codespan,
	nat_double_emphasis,
	nat_emphasis,
	html_discount_image,
	html_linebreak,
	discount_link,
	rndr_raw_inline,
	nat_triple_emphasis,

	NULL,
	rndr_normal_text,

	64,
	"*_-+|",
	NULL };
const struct mkd_renderer nat_xhtml = {
	NULL,
	NULL,

	rndr_blockcode,
	discount_blockquote,
	rndr_raw_block,
	nat_header,
	xhtml_hrule,
	rndr_list,
	rndr_listitem,
	nat_paragraph,
	NULL,
	NULL,
	NULL,

	rndr_autolink,
	rndr_codespan,
	nat_double_emphasis,
	nat_emphasis,
	xhtml_discount_image,
	xhtml_linebreak,
	discount_link,
	rndr_raw_inline,
	nat_triple_emphasis,

	NULL,
	rndr_normal_text,

	64,
	"*_-+|",
	NULL };
