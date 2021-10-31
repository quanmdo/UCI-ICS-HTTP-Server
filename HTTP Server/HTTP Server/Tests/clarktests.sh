#!/bin/bash
SERVER_PATH=~/you/need/to/modify/this
PORT=8008   # feel free to modify this

### Check if a directory does not exist ###
# https://www.cyberciti.biz/faq/howto-check-if-a-directory-exists-in-a-bash-shellscript/
if [ ! -d ${SERVER_PATH} ] 
then
    echo "Directory ${SERVER_PATH} DOES NOT exist. Please modify the script to point to your httpserver." 
    exit 1
fi

# Prev test cleanup:
rm -rf out*
rm -rf ${SERVER_PATH}/TO_PUT

# Create the source files:
head -c 1000 < /dev/urandom > ${SERVER_PATH}/source_small
head -c 10000 < /dev/urandom > ${SERVER_PATH}/source_medium
head -c 100000 < /dev/urandom > ${SERVER_PATH}/source_large
head -c 2222 < /dev/urandom > ${SERVER_PATH}/source_4
head -c 22222 < /dev/urandom > ${SERVER_PATH}/source_5
head -c 222222 < /dev/urandom > ${SERVER_PATH}/source_6
head -c 150000 < /dev/urandom > client_large.bin

(curl -sI http://localhost:${PORT}/source_large > out_h0 & \
curl -s http://localhost:${PORT}/TO_PUT -T client_large.bin > /dev/null & \
curl -s http://localhost:${PORT}/source_small --output out1 & \
curl -s http://localhost:${PORT}/source_medium --output out2 & \
curl -s http://localhost:${PORT}/source_large --output out3 & \
curl -s http://localhost:${PORT}/source_4 --output out4 & \
curl -s http://localhost:${PORT}/source_5 --output out5 & \
curl -s http://localhost:${PORT}/source_6 --output out6 & \
wait)

# Diff the files, if there's any printout before "test done!!!", then a test "failed"
diff out1 ${SERVER_PATH}/source_small
diff out2 ${SERVER_PATH}/source_medium
diff out3 ${SERVER_PATH}/source_large
diff out4 ${SERVER_PATH}/source_4
diff out5 ${SERVER_PATH}/source_5
diff out6 ${SERVER_PATH}/source_6
diff client_large.bin ${SERVER_PATH}/TO_PUT

echo "test done!!!"
