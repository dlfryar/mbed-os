#  mbed-bootloader-spif-nvstore-v3_5_0.hex

[GitHub/ARMmbed/mbed-bootloader/v3.5.0](https://github.com/ARMmbed/mbed-bootloader/tree/v3.5.0)

### Mbed Tools Build Syntax

mbed compile -m NRF52840_DK -t GCC_ARM --profile minimal-printf/profiles/release.json

### Additional Drivers

[SPIF Driver](https://github.com/ARMmbed/spif-driver/#39a918e5d0bfc7b5e6ab96228cc68e00cc93f9a2) used to build 3.5.0 Mbed Bootloader

Add the SPIF driver to mbed-bootloader project

```mbed add https://github.com/ARMmbed/spif-driver/#39a918e5d0bfc7b5e6ab96228cc68e00cc93f9a2```

Code changes to instanciate SPIF driver instead of the SD driver

root@localhost:~/Source/mbed-bootloader# git diff source/main.cpp

```
diff --git a/source/main.cpp b/source/main.cpp
index 37433c7..a568dcc 100755
--- a/source/main.cpp
+++ b/source/main.cpp
@@ -63,7 +63,7 @@ extern ARM_UC_PAAL_UPDATE MBED_CLOUD_CLIENT_UPDATE_STORAGE;
 #endif

 #if defined(ARM_UC_USE_PAL_BLOCKDEVICE) && (ARM_UC_USE_PAL_BLOCKDEVICE==1)
-#include "SDBlockDevice.h"
+#include "SPIFBlockDevice.h"

 /* initialise sd card blockdevice */
 #if defined(MBED_CONF_APP_SPI_MOSI) && defined(MBED_CONF_APP_SPI_MISO) && \
@@ -71,8 +71,8 @@ extern ARM_UC_PAAL_UPDATE MBED_CLOUD_CLIENT_UPDATE_STORAGE;
 SDBlockDevice sd(MBED_CONF_APP_SPI_MOSI, MBED_CONF_APP_SPI_MISO,
                  MBED_CONF_APP_SPI_CLK,  MBED_CONF_APP_SPI_CS);
 #else
-SDBlockDevice sd(MBED_CONF_SD_SPI_MOSI, MBED_CONF_SD_SPI_MISO,
-                 MBED_CONF_SD_SPI_CLK,  MBED_CONF_SD_SPI_CS);
+SPIFBlockDevice sd(MBED_CONF_SPIF_DRIVER_SPI_MOSI, MBED_CONF_SPIF_DRIVER_SPI_MISO,
+                   MBED_CONF_SPIF_DRIVER_SPI_CLK, MBED_CONF_SPIF_DRIVER_SPI_CS);
 #endif

 BlockDevice *arm_uc_blockdevice = &sd;
 ```

### Build OS

lsb_release -a

```
No LSB modules are available.
Distributor ID:	Ubuntu
Description:	Ubuntu 18.04.1 LTS
Release:	18.04
Codename:	bionic
```

### Cross Compiler Version

arm-none-eabi-gcc --version

```
arm-none-eabi-gcc (GNU Tools for Arm Embedded Processors 7-2018-q3-update) 7.3.1 20180622 (release) [ARM/embedded-7-branch revision 261907]
```

### Mbed Tools Version

pip freeze | grep -i mbed-cli

```
mbed-cli==1.8.2
```