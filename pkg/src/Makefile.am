AUTOMAKE_OPTIONS = subdir-objects
bin_PROGRAMS = test_server test_client ipc_send test_rpc ipc_wait
lib_LTLIBRARIES = libaf_ipc.la 
include_HEADERS = af_ipc_common.h af_ipc_client.h af_ipc_server.h af_rpc.h

# ipc library
LIBPATH=$(CURDIR)/.libs
libaf_ipc_la_LDFLAGS = -module -Wall -ggdb3 -std=gnu99 -shared -fPIC -soname, libaf_ipc.so.0 -laf_util
libaf_ipc_la_CFLAGS = -Wall -std=gnu99 -ggdb3
libaf_ipc_la_SOURCES = af_ipc_server.c af_ipc_client.c af_ipc_common.c af_rpc.c

# serialize library

# test server program
test_server_SOURCES = libaf_ipc_test_server.c
test_server_LDADD = -L. -lpthread -levent -laf_ipc -laf_util
test_server_CFLAGS = -Wall -std=gnu99

# test client program
test_client_SOURCES = libaf_ipc_test_client.c
test_client_LDADD = -L. -lpthread -levent -laf_ipc -laf_util
test_client_CFLAGS = -Wall -std=gnu99

# ipc send program
ipc_send_SOURCES = ipc_send.c
ipc_send_LDADD = -L. -lpthread -levent -laf_ipc -laf_util
ipc_send_CFLAGS = -Wall -std=gnu99

# rpc test program
test_rpc_SOURCES = test_rpc.c af_rpc.c
test_rpc_LDADD = -L. -lpthread -levent
test_rpc_CFLAGS = -Wall -std=gnu99

# ipc wait program
ipc_wait_SOURCES = ipc_wait.c
ipc_wait_LDADD = -L. -lpthread -levent -laf_ipc -laf_util
ipc_wait_CFLAGS = -Wall -std=gnu99

.PHONY : build_info.h
$(libaf_ipc_la_SOURCES) : build_info.h
build_info.h :
	$(srcdir)/make-build-info.sh $(srcdir)
