package=zlib
$(package)_version=1.3.1
$(package)_download_path=https://github.com/madler/zlib/releases/download/v1.3.1/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23

define $(package)_set_vars
$(package)_config_opts=--static --prefix=$(host_prefix)
$(package)_cflags_linux=-fPIC
endef

define $(package)_config_cmds
  CC="$($(package)_cc)" AR="$($(package)_ar)" RANLIB="$($(package)_ranlib)" CFLAGS="$($(package)_cflags)" ./configure $($(package)_config_opts)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) install DESTDIR=$($(package)_staging_dir)
endef
