package=zeromq
$(package)_version=4.3.4
$(package)_download_path=https://github.com/zeromq/libzmq/releases/download/v$($(package)_version)/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=c593001a89f5a85dd2ddf564805deb860e02471171b3f204944857336295c3e5

define $(package)_set_vars
$(package)_config_opts=--without-docs --disable-shared --enable-static --disable-dependency-tracking --without-libsodium --enable-curve=no --disable-libunwind --disable-perf --disable-Werror
endef

define $(package)_config_cmds
	./configure $(host) $($(package)_config_opts)
endef

define $(package)_build_cmds
	$(MAKE)
endef

define $(package)_stage_cmds
	$(MAKE) DESTDIR=$($(package)_staging_dir) install
	mkdir -p $($(package)_staging_dir)$(host_prefix)/lib
	# FIX: Search for libzmq.a and force-install it to the correct staging path
	find . -name libzmq.a -exec install -D -m 644 {} $($(package)_staging_dir)$(host_prefix)/lib/libzmq.a \;
endef
