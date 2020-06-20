noinst_LTLIBRARIES = libsftp.la
bin_PROGRAMS = sftp_test

libsftp_la_CPPFLAGS =
libsftp_la_SOURCES = sftp_client.cpp sftp_server.cpp
libsftp_la_LIBADD = -lpthread

sftp_test_CPPFLAGS = 
sftp_test_SOURCES = sftp_test.cpp
sftp_test_LDADD = libsftp.la ../utils/libutils.la
