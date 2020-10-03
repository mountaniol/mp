#GCC=gcc
#CFLAGS=-Wall -Wextra -rdynamic -O2

GCC=clang-10
CFLAGS=-Wall -Wextra -O2
#CFLAGS=-Wall -Wextra -O2
DEBUG=-DDEBUG3
DEBUG += -DDEBUG2
DEBUG += -DDERROR3
#CFLAGS += -fanalyzer

#GCCVERSION=$(shell gcc -dumpversion | sed -e 's/\.\([0-9][0-9]\)/\1/g' -e 's/\.\([0-9]\)/0\1/g' -e 's/^[0-9]\{3,4\}$/&00/')

#ifeq "$(GCCVERSION)" "10"
#	CFLAGS += --fanalyzer
#endif

# client daemon

BUFT_A=buf_t/buf_t.a
J_ARCH=/usr/lib/x86_64-linux-gnu/libjansson.a
J_LIB=jansson
MINI_ARCH=/usr/lib/x86_64-linux-gnu/libminiupnpc.a
MINI_LIB=miniupnpc
MOSQ_LIB=mosquitto
MOSQ_T=mpd
MOSQ_O=mp-main.o mp-jansson.o mp-config.o\
		mp-ports.o mp-cli.o mp-memory.o mp-ctl.o mp-network.o \
		mp-requests.o mp-communicate.o mp-os.o mp-net-utils.o \
		mp-security.o mp-htable.o mp-dispatcher.o mp-mqtt-module.o

MOSQ_C=mp-main.c mp-jansson.c mp-config.c\
		mp-ports.c sec-client-mosq-cli-serv.c mp-memory.c sec-ctl.c mp-network.c \
		mp-requests.c mp-htable.o
# client cli
CLI_O=mp-shell.o mp-jansson.o mp-memory.o mp-ctl.o mp-os.o mp-net-utils.o mp-htable.o mp-dispatcher.o libfort.a
CLI_T=mp

# Port mapper, standalone compilation
U_T=portmapper
U_C=mp-ports.c sec-memory.c

# Files to check with splint
SPLINT_C= mp-cli.c mp-cli.h mp-common.h mp-communicate.c mp-communicate.h \
		mp-config.c mp-config.h mp-ctl.c mp-ctl.h mp-debug.h mp-dict.h mp-jansson.c \
		mp-jansson.h mp-limits.h mp-main.c mp-main.h mp-memory.c mp-memory.h \
		mp-net-utils.c mp-net-utils.h mp-network.c mp-network.h mp-os.c mp-os.h \
		mp-ports.c mp-ports.h mp-requests.c mp-requests.h mp-shell.c mp-tunnel.c \
		mp-tunnel.h mp-version.h

all: m cli
	@echo "Version = $(GCCVERSION)"
	@echo "DEBUG = $(DEBUG)"
	@echo "CFLAGS = $(CFLAGS)"
	@echo "Compiler = $(GCC)"


buft:
	make -C ./buf_t/ all

m: buft $(MOSQ_O)
	@echo "|>> Linking mserver"
	$(GCC) $(CFLAGS) $(DEBUG) $(MOSQ_O) $(BUFT_A) -o $(MOSQ_T) -lpthread -lcrypto -lssl -l$(MINI_LIB) -l$(J_LIB) -l$(MOSQ_LIB)
	#$(GCC) $(CFLAGS) $(DEBUG) $(MOSQ_O) $(BUFT_A) -o $(MOSQ_T) /usr/lib/x86_64-linux-gnu/libmosquitto.so  -lpthread -lcrypto -lssl -l$(MINI_LIB) -l$(J_LIB)
	#$(GCC) $(CFLAGS) $(DEBUG) $(MOSQ_O) $(MINI_ARCH) $(BUFT_A) -o $(MOSQ_T) /usr/lib/x86_64-linux-gnu/libmosquitto.so  -lpthread -lcrypto -lssl $(J_ARCH)
	#$(GCC) $(CFLAGS) $(DEBUG) $(MOSQ_O)  $(J_ARCH) $(MINI_ARCH) $(BUFT_A) -o $(MOSQ_T) /usr/lib/x86_64-linux-gnu/libmosquitto.so  -lpthread -lcrypto -lssl
	#$(GCC) $(CFLAGS) $(DEBUG) $(MOSQ_O) -o $(MOSQ_T) /usr/lib/x86_64-linux-gnu/libmosquitto.so -ljansson -lminiupnpc -lpthread -lssh2

