#! /usr/bin/env python

from zplot import *

ctype = 'eps' if len(sys.argv) < 2 else sys.argv[1]
#c = pdf('figure1.pdf')
c = canvas('pdf', title='result_thread_readrandom', dimensions=[150, 100])
t = table(file='thread_readrandom.data')

d = drawable(canvas=c, coord=[25,22], xrange=[-0.5,t.getmax('rownumber')+0.5],
                yrange=[0,1000], dimensions=[120, 60])

# because tics and axes are different, call axis() twice, once to
# specify x-axis, the other to specify y-axis
axis(d, linewidth=0.5, xtitle='Rocksdb Threads', xtitlesize=6,
xmanual=t.query(select='thread,rownumber'), xlabelfontsize=6,
ytitle='Throughput(kops/s)', ytitlesize=6, ylabelfontsize=6, yauto=[0,1000,500], ticmajorsize=2)

grid(drawable=d, x=False, yrange=[500,1000], ystep=500, linecolor='lightgrey',
        linedash=[2,2], linewidth=0.3)

p = plotter()
L = legend()

barargs = {'drawable':d, 'table':t, 'xfield':'rownumber',
           'linewidth':0.5, 'fill':True, 'barwidth':0.6,
		   'legend':L, 'stackfields':[]}

barargs['yfield'] = 'ext4dax'
barargs['legendtext'] = 'ext4-DAX'
barargs['fillcolor'] = 'dimgray'
barargs['fillstyle'] = 'vline'
barargs['fillsize'] = '0.5'
barargs['fillskip'] = '0.5'
barargs['cluster'] = [0,4]
p.verticalbars(**barargs)

barargs['yfield'] = 'devfs'
barargs['legendtext'] = 'DevFS'
barargs['fillcolor'] = 'tan'
barargs['fillstyle'] = 'dline12'
barargs['fillsize'] = '0.5'
barargs['fillskip'] = '1'
barargs['cluster'] = [1,4]
p.verticalbars(**barargs)

barargs['yfield'] = 'parafs_naive'
barargs['legendtext'] = 'FDFS'
barargs['fillcolor'] = 'black'
barargs['fillstyle'] = 'dline1'
barargs['fillsize'] = '0.5'
barargs['fillskip'] = '1'
barargs['cluster'] = [2,4]
p.verticalbars(**barargs)

barargs['yfield'] = 'parafs_direct'
barargs['legendtext'] = 'FDFS-direct'
barargs['fillcolor'] = 'darkgray'
barargs['fillstyle'] = 'dline2'
barargs['fillsize'] = '0.5'
barargs['fillskip'] = '1'
barargs['cluster'] = [3,4]
p.verticalbars(**barargs)


L.draw(c, coord=[d.left()-20, d.top()+10], width=4, height=4, fontsize=4,
        skipnext=1, hspace=1, skipspace=36)

c.render()
