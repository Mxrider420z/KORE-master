package=univalue
$(package)_version=1.0.4
$(package)_download_path=https://github.com/jgarzik/univalue/archive
$(package)_file_name=v$($(package)_version).tar.gz
$(package)_sha256_hash=2576b509f6d498d30c5e7b255e2e4e11e86055d21a2eb443b0d2d2a454d6de18
$(package)_dependencies=

define $(package)_set_vars
$(package)_config_opts=--enable-static --disable-shared --disable-dependency-tracking --with-pic --enable-benchmark=no
endef

define $(package)_config_cmds
	./autogen.sh && ./configure $(host) $($(package)_config_opts)
endef

define $(package)_build_cmds
	$(MAKE)
endef

define $(package)_stage_cmds
	$(MAKE) DESTDIR=$($(package)_staging_dir) install
	mkdir -p $($(package)_staging_dir)$(host_prefix)/lib
	# FIX: Find the file and force it into the staging directory
	find . -name libunivalue.a -exec install -D -m 644 {} $($(package)_staging_dir)$(host_prefix)/lib/libunivalue.a \;
endef