cli: buft $(CLI_O)
	@echo "|>> Linking mclient"
	@$(GCC) $(CFLAGS) $(DEBUG) $(CLI_O) $(BUFT_A) -o $(CLI_T) -lpthread -l$(J_LIB)
	#@$(GCC) $(CFLAGS) $(DEBUG) $(CLI_O) $(J_ARCH) $(BUFT_A) -o $(CLI_T) -lpthread
	#@$(GCC) $(CFLAGS) $(CLI_O) -o $(CLI_T) /usr/lib/x86_64-linux-gnu/libmosquitto.so -ljansson -lminiupnpc -lpthread
	
u: buft
	$(GCC) -DSTANDALONE $(CFLAGS) $(DEBUG) $(U_C) $(BUFT_A) -o $(U_T) -lminiupnpc

upnp: buft
	$(GCC) $(CFLAGS) -DSTANDALONE $(DEBUG) $(BUFT_A) libfort.a mp-ports.c mp-memory.c -o ports -lminiupnpc

eth: buft
	$(GCC) $(CFLAGS) -DSTANDALONE $(DEBUG) mp-network.c $(BUFT_A)  -o sec-eth
#	/usr/lib/x86_64-linux-gnu/libminiupnpc.a

tunnel: buft
	$(GCC) $(CFLAGS) -DSTANDALONE $(DEBUG) mp-tunnel.c mp-net-utils.c mp-jansson.c mp-memory.c mp-config.c mp-ctl.c mp-os.c mp-htable.c $(BUFT_A) -o tunnel.standalone -lutil -lpthread -lssl -lcrypto -l$(J_LIB) #-lpam
	#$(GCC) $(CFLAGS) -DSTANDALONE $(DEBUG) mp-tunnel.c mp-net-utils.c mp-jansson.c mp-memory.c mp-config.c mp-ctl.c mp-os.c $(J_ARCH)  $(BUFT_A) -o tunnel.standalone -lutil -lpthread -lssl -lcrypto #-lpam

btest: buft
	$(GCC) $(CFLAGS) -ggdb -DSTANDALONE $(DEBUG) mp-memory.c $(BUFT_A) -o buf_t_test.out

clean:
	rm -f $(MOSQ_T) $(MOSQ_O) $(MOSQ_CLI_O) $(MOSQ_CLI_T) *.o
	make -C ./buf_t/ clean
libfort.a:
	mkdir -p libfort/build && cd libfort/build && cmake ../ && make
	cp libfort/build/lib/libfort.a .


.PHONY:check
check:
	@echo "+++ $@: USER=$(USER), UID=$(UID), GID=$(GID): $(CURDIR)"
	#echo ============= 32 bit check =============
	$(ECH)cppcheck -j2 -q --force  --enable=all --platform=unix32 -I/usr/include/openssl $(SPLINT_C)       
	#echo ============= 64 bit check =============
	#$(ECH)cppcheck -q --force  --enable=all --platform=unix64 -I/usr/include/openssl ./*.[ch]

.PHONY:splint
splint:
	@echo "+++ $@: USER=$(USER), UID=$(UID), GID=$(GID): $(CURDIR)"
	splint -noeffect -mustfreeonly -forcehints -weak -redef +matchanyintegral +gnuextensions -preproc +unixlib -I/usr/include/openssl -D__gnuc_va_list=va_list  ./*.[ch]
	#splint -standard -noeffect -redef +matchanyintegral +gnuextensions -preproc +unixlib  +trytorecover -mayaliasunique +posixlib -I/usr/include/openssl -D__gnuc_va_list=va_list $(SPLINT_C)
	#splint -forcehints -standard -redef -exportlocal -export-header -isoreserved  -preproc +unixlib -I/usr/include/openssl -D__gnuc_va_list=va_list  ./*.[ch]
	#splint -checks -redef -exportlocal -export-header -isoreserved  -preproc +unixlib -I/usr/include/openssl -D__gnuc_va_list=va_list  ./*.[ch]
	#splint -forcehints -weak -redef +matchanyintegral +gnuextensions -preproc +unixlib -I/usr/include/openssl -D__gnuc_va_list=va_list  ./*.[ch]
flaw:
	flawfinder ./*.[ch] 

%.o:%.c
	@echo "|>" $@...
	@$(GCC) -g $(INCLUDE) $(CFLAGS) $(DEBUG) -c -o $@ $<


