package=zeromq
$(package)_version=4.3.5
$(package)_download_path=https://github.com/zeromq/libzmq/releases/download/v$($(package)_version)
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=6653ef5910f17954861fe72332e68b03ca6e4d9c7160eb3a8de5a5a913bfab43

define $(package)_set_vars
$(package)_config_opts=--without-docs --disable-shared --enable-static
$(package)_config_opts+=--disable-dependency-tracking --without-libsodium
$(package)_config_opts+=--disable-curve --disable-libunwind --disable-perf
$(package)_config_opts+=--disable-Werror --disable-drafts
$(package)_config_opts_linux=--with-pic
$(package)_config_opts_mingw32=--enable-static --disable-shared
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
  rm -rf share
endef
