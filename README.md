KORE Core integration/staging repository
=====================================

KORE is an open source crypto-currency focused on security and privacy with fast private transactions consisting of low transaction fees & environmental footprint.  It utilizes a custom Proof of Stake protocol for securing its network.


### Coin Specs

<table>
<tr><td>Pre Fork Algo</td><td>Momentum</td></tr>
<tr><td>Post Fork Algo</td><td>Yescrypt R32</td></tr>
<tr><td>Block Time</td><td>60 Seconds</td></tr>
<tr><td>Difficulty Retargeting</td><td>Every Block</td></tr>
<tr><td>Max Coin Supply</td><td>12,000,000 KORE</td></tr>
</table>



### PoS Rewards Breakdown

To be Added!!!

### Installation Steps

Note1: that you can speed up the compilation using the option -j when using make, for example: make -j3

Note2: If you machine has less than 3G memory, you should use a swapfile.

a) enabling swap

```bash
    sudo dd if=/dev/zero of=/swapfile bs=4096 count=1048576
    sudo chmod 600 /swapfile
    sudo mkswap /swapfile
    sudo swapon /swapfile
```

b) disabling swap

```bash
    sudo swapoff -v /swapfile
    sudo rm /swapfile
```

### 1. Installating dependencies

```bash
sudo apt-get update
sudo apt-get install -y git curl jq wget bsdmainutils rsync
sudo apt-get install -y software-properties-common
sudo apt-get install -y autotools-dev autoconf automake build-essential
sudo apt-get install -y qttools5-dev-tools qttools5-dev libprotobuf-dev libqrencode-dev
sudo apt-get install -y libtool pkg-config protobuf-compiler python3
sudo apt-get install -y devscripts debhelper


```

### 2. Building KORE dependencies

```bash
cd depends
make
```

### 3. Building KORE source

```bash
cd ..
./autogen.sh
./configure --with-gui=qt5 --prefix=$(pwd)/depends/x86_64-pc-linux-gnu --disable-tests --enable-tor-browser --disable-dependency-tracking --disable-maintainer-mode MOC=/usr/lib/qt5/bin/moc UIC=/usr/lib/qt5/bin/uic RCC=/usr/lib/qt5/bin/rcc LRELEASE=/usr/lib/qt5/bin/lrelease
make -j$(nproc)
```

### 4. Generating the installer (.deb)

#### First, Install Go
```bash
sudo apt-get install -y golang-go
```

#### Fourth, generate the (.deb)
```bash
from the kore root directory, give the command:
make deploy
```

### 5. Installing kore (.deb)
```bash
  The installer is generated in the share folder, so in order to install it, give the following command:
  cd <kore-dir>/share
  sudo apt install ./kore_<version>_amd64.deb
  * <kore-dir> is the directory where you download the kore git repository
  * <version> is this source code version
  ** if you get problems with old version, you can remove with the command:
    sudo apt-get remove kore --purge -y
```

---

## Building on Older Systems (Ubuntu 16.04 / Linux Mint 18.x)

This section provides additional instructions for building KORE on older Linux distributions such as Ubuntu 16.04 LTS and Linux Mint 18.x.

### Additional Dependencies for Older Systems

On Ubuntu 16.04 / Mint 18.x, you need additional libraries for Qt5 linking:

```bash
sudo apt-get install -y \
  libproxy-dev \
  libglib2.0-dev \
  libpng-dev \
  libharfbuzz-dev \
  libicu-dev \
  libpcre3-dev
```

### Qt5 Tool Paths

The Qt5 tool paths differ between distributions. Check your paths:

```bash
ls /usr/lib/*/qt5/bin/
```

**Ubuntu 22.04 / Newer systems:**
```
MOC=/usr/lib/qt5/bin/moc
UIC=/usr/lib/qt5/bin/uic
RCC=/usr/lib/qt5/bin/rcc
LRELEASE=/usr/lib/qt5/bin/lrelease
```

**Ubuntu 16.04 / Mint 18.x:**
```
MOC=/usr/lib/x86_64-linux-gnu/qt5/bin/moc
UIC=/usr/lib/x86_64-linux-gnu/qt5/bin/uic
RCC=/usr/lib/x86_64-linux-gnu/qt5/bin/rcc
LRELEASE=/usr/lib/x86_64-linux-gnu/qt5/bin/lrelease
```

### Full Build Commands for Mint 18.x / Ubuntu 16.04

```bash
# 1. Install all dependencies
sudo apt-get update
sudo apt-get install -y git curl jq wget bsdmainutils rsync
sudo apt-get install -y software-properties-common
sudo apt-get install -y autotools-dev autoconf automake build-essential
sudo apt-get install -y qttools5-dev-tools qttools5-dev libprotobuf-dev libqrencode-dev
sudo apt-get install -y libtool pkg-config protobuf-compiler python3
sudo apt-get install -y devscripts debhelper
sudo apt-get install -y libproxy-dev libglib2.0-dev libpng-dev libharfbuzz-dev libicu-dev libpcre3-dev

# 2. Clone repository
git clone https://github.com/AluNode/KORE.git
cd KORE

# 3. Build depends
cd depends
make
cd ..

# 4. Generate build files
./autogen.sh

# 5. Configure with correct Qt5 paths for Mint 18.x
./configure --with-gui=qt5 \
  --prefix=$(pwd)/depends/x86_64-pc-linux-gnu \
  --disable-tests \
  --enable-tor-browser \
  --disable-dependency-tracking \
  --disable-maintainer-mode \
  MOC=/usr/lib/x86_64-linux-gnu/qt5/bin/moc \
  UIC=/usr/lib/x86_64-linux-gnu/qt5/bin/uic \
  RCC=/usr/lib/x86_64-linux-gnu/qt5/bin/rcc \
  LRELEASE=/usr/lib/x86_64-linux-gnu/qt5/bin/lrelease

# 6. Build
make -j$(nproc)
```

### Troubleshooting

#### Error: "cannot find -lproxy" or similar library errors
Install the missing development libraries:
```bash
sudo apt-get install -y libproxy-dev libglib2.0-dev libpng-dev libharfbuzz-dev libicu-dev libpcre3-dev
```

#### Error: "Autoconf version 2.71 or higher is required"
This has been fixed in the source. If you encounter this on an older checkout, ensure you have the latest code:
```bash
git pull origin test2
```

#### Error: "relocation R_X86_64_32S against ... can not be used when making a shared object; recompile with -fPIC"
This occurs when static libraries are not compiled with position-independent code. This has been fixed in the depends system. Ensure you have the latest code and rebuild depends:
```bash
git pull origin test2
cd depends
rm -rf built work
make
```

### Build Output

After successful compilation, you will find:
- `src/kored` - KORE daemon
- `src/kore-cli` - Command-line interface
- `src/kore-tx` - Transaction utility
- `src/qt/kore-qt` - Qt5 GUI wallet

