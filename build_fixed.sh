#!/bin/bash
set -e # Stop on error

echo "=== 1. Cleaning Repository ==="
make distclean 2>/dev/null || true
rm -rf autom4te.cache config.status config.log configure
cd src/univalue && make distclean 2>/dev/null || true && rm -rf autom4te.cache && cd ../..
cd src/secp256k1 && make distclean 2>/dev/null || true && rm -rf autom4te.cache && cd ../..

echo "=== 2. Running Root Autogen ==="
./autogen.sh

echo "=== 3. Patching Univalue (Fixing Paths & Tests) ==="
# We overwrite Makefile.am to point correctly to lib/ and include/ and remove broken tests
cat << 'MAKE_UNI' > src/univalue/Makefile.am
ACLOCAL_AMFLAGS = -I build-aux/m4
lib_LTLIBRARIES = libunivalue.la
include_HEADERS = include/univalue.h
noinst_HEADERS = lib/univalue_escapes.h lib/univalue_utffilter.h
libunivalue_la_SOURCES = lib/univalue.cpp lib/univalue_read.cpp lib/univalue_write.cpp
libunivalue_la_CXXFLAGS = -I$(top_srcdir)/include
libunivalue_la_LDFLAGS = -version-info 100:3:100
MAKE_UNI

echo "=== 4. Patching Main Makefile (Fixing Linker Errors) ==="
# We ensure every executable links against Boost, BDB, and UPnP
LIBS_FIX="\$(BOOST_LIBS) \$(BDB_LIBS) \$(MINIUPNPC_LIBS) \$(EVENT_PTHREADS_LIBS) \$(EVENT_LIBS) \$(SSL_LIBS) \$(CRYPTO_LIBS)"
sed -i "s|^kored_LDADD =.*|kored_LDADD = \$(LIBBITCOIN_SERVER) \$(LIBBITCOIN_COMMON) \$(LIBUNIVALUE) \$(LIBBITCOIN_UTIL) \$(LIBBITCOIN_WALLET) \$(LIBBITCOIN_CRYPTO) \$(LIBLEVELDB) \$(LIBMEMENV) \$(LIBSECP256K1) $LIBS_FIX|" src/Makefile.am
sed -i "s|^kore_cli_LDADD =.*|kore_cli_LDADD = \$(LIBBITCOIN_CLI) \$(LIBBITCOIN_UTIL) \$(LIBBITCOIN_CRYPTO) \$(LIBSECP256K1) $LIBS_FIX|" src/Makefile.am
sed -i "s|^kore_tx_LDADD =.*|kore_tx_LDADD = \$(LIBBITCOIN_UNIVALUE) \$(LIBBITCOIN_COMMON) \$(LIBBITCOIN_UTIL) \$(LIBBITCOIN_CRYPTO) \$(LIBSECP256K1) \$(LIBUNIVALUE) $LIBS_FIX|" src/Makefile.am
sed -i "s|^qt_kore_qt_LDADD =.*|qt_kore_qt_LDADD = \$(LIBBITCOINQT) \$(LIBBITCOIN_SERVER) \$(LIBBITCOIN_WALLET) \$(LIBBITCOIN_COMMON) \$(LIBUNIVALUE) \$(LIBBITCOIN_UTIL) \$(LIBBITCOIN_CRYPTO) \$(LIBLEVELDB) \$(LIBMEMENV) \$(LIBSECP256K1) $LIBS_FIX|" src/Makefile.am

# Fix the specific univalue linking flag to avoid "cannot find -lunivalue"
sed -i 's/-lunivalue/univalue\/libunivalue.la/g' src/Makefile.am

echo "=== 5. Manually Building Submodules (Crucial Step) ==="
# We build these NOW so the main build doesn't try (and fail) to configure them recursively

# Build Univalue
echo "--> Building Univalue..."
cd src/univalue
autoreconf -fi
./configure --disable-shared --with-pic
make
cd ../..

# Build Secp256k1
echo "--> Building Secp256k1..."
cd src/secp256k1
autoreconf -fi
./configure --disable-shared --with-pic --enable-module-recovery
make
cd ../..

echo "=== 6. Main Configuration ==="
# Detect Qt
QT_BIN=$(dirname $(find /usr -name moc -type f 2>/dev/null | grep qt5 | head -n 1))
echo "Qt found at: $QT_BIN"

./configure --with-gui=qt5 \
            --prefix=$(pwd)/depends/x86_64-pc-linux-gnu \
            --disable-tests \
            --enable-tor-browser \
            --disable-dependency-tracking \
            MOC="$QT_BIN/moc" \
            UIC="$QT_BIN/uic" \
            RCC="$QT_BIN/rcc" \
            LRELEASE="$QT_BIN/lrelease"

echo "=== 7. Final Compilation ==="
make -j$(nproc)

echo "=== SUCCESS! Run 'make deploy' to generate the installer. ==="
