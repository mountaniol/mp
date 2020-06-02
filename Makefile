GCC=gcc
#GCC=clang-10
CFLAGS=-Wall -Wextra -rdynamic -Os
DEBUG=-DDEBUG3
DEBUG += -DDERROR3
CFLAGS += -fanalyzer

#GCCVERSION=$(shell gcc -dumpversion | sed -e 's/\.\([0-9][0-9]\)/\1/g' -e 's/\.\([0-9]\)/0\1/g' -e 's/^[0-9]\{3,4\}$/&00/')

#ifeq "$(GCCVERSION)" "10"
#	CFLAGS += --fanalyzer
#endif

#DEBUG=-DDEBUG3

# client daemon

J_ARCH=/usr/lib/x86_64-linux-gnu/libjansson.a
MINI_ARCH=/usr/lib/x86_64-linux-gnu/libminiupnpc.a
MOSQ_T=mpd
MOSQ_O=mp-main.o mp-jansson.o buf_t.o mp-config.o\
		mp-ports.o mp-cli.o mp-memory.o mp-ctl.o mp-network.o \
		mp-requests.o mp-communicate.o mp-os.o mp-ssh.o mp-net-utils.o

MOSQ_C=mp-main.c mp-jansson.c buf_t.c mp-config.c\
		mp-ports.c sec-client-mosq-cli-serv.c mp-memory.c sec-ctl.c mp-network.c \
		mp-requests.c
# client cli
CLI_O=mp-shell.o mp-jansson.o mp-memory.o buf_t.o mp-ctl.o mp-os.o mp-net-utils.o libfort.a
CLI_T=mp

# Port mapper, standalone compilation
U_T=portmapper
U_C=mp-ports.c sec-memory.c

# Files to check with splint
SPLINT_C=buf_t.c buf_t.h mp-cli.c mp-cli.h mp-common.h mp-communicate.c mp-communicate.h \
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

m: $(MOSQ_O)
	@echo "|>> Linking mserver"
	$(GCC) $(CFLAGS) $(DEBUG) $(MOSQ_O)  $(J_ARCH) $(MINI_ARCH) -o $(MOSQ_T) /usr/lib/x86_64-linux-gnu/libmosquitto.so  -lpthread -lssh2
	#$(GCC) $(CFLAGS) $(DEBUG) $(MOSQ_O) -o $(MOSQ_T) /usr/lib/x86_64-linux-gnu/libmosquitto.so -ljansson -lminiupnpc -lpthread -lssh2

cli: $(CLI_O)
	@echo "|>> Linking mclient"
	@$(GCC) $(CFLAGS) $(DEBUG) $(CLI_O) $(J_ARCH) -o $(CLI_T) -lpthread
	#@$(GCC) $(CFLAGS) $(CLI_O) -o $(CLI_T) /usr/lib/x86_64-linux-gnu/libmosquitto.so -ljansson -lminiupnpc -lpthread
	
u:
	$(GCC) -DSTANDALONE $(CFLAGS) $(DEBUG) $(U_C) -o $(U_T) -lminiupnpc

upnp:
	$(GCC) $(CFLAGS) -DSTANDALONE $(DEBUG) libfort.a mp-ports.c mp-memory.c -o ports -lminiupnpc

eth:
	$(GCC) $(CFLAGS) -DSTANDALONE $(DEBUG) mp-network.c -o sec-eth
#	/usr/lib/x86_64-linux-gnu/libminiupnpc.a

tunnel:
	$(GCC) $(CFLAGS) -DSTANDALONE $(DEBUG) mp-tunnel.c mp-net-utils.c buf_t.c mp-jansson.c mp-memory.c $(J_ARCH) -o tunnel.standalone -lutil -lpthread -lpam
clean:
	rm -f $(MOSQ_T) $(MOSQ_O) $(MOSQ_CLI_O) $(MOSQ_CLI_T) *.o 


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


