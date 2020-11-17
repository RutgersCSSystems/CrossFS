import math
import shlex
import sys

num_chnks = int(0)

if len(sys.argv) != 3:  # the program name and the two arguments
  # stop the program and print an error message
  sys.exit("Must provide input file name and time interval")


f = open(sys.argv[1], 'r')
print f

print ""

start_time_array = []
end_time_array = []
size_array= []

result_arr = [long]


for line in f:
	words = line.split()
	#print words[0] +" " + words[1] +" " +words[2]
	if len(words[0]) > 0:
		start_time_array.append(words[0])
		end_time_array.append(words[1])
		size_array.append(words[2])


length=len(start_time_array)

#for index in range(len(start_time_array)):
	##print start_time_array[index]+" "+end_time_array[index]+" "+size_array[index] 

temp = long(0)
strt = long(start_time_array[0])
interval = long(sys.argv[2]) * 1000000;


#while index < len(start_time_array):	
for index in range(len(start_time_array)):

	#strt = long(start_time_array[index])
	
	if long(start_time_array[index]) < strt:
		continue

	#print str(start_time_array[index])+" "+str(strt)

	temp = strt
	tot_bytes = long(0)
	end_range = interval + strt


	for endidx in range(len(end_time_array)):

		if long(end_time_array[endidx]) <= end_range and  long(end_time_array[endidx]) > strt:
			tot_bytes = tot_bytes + long(size_array[endidx])

	print "start time: "+str(strt)+" end time: "+ str(end_range)+" Bytes: "+str(tot_bytes)
	strt = temp + interval


			#if index + 1 == length:
			#	print str(strt)+" "+ str(endt)+" "+str(size_array[endidx])
			

