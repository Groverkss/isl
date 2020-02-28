/*
 * Copyright 2011      INRIA Saclay
 * Copyright 2012      Ecole Normale Superieure
 *
 * Use of this software is governed by the MIT license
 *
 * Written by Sven Verdoolaege, INRIA Saclay - Ile-de-France,
 * Parc Club Orsay Universite, ZAC des vignes, 4 rue Jacques Monod,
 * 91893 Orsay, France
 * and Ecole Normale Superieure, 45 rue d'Ulm, 75230 Paris, France
 */

#include <isl_pw_macro.h>

/* Given a function "cmp" that returns the set of elements where
 * "el1" is "better" than "el2", return this set.
 */
static __isl_give isl_set *FN(PW,better)(__isl_keep EL *el1, __isl_keep EL *el2,
	__isl_give isl_set *(*cmp)(__isl_take EL *el1, __isl_take EL *el2))
{
	return cmp(FN(EL,copy)(el1), FN(EL,copy)(el2));
}

/* Return a list containing the domains of the pieces of "pw".
 */
static __isl_give isl_set_list *FN(PW,extract_domains)(__isl_keep PW *pw)
{
	int i;
	isl_ctx *ctx;
	isl_set_list *list;

	if (!pw)
		return NULL;
	ctx = FN(PW,get_ctx)(pw);
	list = isl_set_list_alloc(ctx, pw->n);
	for (i = 0; i < pw->n; ++i)
		list = isl_set_list_add(list, isl_set_copy(pw->p[i].set));

	return list;
}

/* Given sets B ("set"), C ("better") and A' ("out"), return
 *
 *	(B \cap C) \cup ((B \setminus C) \setminus A')
 */
static __isl_give isl_set *FN(PW,better_or_out)(__isl_take isl_set *set,
	__isl_take isl_set *better, __isl_take isl_set *out)
{
	isl_set *set_better, *set_out;

	set_better = isl_set_intersect(isl_set_copy(set), isl_set_copy(better));
	set_out = isl_set_subtract(isl_set_subtract(set, better), out);

	return isl_set_union(set_better, set_out);
}

/* Given sets A ("set"), C ("better") and B' ("out"), return
 *
 *	(A \setminus C) \cup ((A \cap C) \setminus B')
 */
static __isl_give isl_set *FN(PW,worse_or_out)(__isl_take isl_set *set,
	__isl_take isl_set *better, __isl_take isl_set *out)
{
	isl_set *set_worse, *set_out;

	set_worse = isl_set_subtract(isl_set_copy(set), isl_set_copy(better));
	set_out = isl_set_subtract(isl_set_intersect(set, better), out);

	return isl_set_union(set_worse, set_out);
}

/* Internal data structure used by isl_pw_*_union_opt_cmp
 * that keeps track of a piecewise expression with updated cells.
 * "pw" holds the original piecewise expression.
 * "list" holds the updated cells.
 */
S(PW,union_opt_cmp_data) {
	PW *pw;
	isl_set_list *cell;
};

/* Free all memory allocated for "data".
 */
static void FN(PW,union_opt_cmp_data_clear)(S(PW,union_opt_cmp_data) *data)
{
	isl_set_list_free(data->cell);
	FN(PW,free)(data->pw);
}

/* Given two piecewise expressions data1->pw and data2->pw, replace
 * their domains
 * by the sets in data1->cell and data2->cell and combine the results into
 * a single piecewise expression.
 * The pieces of data1->pw and data2->pw are assumed to have been sorted
 * according to the function value expressions.
 * The pieces of the result are also sorted in this way.
 *
 * Run through the pieces of data1->pw and data2->pw in order until they
 * have both been exhausted, picking the piece from data1->pw or data2->pw
 * depending on which should come first, together with the corresponding
 * domain from data1->cell or data2->cell.  In cases where the next pieces
 * in both data1->pw and data2->pw have the same function value expression,
 * construct only a single piece in the result with as domain
 * the union of the domains in data1->cell and data2->cell.
 */
static __isl_give PW *FN(PW,merge)(S(PW,union_opt_cmp_data) *data1,
	S(PW,union_opt_cmp_data) *data2)
{
	int i, j;
	PW *res;
	PW *pw1 = data1->pw;
	PW *pw2 = data2->pw;
	isl_set_list *list1 = data1->cell;
	isl_set_list *list2 = data2->cell;

	if (!pw1 || !pw2)
		return NULL;

	res = FN(PW,alloc_size)(isl_space_copy(pw1->dim), pw1->n + pw2->n);

	i = 0; j = 0;
	while (i < pw1->n || j < pw2->n) {
		int cmp;
		isl_set *set;
		EL *el;

		if (i < pw1->n && j < pw2->n)
			cmp = FN(EL,plain_cmp)(pw1->p[i].FIELD,
						pw2->p[j].FIELD);
		else
			cmp = i < pw1->n ? -1 : 1;

		if (cmp < 0) {
			set = isl_set_list_get_set(list1, i);
			el = FN(EL,copy)(pw1->p[i].FIELD);
			++i;
		} else if (cmp > 0) {
			set = isl_set_list_get_set(list2, j);
			el = FN(EL,copy)(pw2->p[j].FIELD);
			++j;
		} else {
			set = isl_set_union(isl_set_list_get_set(list1, i),
					    isl_set_list_get_set(list2, j));
			el = FN(EL,copy)(pw1->p[i].FIELD);
			++i;
			++j;
		}
		res = FN(PW,add_piece)(res, set, el);
	}

	return res;
}

