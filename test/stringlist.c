#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "test.h"
#include "assertions.h"
#include "../stringlist.h"

static stringlist* setup_stringlist(const char *s1, const char *s2, const char *s3)
{
	stringlist *sl;

	sl = stringlist_new(NULL);
	assert_not_null("stringlist_new returns a non-null pointer", sl);
	assert_int_equals("adding the s1 string to the list", stringlist_add(sl, s1), 0);
	assert_int_equals("adding the s2 string to the list", stringlist_add(sl, s2), 0);
	assert_int_equals("adding the s3 string to the list", stringlist_add(sl, s3), 0);

	return sl;
}

void test_stringlist_init()
{
	stringlist *sl;

	sl = stringlist_new(NULL);

	test("stringlist: Initialization Routines");
	assert_not_null("stringlist_new returns a non-null pointer", sl);
	assert_int_equals("a new stringlist is empty", sl->num, 0);
	assert_int_not_equal("a new stringlist has extra capacity", sl->len, 0);

	stringlist_free(sl);
}

void test_stringlist_init_with_data()
{
	stringlist *sl;
	char *data[32];
	char buf[32];
	size_t i;

	for (i = 0; i < 32; i++) {
		snprintf(buf, 32, "data%u", i);
		data[i] = strdup(buf);
	}
	data[32] = NULL;

	sl = stringlist_new(data);
	test("stringlist: Initialization (with data)");
	assert_int_equals("stringlist should have 32 strings", sl->num, 32);
	assert_int_greater_than("stringlist should have more than 32 slots", sl->len, 32);
	assert_str_equals("spot-check strings[0]", sl->strings[0], "data0");
	assert_str_equals("spot-check strings[14]", sl->strings[14], "data14");
	assert_str_equals("spot-check strings[28]", sl->strings[28], "data28");
	assert_str_equals("spot-check strings[31]", sl->strings[31], "data31");
	assert_null("strings[32] should be a NULL terminator", sl->strings[32]);

	stringlist_free(sl);
	for (i = 0; i < 32; i++) {
		free(data[i]);
	}
}

void test_stringlist_basic_add_remove_search()
{
	stringlist *sl;

	sl = stringlist_new(NULL);

	test("stringlist: Basic Add/Remove/Search");
	assert_not_null("have a non-null stringlist", sl);

	assert_int_equals("add 'first string' to string list", stringlist_add(sl, "first string"), 0);
	assert_int_equals("add 'second string' to string list", stringlist_add(sl, "second string"), 0);

	assert_stringlist(sl, "sl", 2, "first string", "second string");

	assert_int_equals("search for 'first string' succeeds", stringlist_search(sl, "first string"), 0);
	assert_int_equals("search for 'second string' succeeds", stringlist_search(sl, "second string"), 0);
	assert_int_not_equal("search for 'no such string' fails", stringlist_search(sl, "no such string"), 0);

	assert_int_equals("removal of 'first string' succeeds", stringlist_remove(sl, "first string"), 0);
	assert_stringlist(sl, "sl", 1, "second string");

	assert_int_equals("search for 'second string' still succeeds", stringlist_search(sl, "second string"), 0);

	stringlist_free(sl);
}

void test_stringlist_add_all()
{
	stringlist *sl1, *sl2;

	sl1 = setup_stringlist("lorem", "ipsum", "dolor");
	sl2 = setup_stringlist("sit", "amet", "consectetur");

	test("stringlist: Combination of string lists");
	assert_stringlist(sl1, "pre-combine sl1", 3, "lorem", "ipsum", "dolor");
	assert_stringlist(sl2, "pre-combine sl2", 3, "sit", "amet", "consectetur");

	assert_int_equals("combine sl1 and sl2 successfully", stringlist_add_all(sl1, sl2), 0);
	assert_stringlist(sl1, "post-combine sl1", 6, "lorem", "ipsum", "dolor", "sit", "amet", "consectetur");
	assert_stringlist(sl2, "post-combine sl2", 3, "sit", "amet", "consectetur");

	stringlist_free(sl1);
	stringlist_free(sl2);
}

void test_stringlist_remove_all()
{
	stringlist *sl1, *sl2;

	sl1 = setup_stringlist("lorem", "ipsum", "dolor");
	sl2 = setup_stringlist("ipsum", "dolor", "sit");

	test("stringlist: Removal of string lists");
	assert_stringlist(sl1, "pre-remove sl1", 3, "lorem", "ipsum", "dolor");
	assert_stringlist(sl2, "pre-remove sl2", 3, "ipsum", "dolor", "sit");

	assert_int_equals("remove sl2 from sl1 successfully", stringlist_remove_all(sl1, sl2), 0);
	assert_stringlist(sl1, "post-remove sl1", 1, "lorem");
	assert_stringlist(sl2, "post-remove sl2", 3, "ipsum", "dolor", "sit");

	stringlist_free(sl1);
	stringlist_free(sl2);
}

void test_stringlist_expansion()
{
	stringlist *sl;
	size_t max, i, len;
	char buf[32];
	char assertion[128];

	sl = stringlist_new(NULL);
	test("stringlist: Automatic List Expansion");
	assert_not_null("have a non-null stringlist", sl);

	max = sl->len;
	for (i = 0; i < max * 2 + 1; i++) {
		snprintf(buf, 32, "string%u", i);
		snprintf(assertion, 128, "add 'string%u' to stringlist", i);
		assert_int_equals(assertion, stringlist_add(sl, buf), 0);

		snprintf(assertion, 128, "stringlist should have %u strings", i + 1);
		assert_int_equals(assertion, sl->num, i + 1);
	}

	assert_int_not_equal("sl->len should have changed", sl->len, max);

	stringlist_free(sl);
}

