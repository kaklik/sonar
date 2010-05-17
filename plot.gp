!./sonar
set size 1,1
set origin 0,0
set multiplot

set size 0.95,0.2
set origin 0,0.8
set xrange [0:150]
set xlabel "sample"
set autoscale y
set key off
set ytics 20000
plot "/tmp/chirp.txt" using 1:2 with lines title 'chirp' 

set size 0.95,0.4
set origin 0,0.4
set xrange [0:5] 
set xlabel "distance [m]"
set autoscale y
set key on
set ytics 5000
set mxtics 10
plot "/tmp/sonar.txt" using 1:2 with lines title 'L echo', "" using 1:3 with lines title 'R echo'

set size 0.95,0.4
set origin 0,0
set xrange [0:5]
set yrange [0:2e9]
#set autoscale y
set key on
set ytics 1e9
plot "/tmp/sonar.txt" using 1:4 with lines title 'L correlation', "" using 1:5 with lines title 'R correlation'
pause 1
reread
