#!/bin/sh

echo "Enter application name and arguments as params for the script"

sudo chmod o+rw /dev/cpu/*/msr
sudo modprobe msr

#sudo likwid-perfctr  -C 0-7  -g INSTR_RETIRED_ANY:FIXC0,MEM_UNCORE_RETIRED_LOCAL_DRAM:PMC0,MEM_UNCORE_RETIRED_REMOTE_DRAM:PMC1 $1
#sudo likwid-perfctr  -C 0-7 -g INSTR_RETIRED_ANY:FIXC0,MEM_UNCORE_RETIRED_LOCAL_DRAM:PMC0,MEM_INST_RETIRED_LOADS:PMC1,MEM_INST_RETIRED_STORES:PMC2,UNC_L3_MISS_ANY:UPMC0 $1
#sudo likwid-perfctr  -C 0-7 -g INSTR_RETIRED_ANY:FIXC0 $1
#sudo likwid-powermeter -p  $1


#Haswell Memory stats
#sudo likwid-powermeter  $1
#sudo likwid-perfctr  -C 0-7 -g INSTR_RETIRED_ANY:FIXC0,MEM_LOAD_UOPS_LLC_MISS_RETIRED_LOCAL_DRAM:PMC0,MEM_UOP_RETIRED_LOADS:PMC1,MEM_UOP_RETIRED_STORES:PMC2 $1

sudo likwid-perfctr  -C 0-3 -g INSTR_RETIRED_ANY:FIXC0 $1




