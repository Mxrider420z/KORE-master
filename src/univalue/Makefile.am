ACLOCAL_AMFLAGS = -I build-aux/m4
lib_LTLIBRARIES = libunivalue.la
include_HEADERS = include/univalue.h
noinst_HEADERS = lib/univalue_escapes.h lib/univalue_utffilter.h
libunivalue_la_SOURCES = lib/univalue.cpp lib/univalue_read.cpp lib/univalue_write.cpp
libunivalue_la_CXXFLAGS = -I$(top_srcdir)/include
libunivalue_la_LDFLAGS = -version-info 100:3:100
