#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>

#include "interval_tree.h"

#define	printk	printf
#define	KERN_ALERT	""
#define	u32	uint32_t
#define	cycles_t	time_t
#define	get_cycles()	time(NULL)
#define	div_u64(numerator, denominator)	(numerator / denominator)
#define	prandom_seed_state(arg1, arg2)	srand((unsigned int)arg2)
#define	prandom_u32_state(arg1)	rand()
struct rnd_state {
	unsigned long	val;
};

#define NODES        100
#define PERF_LOOPS   100000
#define SEARCHES     100
#define SEARCH_LOOPS 10000

static struct rb_root root = RB_ROOT;
static struct interval_tree_node nodes[NODES];
static u32 queries[SEARCHES];

static struct rnd_state rnd;

static inline unsigned long
search(unsigned long query, struct rb_root *root)
{
	struct interval_tree_node *node;
	unsigned long results = 0;

	for (node = interval_tree_iter_first(root, query, query); node;
	     node = interval_tree_iter_next(node, query, query))
		results++;
	return results;
}

static void init(void)
{
	int i;
	for (i = 0; i < NODES; i++) {
		u32 a = prandom_u32_state(&rnd);
		u32 b = prandom_u32_state(&rnd);
		if (a <= b) {
			nodes[i].start = a;
			nodes[i].last = b;
		} else {
			nodes[i].start = b;
			nodes[i].last = a;
		}
	}
	for (i = 0; i < SEARCHES; i++)
		queries[i] = prandom_u32_state(&rnd);
}

static int interval_tree_test_init(void)
{
	int i, j;
	unsigned long results;
	cycles_t time1, time2, _time;

	printk(KERN_ALERT "interval tree insert/remove");

	prandom_seed_state(&rnd, 3141592653589793238ULL);
	init();

	time1 = get_cycles();

	for (i = 0; i < PERF_LOOPS; i++) {
		for (j = 0; j < NODES; j++)
			interval_tree_insert(nodes + j, &root);
		for (j = 0; j < NODES; j++)
			interval_tree_remove(nodes + j, &root);
	}

	time2 = get_cycles();
	_time = time2 - time1;

	_time = div_u64(_time, PERF_LOOPS);
	printk(" -> %llu cycles\n", (unsigned long long)time);

	printk(KERN_ALERT "interval tree search");

	for (j = 0; j < NODES; j++)
		interval_tree_insert(nodes + j, &root);

	time1 = get_cycles();

	results = 0;
	for (i = 0; i < SEARCH_LOOPS; i++)
		for (j = 0; j < SEARCHES; j++)
			results += search(queries[j], &root);

	time2 = get_cycles();
	_time = time2 - time1;

	_time = div_u64(_time, SEARCH_LOOPS);
	results = div_u64(results, SEARCH_LOOPS);
	printk(" -> %llu cycles (%lu results)\n",
	       (unsigned long long)time, results);

	return -EAGAIN; /* Fail will directly unload the module */
}

static void interval_tree_test_exit(void)
{
	printk(KERN_ALERT "test exit\n");
}

int main(int argc, char **argv)
{
	interval_tree_test_init();
	interval_tree_test_exit();
	return 0;
}
