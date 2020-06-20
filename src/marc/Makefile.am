bin_PROGRAMS = marc
marc_CPPFLAGS = -I$(top_srcdir)/include
marc_SOURCES = ClientManager.cpp  ClientNode.cpp  main.cpp  MasterListener.cpp  MasterNode.cpp  ResultNode.cpp  ResultNodeManager.cpp  TaskManager.cpp  TypeDefs.cpp HttpServer.cpp
marc_LDADD = ../utils/libutils.la ../sftp/libsftp.la $(top_srcdir)/lib/libevent.a -lpthread -lrt
