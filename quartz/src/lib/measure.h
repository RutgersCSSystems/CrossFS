/***************************************************************************
Copyright 2016 Hewlett Packard Enterprise Development LP.  
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or (at
your option) any later version. This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details. You
should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
***************************************************************************/
#ifndef __MEASURE_H
#define __MEASURE_H

/**
 * \file 
 * 
 * Memory latency and bandwidth measurements
 */

/**
 * \brief Measure memory read bandwidth
 *
 * Measures memory read bandwidth from a local socket (cpu_node) 
 * to the memory of a remote socket (mem_node). It does this 
 * by firing a bunch of threads issuing streaming instructions
 * to saturate memory bandwidth. 
 */
double measure_read_bw(int cpu_node, int mem_node);

/**
 * \brief Measure memory write bandwidth
 *
 * Measures memory write bandwidth from a local socket (cpu_node) 
 * to the memory of a remote socket (mem_node).
 * See measure_read_bw for how this is done.
 */
double measure_write_bw(int cpu_node, int mem_node);


/** 
 * \brief Measure memory latency 
 * 
 * Measures memory read latency from one local socket to the memory of a 
 * remote socket. It does this using a pointer chasing microbenchmark.
 * The microbenchmark setups an array where each element determines the
 * element to be read next.
 */ 
int measure_latency(cpu_model_t* cpu, int from_node_id, int to_node_id);

/**
 * \brief Calibrate memory latency
 *
 * Automatically tweaks the memory latency based on the detected hardware latency
 * on the target systems.
 */
void latency_calibration();

#endif
