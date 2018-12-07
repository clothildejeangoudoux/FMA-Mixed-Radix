#!/bin/bash


if ! [ -z "$1" ] && [ "$1" -eq 1 ]
then
    TEST_PROG="valgrind --leak-check=full --num-callers=50 --child-silent-after-fork=yes ./fma_tests"
else
    TEST_PROG="./fma_tests"
fi

$TEST_PROG BBBD 1 fma_bin64_bin64_bin64_dec64.testvector fma_bin64_bin64_bin64_dec64.dat
# $TEST_PROG BBDB 1 fma_bin64_bin64_dec64_bin64.testvector fma_bin64_bin64_dec64_bin64.dat
# $TEST_PROG BBDD 1 fma_bin64_bin64_dec64_dec64.testvector fma_bin64_bin64_dec64_dec64.dat
# $TEST_PROG BDBB 1 fma_bin64_dec64_bin64_bin64.testvector fma_bin64_dec64_bin64_bin64.dat
# $TEST_PROG BDBD 1 fma_bin64_dec64_bin64_dec64.testvector fma_bin64_dec64_bin64_dec64.dat
# $TEST_PROG BDDB 1 fma_bin64_dec64_dec64_bin64.testvector fma_bin64_dec64_dec64_bin64.dat
# $TEST_PROG BDDD 1 fma_bin64_dec64_dec64_dec64.testvector fma_bin64_dec64_dec64_dec64.dat
# $TEST_PROG DBBB 1 fma_dec64_bin64_bin64_bin64.testvector fma_dec64_bin64_bin64_bin64.dat
# $TEST_PROG DBBD 1 fma_dec64_bin64_bin64_dec64.testvector fma_dec64_bin64_bin64_dec64.dat
# $TEST_PROG DBDB 1 fma_dec64_bin64_dec64_bin64.testvector fma_dec64_bin64_dec64_bin64.dat
# $TEST_PROG DBDD 1 fma_dec64_bin64_dec64_dec64.testvector fma_dec64_bin64_dec64_dec64.dat
# $TEST_PROG DDBB 1 fma_dec64_dec64_bin64_bin64.testvector fma_dec64_dec64_bin64_bin64.dat
# $TEST_PROG DDBD 1 fma_dec64_dec64_bin64_dec64.testvector fma_dec64_dec64_bin64_dec64.dat
# $TEST_PROG DDDB 1 fma_dec64_dec64_dec64_bin64.testvector fma_dec64_dec64_dec64_bin64.dat

# if ! [ -z "$2" ] && [ "$2" -eq 1 ]
# then
#    gnuplot -p -e "set xlabel 'Time';set ylabel 'Nb exec';set grid; set boxwidth 0.95 relative; set style fill transparent solid 0.5 noborder; plot 'fma_bin64_bin64_bin64_dec64.dat' w boxes lc rgb'blue' title 'Timings FMA BBBD'"

#    gnuplot -p -e "set xlabel 'Time';set ylabel 'Nb exec';set grid; set boxwidth 0.95 relative; set style fill transparent solid 0.5 noborder; plot 'fma_bin64_bin64_dec64_bin64.dat' w boxes lc rgb'blue' title 'Timings FMA BBDB'"

#    gnuplot -p -e "set xlabel 'Time';set ylabel 'Nb exec';set grid; set boxwidth 0.95 relative; set style fill transparent solid 0.5 noborder; plot 'fma_bin64_bin64_dec64_dec64.dat' w boxes lc rgb'blue' title 'Timings FMA BBDD'"

#    gnuplot -p -e "set xlabel 'Time';set ylabel 'Nb exec';set grid; set boxwidth 0.95 relative; set style fill transparent solid 0.5 noborder; plot 'fma_bin64_dec64_bin64_bin64.dat' w boxes lc rgb'blue' title 'Timings FMA BDBB'"

#    gnuplot -p -e "set xlabel 'Time';set ylabel 'Nb exec';set grid; set boxwidth 0.95 relative; set style fill transparent solid 0.5 noborder; plot 'fma_bin64_dec64_bin64_bin64.dat' w boxes lc rgb'blue' title 'Timings FMA BDBD'"

#    gnuplot -p -e "set xlabel 'Time';set ylabel 'Nb exec';set grid; set boxwidth 0.95 relative; set style fill transparent solid 0.5 noborder; plot 'fma_bin64_dec64_dec64_bin64.dat' w boxes lc rgb'blue' title 'Timings FMA BDDB'"

#    gnuplot -p -e "set xlabel 'Time';set ylabel 'Nb exec';set grid; set boxwidth 0.95 relative; set style fill transparent solid 0.5 noborder; plot 'fma_bin64_dec64_dec64_dec64.dat' w boxes lc rgb'blue' title 'Timings FMA BDDD'"

#    gnuplot -p -e "set xlabel 'Time';set ylabel 'Nb exec';set grid; set boxwidth 0.95 relative; set style fill transparent solid 0.5 noborder; plot 'fma_dec64_bin64_bin64_bin64.dat' w boxes lc rgb'green' title 'Timings FMA DBBB'"

#    gnuplot -p -e "set xlabel 'Time';set ylabel 'Nb exec';set grid; set boxwidth 0.95 relative; set style fill transparent solid 0.5 noborder; plot 'fma_dec64_bin64_bin64_dec64.dat' w boxes lc rgb'green' title 'Timings FMA DBBD'"

#    gnuplot -p -e "set xlabel 'Time';set ylabel 'Nb exec';set grid; set boxwidth 0.95 relative; set style fill transparent solid 0.5 noborder; plot 'fma_dec64_bin64_dec64_bin64.dat' w boxes lc rgb'green' title 'Timings FMA DBDB'"

#    gnuplot -p -e "set xlabel 'Time';set ylabel 'Nb exec';set grid; set boxwidth 0.95 relative; set style fill transparent solid 0.5 noborder; plot 'fma_dec64_bin64_dec64_dec64.dat' w boxes lc rgb'green' title 'Timings FMA DBDD'"

#    gnuplot -p -e "set xlabel 'Time';set ylabel 'Nb exec';set grid; set boxwidth 0.95 relative; set style fill transparent solid 0.5 noborder; plot 'fma_dec64_dec64_bin64_bin64.dat' w boxes lc rgb'green' title 'Timings FMA DDBB'"

#    gnuplot -p -e "set xlabel 'Time';set ylabel 'Nb exec';set grid; set boxwidth 0.95 relative; set style fill transparent solid 0.5 noborder; plot 'fma_dec64_dec64_bin64_dec64.dat' w boxes lc rgb'green' title 'Timings FMA DDBD'"

#    gnuplot -p -e "set xlabel 'Time';set ylabel 'Nb exec';set grid; set boxwidth 0.95 relative; set style fill transparent solid 0.5 noborder; plot 'fma_dec64_dec64_dec64_bin64.dat' w boxes lc rgb'green' title 'Timings FMA DDDB'"
# fi
