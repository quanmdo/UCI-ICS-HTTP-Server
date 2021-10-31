#!/bin/bash

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# trap ctrl-c and call ctrl_c() 
trap ctrl_c INT  
ctrl_c() {
  kill_server
}

start_server() {
  ./httpserver 8080 &
  sleep 0.5
}

kill_server() {
  pkill httpserver
}

check_test() {
  RET=$?
  if [[ $RET -eq 0 ]]; then
    echo -e "${GREEN}PASS${NC}"
  else
    echo -e "${RED}FAIL${NC}"
  fi
}

print_banner() {
  CYAN='\033[0;36m'
  echo -e "${CYAN}$1${NC}"
}

# =========== start of test script functions ==================
# send a GET request
test_one() {
  print_banner "Test 1: ${test_str[0]}"
  start_server
  head -c 100 /dev/urandom > dldkald10
  curl -s http://localhost:8080/dldkald10 -o test1.out
  kill_server
  diff dldkald10 test1.out
  check_test
}

# Send a PUT request
test_two() {
  print_banner "Test 2: ${test_str[1]}"
  start_server
  dd if=/dev/urandom of=myfile bs=1024 count=63 status=none # 63 KiB file
  curl -s http://localhost:8080/e901kollkad -T myfile
  kill_server
  diff myfile e901kollkad
  check_test
}
# Send multiple GET requests
test_three() {
  print_banner "Test 3: ${test_str[2]}"
  # start_server
  ./httpserver 8080 -l requests.log &
  sleep 0.5

  head -c 2 /dev/urandom > 12dsaDSAEFV
  head -c 20 /dev/urandom > WQD_-12
  head -c 200 /dev/urandom > a
  ( curl -v http://localhost:8080/12dsaDSAEFV -o test3-cl1.out & \
  curl -v http://localhost:8080/WQD_-12 -o test3-cl2.out & \
  curl -v http://localhost:8080/a -o test3-cl3.out & \
 curl -v http://localhost:8080/12dsaDSAEFV -o test3-cl1.out & \
 curl -v http://localhost:8080/WQD_-12 -o test3-cl2.out & \
 curl -v http://localhost:8080/a -o test3-cl3.out & \
  wait)
  kill_server
  #diff 12dsaDSAEFV test3-cl1.out && \
  diff WQD_-12 test3-cl2.out && \
  diff a test3-cl3.out
  check_test
}

# test healthcheck
test_four() {
  print_banner "Test 4: ${test_str[3]}" #"Test 4: GET request to healthcheck"
  ./httpserver 8080 -l requests.log &
  sleep 0.5
  curl -s http://localhost:8080/dldkald10 # exists from test 1
  curl -s http://localhost:8080/1234l # should not exist
  printf "1\n2" > test4-hck.out
  curl -s http://localhost:8080/healthcheck -o test4-server.out
  kill_server
  diff test4-hck.out test4-server.out
  check_test
}

# test healthcheck
test_five() {
  #print_banner "Test 5: ${test_str[4]}" #"Test 4: GET request to healthcheck (404) error"
  #start_server -l requests.log
  ./httpserver 8080 -l requests.log &
  sleep 0.5
  #multiple puts at once
#   (curl -T test0 http://localhost:8080/test0OUT & \
# curl -T test1 http://localhost:8080/test1OUT & \
# curl -T test2 http://localhost:8080/test2OUT & \
# curl -T test3 http://localhost:8080/test3OUT & \
# curl -T test4 http://localhost:8080/test4OUT & \
#   wait)
(
curl -T test0 http://localhost:8080/test0OUT & \
curl http://localhost:8080/test1OUT & \
curl -I test2 http://localhost:8080/test2OUT & \
curl -T test3 http://localhost:8080/test3OUT & \
curl http://localhost:8080/test4OUT & \
  wait)
sleep 5
  kill_server
  #grep -q "404" test5-server.out
  check_test
}

# =========== main to select test ==================
test_str=("GET a small binary file" \
	"PUT a large binary file" \
	"GET multiple small binary files" \
	"GET request to healthcheck" \
	"GET request to healthcheck (404) error")
len_test_str=${#test_str[@]}
echo "Select test to run:"
for index in "${!test_str[@]}"
do
	printf "\t$((index+1))) ${test_str[index]}\n"
done
printf "\t$((len_test_str+1))) All tests\nEnter number: "

read TEST

# build httpserver
printf "\n===== Build httpserver =====\n"
make clean
make

printf "\n====== START of TESTS ======\n"
case $TEST in

  1)
    test_one
    ;;

  2)
    test_two
    ;;

  3) 
    test_three
    ;;

  4)
    test_four
    ;;
  5)
    test_five
    ;;
  6)
    test_one
    test_two
    test_three
    test_four
    test_five
    ;;

  *)
    echo -n "unknown"
    ;;
esac
printf "====== END of TESTS ======\n\n"
echo -e "${RED}Delete test files?${NC}"
ls | grep -E "*.out"
ls | grep -P '^(?!Makefile)([a-zA-Z0-9_-]+){1,27}$'
echo -ne "${RED}y/n?${NC} "
read CLEAN
if [[ $CLEAN == 'y' ]]; then
  rm *.out
  ls | grep -P '^(?!Makefile)([a-zA-Z0-9_-]+){1,27}$' | xargs rm &> /dev/null
fi

# curl -v -T test0 http://localhost:8080/test0
# curl -v -T test1 http://localhost:8080/test1
# curl -v -T test2 http://localhost:8080/test2
# curl -v -T test3 http://localhost:8080/test3
# curl -v -T test4 http://localhost:8080/test4