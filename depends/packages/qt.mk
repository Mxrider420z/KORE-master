package=qt
$(package)_version=5.15.5
$(package)_download_path=https://download.qt.io/archive/qt/5.15/$($(package)_version)/single
$(package)_file_name=qt-everywhere-opensource-src-$($(package)_version).tar.xz
$(package)_sha256_hash=5a97827bdf9fd515f43bc7651defaf64fecb7a55e051c79b8f80510d0e990f06
$(package)_dependencies=openssl zlib

define $(package)_set_vars
$(package)_config_opts = \
  -commercial \
  -confirm-license \
  -no-compile-examples \
  -no-cups \
  -no-eglfs \
  -no-glib \
  -no-icu \
  -no-linuxfb \
  -no-opengl \
  -no-qml-debug \
  -no-sql-sqlite \
  -no-use-gold-linker \
  -no-xcb \
  -nomake examples \
  -nomake tests \
  -opensource \
  -openssl-linked \
  -platform linux-g++ \
  -qt-libpng \
  -qt-libjpeg \
  -qt-pcre \
  -qt-zlib \
  -static \
  -silent \
  -v \
  -no-feature-bearermanagement \
  -no-feature-colordialog \
  -no-feature-concurrent \
  -no-feature-dial \
  -no-feature-fontcombobox \
  -no-feature-ftp \
  -no-feature-keysequenceedit \
  -no-feature-lcdnumber \
  -no-feature-pdf \
  -no-feature-printdialog \
  -no-feature-printer \
  -no-feature-printpreviewdialog \
  -no-feature-printpreviewwidget \
  -no-feature-sql \
  -no-feature-statemachine \
  -no-feature-syntaxhighlighter \
  -no-feature-textbrowser \
  -no-feature-textodfwriter \
  -no-feature-topleveldomain \
  -no-feature-udpsocket \
  -no-feature-undostack \
  -no-feature-validator \
  -no-feature-wizard \
  -no-feature-xml \
  --prefix=$(host_prefix)

$(package)_config_opts_linux = -xcb
$(package)_build_env =
endef

define $(package)_config_cmds
  ./configure $($(package)_config_opts)
endef

define $(package)_build_cmds
  $(MAKE) -j$(nproc)
endef

define $(package)_stage_cmds
  $(MAKE) -j$(nproc) install DESTDIR=$([$(package)_staging_dir])
endef
