#
# (C) Copyright 2003
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.
#
# See file CREDITS for list of people who contributed to this
# project.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston,
# MA 02111-1307 USA
#
#
include $(TOPDIR)/config.mk

CFLAGS += -I$(TOPDIR)/arch/mips/include/asm/arch-octeon

#LIB	= $(obj)lib$(BOARD).a mips.lds
LIB	= $(obj)lib$(CPU).o

COBJS-y	=  	octeon_board_common.o \
		octeon_board_display.o \
		octeon_board_stubs.o \
		octeon_boot_bus_moveable.o
SOBJS	=
COBJS-$(CONFIG_OCTEON_BOOT_BUS) += octeon_boot_bus.o
COBJS-$(CONFIG_CMD_IDE)		+= octeon_board_cf.o
COBJS-$(CONFIG_PCI)		+= octeon_board_pci.o
COBJS-$(CONFIG_OCTEON_MMC)	+= octeon_board_mmc.o
COBJS-$(CONFIG_BOOTP_VENDOREX)	+= octeon_board_dhcp.o
COBJS-$(CONFIG_OF_LIBFDT)	+= octeon_board_phy.o

SRCS	:= $(SOBJS:.o=.S) $(COBJS:.o=.c) $(COBJS-y:.o=.c)
OBJS	:= $(addprefix $(obj),$(COBJS-y))
SOBJS	:= $(addprefix $(obj),$(SOBJS))

$(LIB):	$(obj).depend $(OBJS) $(SOBJS)
	$(call cmd_link_o_target, $(OBJS) $(SOBJS))

#mips.lds	: u-boot.lds
#	ln -sf u-boot.lds mips.lds
#	ln -sf u-boot.lds mips-octeon.lds

#########################################################################
include $(SRCTREE)/rules.mk

sinclude $(obj).depend
