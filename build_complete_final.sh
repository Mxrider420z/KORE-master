#!/bin/bash
set -e

echo "=== 1. Cleaning Up ==="
make distclean 2>/dev/null || true
rm -rf autom4te.cache config.status config.log configure
cd src/univalue && make distclean 2>/dev/null || true && rm -rf autom4te.cache config.status config.log configure && cd ../..
cd src/secp256k1 && make distclean 2>/dev/null || true && rm -rf autom4te.cache config.status config.log configure && cd ../..

echo "=== 2. Generating Build System ==="
./autogen.sh

echo "=== 3. Fixing Univalue ==="
cat << 'MAKE_UNI' > src/univalue/Makefile.am
ACLOCAL_AMFLAGS = -I build-aux/m4
lib_LTLIBRARIES = libunivalue.la
include_HEADERS = include/univalue.h
noinst_HEADERS = lib/univalue_escapes.h lib/univalue_utffilter.h
libunivalue_la_SOURCES = lib/univalue.cpp lib/univalue_read.cpp lib/univalue_write.cpp
libunivalue_la_CXXFLAGS = -I$(top_srcdir)/include
libunivalue_la_LDFLAGS = -version-info 100:3:100
MAKE_UNI

echo "=== 4. Patching Makefile.am (The Missing Link) ==="
# We add $(LIBBITCOIN_CLI) to qt_kore_qt_LDADD to fix the RPCConvertValues error.
LIBS_FIX="\$(BOOST_LIBS) \$(BDB_LIBS) \$(MINIUPNPC_LIBS) \$(EVENT_PTHREADS_LIBS) \$(EVENT_LIBS) \$(SSL_LIBS) \$(CRYPTO_LIBS) \$(LIBTOR)"
QT_LIBS_FIX="\$(QT_LIBS) \$(QT_DBUS_LIBS) \$(QR_LIBS) \$(PROTOBUF_LIBS)"

# kored
sed -i "s|^kored_LDADD =.*|kored_LDADD = \$(LIBBITCOIN_SERVER) \$(LIBBITCOIN_COMMON) \$(LIBUNIVALUE) \$(LIBBITCOIN_UTIL) \$(LIBBITCOIN_WALLET) \$(LIBBITCOIN_CRYPTO) \$(LIBLEVELDB) \$(LIBMEMENV) \$(LIBSECP256K1) $LIBS_FIX|" src/Makefile.am

# kore-cli
sed -i "s|^kore_cli_LDADD =.*|kore_cli_LDADD = \$(LIBBITCOIN_CLI) \$(LIBBITCOIN_UTIL) \$(LIBBITCOIN_CRYPTO) \$(LIBSECP256K1) \$(LIBUNIVALUE) $LIBS_FIX|" src/Makefile.am

# kore-tx
sed -i "s|^kore_tx_LDADD =.*|kore_tx_LDADD = \$(LIBBITCOIN_UNIVALUE) \$(LIBBITCOIN_COMMON) \$(LIBBITCOIN_UTIL) \$(LIBBITCOIN_CRYPTO) \$(LIBSECP256K1) \$(LIBUNIVALUE) $LIBS_FIX|" src/Makefile.am

# kore-qt (Added $(LIBBITCOIN_CLI) here!)
sed -i "s|^qt_kore_qt_LDADD =.*|qt_kore_qt_LDADD = \$(LIBBITCOINQT) \$(LIBBITCOIN_SERVER) \$(LIBBITCOIN_WALLET) \$(LIBBITCOIN_COMMON) \$(LIBUNIVALUE) \$(LIBBITCOIN_UTIL) \$(LIBBITCOIN_CRYPTO) \$(LIBLEVELDB) \$(LIBMEMENV) \$(LIBSECP256K1) \$(LIBBITCOIN_CLI) $LIBS_FIX $QT_LIBS_FIX|" src/Makefile.am

# Fix univalue linking flag
sed -i 's/-lunivalue/univalue\/libunivalue.la/g' src/Makefile.am

echo "=== 5. Building Submodules ==="
echo "--> Univalue"
cd src/univalue
autoreconf -fi
./configure --disable-shared --with-pic --disable-maintainer-mode
make
cd ../..

echo "--> Secp256k1"
cd src/secp256k1
autoreconf -fi
./configure --disable-shared --with-pic --enable-module-recovery --disable-maintainer-mode
make
cd ../..

echo "=== 6. Configuring Main Build ==="
QT_BIN=$(dirname $(find /usr -name moc -type f 2>/dev/null | grep qt5 | head -n 1))
./configure --with-gui=qt5 \
            --prefix=$(pwd)/depends/x86_64-pc-linux-gnu \
            --disable-tests \
            --enable-tor-browser \
            --disable-dependency-tracking \
            --disable-maintainer-mode \
            MOC="$QT_BIN/moc" \
            UIC="$QT_BIN/uic" \
            RCC="$QT_BIN/rcc" \
            LRELEASE="$QT_BIN/lrelease"

echo "=== 7. Compiling ==="
make -j$(nproc)

echo "=== SUCCESS! Running make deploy... ==="
make deploy