/* Given a function "cmp" that returns the set of elements where
 * "el1" is "better" than "el2", return a piecewise
 * expression defined on the union of the definition domains
 * of "pw1" and "pw2" that maps to the "best" of "pw1" and
 * "pw2" on each cell.  If only one of the two input functions
 * is defined on a given cell, then it is considered the best.
 *
 * Run through all pairs of pieces in "pw1" and "pw2".
 * If the domains of these pieces intersect, then the intersection
 * needs to be distributed over the two pieces based on "cmp".
 * Let C be the set where the piece from "pw2" is better (according to "cmp")
 * than the piece from "pw1".  Let A be the domain of the piece from "pw1" and
 * B the domain of the piece from "pw2".
 *
 * The elements in C need to be removed from A, except for those parts
 * that lie outside of B.  That is,
 *
 *	A <- (A \setminus C) \cup ((A \cap C) \setminus B')
 *
 * Conversely, the elements in B need to be restricted to C, except
 * for those parts that lie outside of A.  That is
 *
 *	B <- (B \cap C) \cup ((B \setminus C) \setminus A')
 *
 * Since all pairs of pieces are considered, the domains are updated
 * several times.  A and B refer to these updated domains
 * (kept track of in data[0].cell and data[1].cell), while A' and B' refer
 * to the original domains of the pieces.  It is safe to use these
 * original domains because the difference between, say, A' and A is
 * the domains of pw2-pieces that have been removed before and
 * those domains are disjoint from B.  A' is used instead of A
 * because the continued updating of A may result in this domain
 * getting broken up into more disjuncts.
 *
 * After the updated domains have been computed, the result is constructed
 * from "pw1", "pw2", data[0].cell and data[1].cell.  If there are any pieces
 * in "pw1" and "pw2" with the same function value expression, then
 * they are combined into a single piece in the result.
 * In order to be able to do this efficiently, the pieces of "pw1" and
 * "pw2" are first sorted according to their function value expressions.
 */
static __isl_give PW *FN(PW,union_opt_cmp)(
	__isl_take PW *pw1, __isl_take PW *pw2,
	__isl_give isl_set *(*cmp)(__isl_take EL *el1, __isl_take EL *el2))
{
	S(PW,union_opt_cmp_data) data[2] = { { pw1, NULL }, { pw2, NULL } };
	int i, j;
	isl_size n1, n2;
	PW *res = NULL;
	isl_ctx *ctx;

	if (!pw1 || !pw2)
		goto error;

	ctx = isl_space_get_ctx(pw1->dim);
	if (!isl_space_is_equal(pw1->dim, pw2->dim))
		isl_die(ctx, isl_error_invalid,
			"arguments should live in the same space", goto error);

	if (FN(PW,is_empty)(pw1)) {
		FN(PW,free)(pw1);
		return pw2;
	}

	if (FN(PW,is_empty)(pw2)) {
		FN(PW,free)(pw2);
		return pw1;
	}

	for (i = 0; i < 2; ++i) {
		data[i].pw = FN(PW,sort)(data[i].pw);
		data[i].cell = FN(PW,extract_domains)(data[i].pw);
	}

	n1 = FN(PW,n_piece)(data[0].pw);
	n2 = FN(PW,n_piece)(data[1].pw);
	if (n1 < 0 || n2 < 0)
		goto error;
	for (i = 0; i < n1; ++i) {
		for (j = 0; j < n2; ++j) {
			isl_bool disjoint;
			isl_set *better, *set_i, *set_j;
			EL *el_i, *el_j;

			set_i = FN(PW,peek_domain_at)(data[0].pw, i);
			set_j = FN(PW,peek_domain_at)(data[1].pw, j);
			disjoint = isl_set_is_disjoint(set_i, set_j);
			if (disjoint < 0)
				goto error;
			if (disjoint)
				continue;
			el_i = FN(PW,peek_base_at)(data[0].pw, i);
			el_j = FN(PW,peek_base_at)(data[1].pw, j);
			better = FN(PW,better)(el_j, el_i, cmp);
			set_i = isl_set_list_get_set(data[0].cell, i);
			set_j = FN(PW,get_domain_at)(data[1].pw, j);
			set_i = FN(PW,worse_or_out)(set_i,
						isl_set_copy(better), set_j);
			data[0].cell =
				isl_set_list_set_set(data[0].cell, i, set_i);
			set_i = FN(PW,get_domain_at)(data[0].pw, i);
			set_j = isl_set_list_get_set(data[1].cell, j);
			set_j = FN(PW,better_or_out)(set_j, better, set_i);
			data[1].cell =
				isl_set_list_set_set(data[1].cell, j, set_j);
		}
	}

	res = FN(PW,merge)(&data[0], &data[1]);
	for (i = 0; i < 2; ++i)
		FN(PW,union_opt_cmp_data_clear)(&data[i]);

	return res;
error:
	for (i = 0; i < 2; ++i)
		FN(PW,union_opt_cmp_data_clear)(&data[i]);
	return FN(PW,free)(res);
}
