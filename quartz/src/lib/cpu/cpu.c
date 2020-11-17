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
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <string.h>
#include "cpu.h"
#include "dev.h"
#include "error.h"
#include "misc.h"
#include "known_cpus.h"

// Mainline architectures and processors available here:
// https://software.intel.com/en-us/articles/intel-architecture-and-processor-identification-with-cpuid-model-and-family-numbers
//
// It turns out that CPUID is not an accurate approach to identifying a 
// processor as different processors may have the same CPUID.
// So instead we rely on the brand string returned by /proc/cpuinfo:model_name

#define MASK(msb, lsb) (~((~0) << (msb+1)) & ((~0) << lsb))
#define EXTRACT(val, msb, lsb) ((MASK(msb, lsb) & val) >> lsb)  
#define MODEL(eax) EXTRACT(eax, 7, 4)
#define EXTENDED_MODEL(eax) EXTRACT(eax, 19, 16)
#define MODEL_NUMBER(eax) ((EXTENDED_MODEL(eax) << 4) | MODEL(eax))
#define FAMILY(eax) EXTRACT(eax, 11, 8)

void cpuid(unsigned int info, unsigned int *eax, unsigned int *ebx, unsigned int *ecx, unsigned int *edx)
{
    __asm__(
        "cpuid;"                                           
        :"=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
        :"a" (info) 
    );
}

// caller is responsible for freeing memory allocated by this function
char* cpuinfo(char* valname)
{
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen("/proc/cpuinfo", "r");
    if (fp == NULL) {
        return NULL;
    }

    while ((read = getline(&line, &len, fp)) != -1) {
        if (strstr(line, valname)) {
            char* colon = strchr(line, ':');
            int len = colon - line;
            char* buf = malloc(strlen(line) - len);
            strcpy(buf, &line[len+2]);
            free(line);
            fclose(fp);
            return buf;
        }
    }

    free(line);
    fclose(fp);
    return NULL;
}

// reads current cpu frequency through the /proc/cpuinfo file
// avoid calling this function often
int cpu_speed_mhz()
{
    size_t val;
    char*  str = cpuinfo("cpu MHz");
    val = string_to_size(str);
    free(str);
    return val;
}

// reads cpu LLC cache size through the /proc/cpuinfo file
// avoid calling this function often
size_t cpu_llc_size_bytes()
{
    size_t val;
    char*  str = cpuinfo("cache size");
    val = string_to_size(str);
    free(str);
    return val;
}

// caller is responsible for freeing memory allocated by this function
char* cpu_model_name()
{
    return cpuinfo("model name");
}

int match(const char* to_match, const char* regex_text)
{
    int ret;
    const char* p = to_match;
    regex_t regex;
    regmatch_t m[1];

    if ((ret = regcomp(&regex, regex_text, REG_EXTENDED|REG_NEWLINE)) != 0) {
        return E_ERROR;
    }
    if ((ret = regexec(&regex, p, 1, m, 0))) {
        regfree(&regex);
        return E_ERROR; // no match
    }
    regfree(&regex);
    return E_SUCCESS;
}

cpu_model_t* cpu_model()
{
    int i;
    char* model_name;
    cpu_model_t* cpu_model = NULL;

    if ((model_name = cpu_model_name()) == NULL) {
        return NULL;
    }

    for (i=0; known_cpus[i] != 0; i++) {
        cpu_model_t* c = known_cpus[i];
	
        if (match(model_name, c->desc.vendor_name) == E_SUCCESS && 
            match(model_name, c->desc.brand_name) == E_SUCCESS && 
            match(model_name, c->desc.brand_processor_number) == E_SUCCESS) 
        {
            cpu_model = known_cpus[i];
            DBG_LOG(INFO, "Detected CPU model '%s'\n", cpu_model->desc.uarch);
            break;
        }       
    }

    free(model_name);

    if (!cpu_model) {
        return NULL;
    }

    // complete the model with some runtime information
    cpu_model->llc_size_bytes = cpu_llc_size_bytes();
//    cpu_model->speed_mhz = cpu_speed_mhz();

    return cpu_model;
}
