"Simple demonstration of MatPlotLib plotting."""
import math
import shlex
import sys
from subprocess import call

idx = []
Y = []
Y1 =[]
Y2 = []
words = []
a =0

if len(sys.argv) != 2:  # the program name and the two arguments
  # stop the program and print an error message
  sys.exit("Must provide input file name and time interval")

f = open(sys.argv[1], 'r')
for line in f:
 print line

 str = "wget " + line
 
call(str)	

