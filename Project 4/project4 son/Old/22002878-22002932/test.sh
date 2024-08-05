echo "\t testcase: test1 create"
./fatmod disk1 -c TEST1.TXT
echo "\t testcase: test4"
./fatmod disk1 -c TEST4.TXT
./fatmod disk1 -w TEST4.TXT 0 5 65
echo "\t testcase: read"
./fatmod disk1 -r -a TEST4.TXT
echo "\t testcase: test3"
./fatmod disk1 -c TEST3.TXT
./fatmod disk1 -w TEST3.TXT 0 5 65
./fatmod disk1 -w TEST3.TXT 5 5 66
echo "\t testcase: read"
./fatmod disk1 -r -a TEST3.TXT
echo "\t testcase: test2"
./fatmod disk1 -c TEST2.TXT
./fatmod disk1 -w TEST2.TXT 0 5 65
./fatmod disk1 -w TEST2.TXT 2 5 66
./fatmod disk1 -w TEST2.TXT 4 5 67
echo "\t testcase: read"
./fatmod disk1 -r -a TEST2.TXT
echo "\t testcase: ls"
./fatmod disk1 -l
echo "\t testcase: delete test1"
./fatmod disk1 -d TEST1.TXT
echo "\t testcase: delete test2"
./fatmod disk1 -d TEST2.TXT
echo "\t testcase: delete test3"
./fatmod disk1 -d TEST3.TXT
echo "\t testcase: delete test4"
./fatmod disk1 -d TEST4.TXT
echo "\t testcase: ls"
./fatmod disk1 -l
echo ""