void test_stringlist_remove_nonexistent()
{
	const char *tomato = "tomato";

	stringlist *sl;
	sl = setup_stringlist("apple", "pear", "banana");

	test("stringlist: Remove non-existent");
	assert_stringlist(sl, "pre-remove sl", 3, "apple", "pear", "banana");
	assert_int_not_equal("removal of 'tomato' from stringlist fails", stringlist_remove(sl, tomato), 0);
	assert_stringlist(sl, "pre-remove sl", 3, "apple", "pear", "banana");

	stringlist_free(sl);
}

void test_stringlist_qsort()
{
	const char *a = "alice";
	const char *b = "bob";
	const char *c = "candace";

	stringlist *sl;
	test("stringlist: Sorting");
	sl = setup_stringlist(b, c, a);
	assert_stringlist(sl, "pre-sort sl", 3, "bob", "candace", "alice");

	stringlist_sort(sl, STRINGLIST_SORT_ASC);
	assert_stringlist(sl, "post-sort ASC sl", 3, "alice", "bob", "candace");

	stringlist_sort(sl, STRINGLIST_SORT_DESC);
	assert_stringlist(sl, "post-sort DESC sl", 3, "candace", "bob", "alice");

	stringlist_free(sl);
}

void test_stringlist_uniq()
{
	const char *a = "alice";
	const char *b = "bob";
	const char *c = "candace";

	stringlist *sl;

	sl = setup_stringlist(b, c, a);
	stringlist_add(sl, b);
	stringlist_add(sl, b);
	stringlist_add(sl, c);

	test("stringlist: Uniq");
	assert_stringlist(sl, "pre-uniq sl", 6, "bob", "candace", "alice", "bob", "bob", "candace");

	stringlist_uniq(sl);
	assert_stringlist(sl, "post-uniq sl", 3, "alice", "bob", "candace");

	stringlist_free(sl);
}

void test_stringlist_uniq_already()
{
	const char *a = "alice";
	const char *b = "bob";
	const char *c = "candace";

	stringlist *sl;

	sl = setup_stringlist(b, c, a);

	test("stringlist: Uniq (no changes needed)");
	assert_stringlist(sl, "pre-uniq sl", 3, "bob", "candace", "alice");

	stringlist_uniq(sl);
	assert_stringlist(sl, "post-uniq sl", 3, "alice", "bob", "candace");

	stringlist_free(sl);
}

void test_stringlist_diff()
{
	const char *a = "alice";
	const char *b = "bob";
	const char *c = "candace";
	const char *d = "david";
	const char *e = "ethan";

	stringlist *sl1, *sl2;

	sl1 = setup_stringlist(b, c, a);
	sl2 = setup_stringlist(b, c, a);

	test("stringlist: Diff");
	assert_int_not_equal("sl1 and sl2 are equivalent", stringlist_diff(sl1, sl2), 0);

	stringlist_add(sl2, d);
	assert_int_equals("after addition of 'david' to sl2, sl1 and sl2 differ", stringlist_diff(sl1, sl2), 0);

	stringlist_add(sl1, e);
	assert_int_equals("after addition of 'ethan' to sl1, sl1 and sl2 differ", stringlist_diff(sl1, sl2), 0);

	stringlist_free(sl1);
	stringlist_free(sl2);
}

void test_stringlist_diff_non_uniq()
{
	const char *s1 = "string1";
	const char *s2 = "string2";
	const char *s3 = "string3";

	stringlist *sl1, *sl2;

	sl1 = setup_stringlist(s1, s2, s2);
	sl2 = setup_stringlist(s1, s2, s3);

	test("stringlist: Diff (non-unique entries)");
	assert_int_equals("sl1 and sl2 are different (forward)", stringlist_diff(sl1, sl2), 0);
	assert_int_equals("sl2 and sl1 are different (reverse)", stringlist_diff(sl2, sl1), 0);

	stringlist_free(sl1);
	stringlist_free(sl2);
}

void test_stringlist_diff_single_string()
{
	stringlist *sl1, *sl2;
	const char *a = "string a";
	const char *b = "string b";

	sl1 = stringlist_new(NULL);
	stringlist_add(sl1, a);

	sl2 = stringlist_new(NULL);
	stringlist_add(sl2, b);

	test("stringlist: Diff (single string)");
	assert_int_equals("sl1 and sl2 are different (forward)", stringlist_diff(sl1, sl2), 0);
	assert_int_equals("sl2 and sl1 are different (reverse)", stringlist_diff(sl2, sl1), 0);

	stringlist_free(sl1);
	stringlist_free(sl2);
}

void test_suite_stringlist()
{
	test_stringlist_init();
	test_stringlist_init_with_data();
	test_stringlist_basic_add_remove_search();
	test_stringlist_add_all();
	test_stringlist_remove_all();
	test_stringlist_expansion();
	test_stringlist_qsort();
	test_stringlist_uniq();
	test_stringlist_uniq_already();

	test_stringlist_diff();
	test_stringlist_diff_non_uniq();
	test_stringlist_diff_single_string();
}
