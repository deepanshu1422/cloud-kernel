/*
 * CPU <-> hardware queue mapping helpers
 *
 * Copyright (C) 2013-2014 Jens Axboe
 */
#include <linux/kernel.h>
#include <linux/threads.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/cpu.h>

#include <linux/blk-mq.h>
#include "blk.h"
#include "blk-mq.h"

static int queue_index(unsigned int nr_queues, const int q)
{
	return q % nr_queues;
}

static int get_first_sibling(unsigned int cpu)
{
	unsigned int ret;

	ret = cpumask_first(topology_sibling_cpumask(cpu));
	if (ret < nr_cpu_ids)
		return ret;

	return cpu;
}

int blk_mq_map_queues(struct blk_mq_tag_set *set)
{
	unsigned int *map = set->mq_map;
	unsigned int nr_queues = set->nr_hw_queues;
	unsigned int cpu, first_sibling, q = 0;

	for_each_possible_cpu(cpu)
		map[cpu] = -1;

	/*
	 * Spread queues among present CPUs first for minimizing
	 * count of dead queues which are mapped by all un-present CPUs
	 */
	for_each_present_cpu(cpu) {
		if (q >= nr_queues)
			break;
		map[cpu] = queue_index(nr_queues, q++);
	}

	for_each_possible_cpu(cpu) {
		if (map[cpu] != -1)
			continue;
		/*
		 * First do sequential mapping between CPUs and queues.
		 * In case we still have CPUs to map, and we have some number of
		 * threads per cores then map sibling threads to the same queue for
		 * performace optimizations.
		 */
		if (cpu < nr_queues) {
			map[cpu] = queue_index(nr_queues, q++);
		} else {
			first_sibling = get_first_sibling(cpu);
			if (first_sibling == cpu)
				map[cpu] = queue_index(nr_queues, q++);
			else
				map[cpu] = map[first_sibling];
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(blk_mq_map_queues);

/*
 * We have no quick way of doing reverse lookups. This is only used at
 * queue init time, so runtime isn't important.
 */
int blk_mq_hw_queue_to_node(unsigned int *mq_map, unsigned int index)
{
	int i;

	for_each_possible_cpu(i) {
		if (index == mq_map[i])
			return local_memory_node(cpu_to_node(i));
	}

	return NUMA_NO_NODE;
}
