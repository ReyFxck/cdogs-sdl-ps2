# Compiles prepared copies of the pinned SDK sources, not the SDK checkout.
IOP_BIN = cdogs_cdfs.irx
IOP_OBJS = main.o cdfs_iop.o imports.o
IOP_IMPORT_INCS += cdvd/cdvdman debug/sior system/intrman system/ioman \
  system/loadcore system/sifcmd system/sifman system/stdio system/sysclib \
  system/sysmem system/threadman
include $(PS2SDKSRC)/Defs.make
include $(PS2SDKSRC)/iop/Rules.bin.make
include $(PS2SDKSRC)/iop/Rules.make
