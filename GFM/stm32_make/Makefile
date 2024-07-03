
########################################################################################################################
# Dependency directories
########################################################################################################################

CUBE_DIR		?= STM32CubeF4
CMSIS_DIR 		?= cmsis
MSPDEBUG_DIR 	?= mspdebug
LIBSODIUM_DIR	?= libsodium
TINYAES_DIR		?= tinyaes
MUSL_DIR		?= musl
RSLIB_DIR		?= reed_solomon

########################################################################################################################
# Algorithm parameters
########################################################################################################################

FMEAS_ADC_SAMPLING_RATE ?= 1000
FMEAS_ADC_MAX			?= 4096
FMEAS_FFT_LEN			?= 256
FMEAS_FFT_WINDOW 		?= gaussian
FMEAS_FFT_WINDOW_SIGMA 	?= 16.0
# TODO: validate
FMEAS_SAMPLING_RATE		?= $(shell echo $(FMEAS_ADC_SAMPLING_RATE) / \($(FMEAS_FFT_LEN)/2\) | bc -l)

DSSS_GOLD_CODE_NBITS 	?= 5
DSSS_DECIMATION			?= 10
# TODO maybe auto adjust this based on detection rate?
DSSS_THRESHOLD_FACTOR	?= 5.0f
DSSS_WAVELET_WIDTH		?= 7.3
DSSS_WAVELET_LUT_SIZE	?= 69
DSSS_FILTER_FC			?= 3e-3
DSSS_FILTER_ORDER		?= 12

# Transmission symbols: 20 for 20*6=120 bit key + 10 for reed-solomon ECC
TRANSMISSION_SYMBOLS	?= 30
PRESIG_STORE_SIZE		?= 3

# will be generated if necessary
PRESIG_KEYFILE			?= presig_test_key.secret


########################################################################################################################
# Sources
########################################################################################################################

C_SOURCES		:= src/main.c
C_SOURCES		+= src/mspdebug_wrapper.c
C_SOURCES		+= src/spi_flash.c
C_SOURCES		+= src/freq_meas.c
C_SOURCES		+= src/dsss_demod.c
C_SOURCES		+= src/rslib.c
C_SOURCES		+= src/crypto.c
C_SOURCES		+= src/adc.c
C_SOURCES		+= src/protocol.c
C_SOURCES		+= src/serial.c
C_SOURCES		+= src/con_usart.c
C_SOURCES		+= src/dma_util.c
C_SOURCES		+= src/gpio_helpers.c
C_SOURCES		+= tinyprintf/tinyprintf.c

C_SOURCES		+= $(MSPDEBUG_DIR)/drivers/jtaglib.c

CMSIS_SOURCES		+= TransformFunctions/arm_rfft_fast_init_f32.c
CMSIS_SOURCES		+= TransformFunctions/arm_rfft_fast_f32.c
CMSIS_SOURCES		+= TransformFunctions/arm_cfft_init_f32.c
CMSIS_SOURCES		+= TransformFunctions/arm_cfft_f32.c
CMSIS_SOURCES		+= TransformFunctions/arm_cfft_radix8_f32.c
CMSIS_SOURCES		+= CommonTables/arm_const_structs.c
CMSIS_SOURCES		+= CommonTables/arm_common_tables.c
CMSIS_SOURCES		+= TransformFunctions/arm_bitreversal.c
CMSIS_SOURCES		+= TransformFunctions/arm_bitreversal2.c
CMSIS_SOURCES		:= $(addprefix $(CMSIS_DIR)/CMSIS/DSP/Source/,$(CMSIS_SOURCES))

MUSL_SOURCES		+= math/tanhf.c math/atanhf.c
MUSL_SOURCES		+= math/expm1f.c math/log1pf.c
MUSL_SOURCES		+= math/expf.c math/exp2f_data.c
MUSL_SOURCES		+= math/powf.c
MUSL_SOURCES		+= math/sqrtf.c
MUSL_SOURCES		+= math/fabsf.c
MUSL_SOURCES		+= stdlib/abs.c
MUSL_SOURCES		+= string/memset.c
MUSL_SOURCES		+= string/memcpy.c
MUSL_SOURCES		+= string/memcmp.c
MUSL_SOURCES		+= string/strlen.c
MUSL_SOURCES		+= math/__math_oflowf.c
MUSL_SOURCES		+= math/__math_uflowf.c
MUSL_SOURCES		+= math/__math_xflowf.c
MUSL_SOURCES		+= math/__math_invalidf.c
MUSL_SOURCES		+= math/powf_data.c
MUSL_SOURCES		:= $(addprefix $(MUSL_DIR)/src/,$(MUSL_SOURCES))

RSLIB_SOURCES		+= $(addprefix $(RSLIB_DIR)/src/,rs.c ecc.c berlekamp.c galois.c)

C_SOURCES += $(CMSIS_SOURCES) $(MUSL_SOURCES) $(RSLIB_SOURCES)

CXX_SOURCES			+=

BUILDDIR 	?= build
BINARY 		:= safetyreset.elf
LDSCRIPT 	:= stm32f407.ld

########################################################################################################################
# Build parameters
########################################################################################################################

PREFIX		?= arm-none-eabi-

DEBUG		?= 1

CC			:= $(PREFIX)gcc
CXX			:= $(PREFIX)g++
LD			:= $(PREFIX)gcc
AR			:= $(PREFIX)ar
AS			:= $(PREFIX)as
SIZE		:= $(PREFIX)size
NM			:= $(PREFIX)nm
OBJCOPY		:= $(PREFIX)objcopy
OBJDUMP		:= $(PREFIX)objdump
GDB			:= $(PREFIX)gdb

HOST_CC				?= $(HOST_PREFIX)gcc
HOST_CXX			?= $(HOST_PREFIX)g++
HOST_LD				?= $(HOST_PREFIX)gcc
HOST_AR				?= $(HOST_PREFIX)ar
HOST_AS				?= $(HOST_PREFIX)as
HOST_OBJCOPY		?= $(HOST_PREFIX)objcopy
HOST_OBJDUMP		?= $(HOST_PREFIX)objdump

PYTHON3 ?= python3
DOT		?= dot

CMSIS_DIR_ABS 		:= $(abspath $(CMSIS_DIR))
MSPDEBUG_DIR_ABS 	:= $(abspath $(MSPDEBUG_DIR))
LIBSODIUM_DIR_ABS	:= $(abspath $(LIBSODIUM_DIR))
TINYAES_DIR_ABS		:= $(abspath $(TINYAES_DIR))
MUSL_DIR_ABS		:= $(abspath $(MUSL_DIR))

ARCH_FLAGS		?= -mthumb -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16
SYSTEM_FLAGS	?= -nostdlib -ffreestanding -nostartfiles
#SYSTEM_FLAGS	?= -ffreestanding -nostartfiles

COMMON_CFLAGS		+= -Imspdebug/util -Imspdebug/drivers -Ilevmarq
COMMON_CFLAGS		+= -I$(CMSIS_DIR_ABS)/CMSIS/DSP/Include -I$(CMSIS_DIR_ABS)/CMSIS/Core/Include 
CFLAGS				+= -I$(abspath musl_include_shims)
CFLAGS				+= -Itinyprintf
COMMON_CFLAGS		+= -I$(BUILDDIR) -Isrc -Itinyaes
CFLAGS				+= -I$(CUBE_DIR)/Drivers/CMSIS/Device/ST/STM32F4xx/Include
COMMON_CFLAGS		+= -I$(LIBSODIUM_DIR_ABS)/src/libsodium/include -I$(BUILDDIR)/libsodium/src/libsodium/include -I$(LIBSODIUM_DIR_ABS)/src/libsodium/include/sodium
COMMON_CFLAGS 		+= -I$(RSLIB_DIR)/src

COMMON_CFLAGS	+= -O0 -std=gnu11 -g -DSTM32F407xx -DSTM32F4 -DDEBUG=$(DEBUG)
CFLAGS			+= $(ARCH_FLAGS) $(SYSTEM_FLAGS)
#SIM_CFLAGS		+= -mthumb -mcpu=cortex-m4 -mfloat-abi=soft
CFLAGS			+= -fno-common -ffunction-sections -fdata-sections
COMMON_CFLAGS 	+= -DARM_DSP_CONFIG_TABLES -DARM_FFT_ALLOW_TABLES \
				   -DARM_TABLE_TWIDDLECOEF_F32_128 -DARM_TABLE_BITREVIDX_FLT_128 \
				   -DARM_TABLE_TWIDDLECOEF_F32_128 -DARM_TABLE_TWIDDLECOEF_RFFT_F32_256
COMMON_CFLAGS	+= -DDSSS_GOLD_CODE_NBITS=$(DSSS_GOLD_CODE_NBITS)
COMMON_CFLAGS	+= -DFMEAS_FFT_LEN=$(FMEAS_FFT_LEN)
COMMON_CFLAGS	+= -DFMEAS_ADC_MAX=$(FMEAS_ADC_MAX)
COMMON_CFLAGS	+= -DFMEAS_ADC_SAMPLING_RATE=$(FMEAS_ADC_SAMPLING_RATE)
COMMON_CFLAGS	+= -DFMEAS_FFT_WINDOW_SIGMA=$(FMEAS_FFT_WINDOW_SIGMA)
COMMON_CFLAGS	+= -DDSSS_DECIMATION=$(DSSS_DECIMATION)
COMMON_CFLAGS	+= -DDSSS_THRESHOLD_FACTOR=$(DSSS_THRESHOLD_FACTOR)
COMMON_CFLAGS	+= -DDSSS_WAVELET_WIDTH=$(DSSS_WAVELET_WIDTH)
COMMON_CFLAGS	+= -DDSSS_WAVELET_LUT_SIZE=$(DSSS_WAVELET_LUT_SIZE)
COMMON_CFLAGS	+= -DTRANSMISSION_SYMBOLS=$(TRANSMISSION_SYMBOLS)
COMMON_CFLAGS	+= -DPRESIG_STORE_SIZE=$(PRESIG_STORE_SIZE)

# for musl
CFLAGS			+= -Dhidden=

SIM_CFLAGS	+= -lm -DSIMULATION
SIM_CFLAGS	+= -Wall -Wextra -Wpedantic -Wshadow -Wimplicit-function-declaration -Wundef -Wno-unused-parameter

INT_CFLAGS	+= -Wall -Wextra -Wpedantic -Wshadow -Wimplicit-function-declaration -Wundef -Wno-unused-parameter
INT_CFLAGS	+= -Wredundant-decls -Wmissing-prototypes -Wstrict-prototypes

CXXFLAGS	+= -Os -g
CXXFLAGS	+= $(ARCH_FLAGS) $(SYSTEM_FLAGS)
CXXFLAGS	+= -fno-common -ffunction-sections -fdata-sections
CXXFLAGS	+= -Wall -Wextra -Wshadow -Wundef -Wredundant-decls
CXXFLAGS	+= -I.

LDFLAGS			+= $(ARCH_FLAGS) $(SYSTEM_FLAGS)

LIBS		+= -lgcc
LDFLAGS		+= -Wl,--gc-sections

LINKMEM_FLAGS ?= --trim-stubs=startup_stm32f407xx.o --trace-sections .isr_vector --highlight-subdirs $(BUILDDIR)

OBJS := $(addprefix $(BUILDDIR)/,$(C_SOURCES:.c=.o) $(CXX_SOURCES:.cpp=.o))

ALL_OBJS := $(OBJS)
ALL_OBJS += $(BUILDDIR)/src/startup_stm32f407xx.o
ALL_OBJS += $(BUILDDIR)/src/system_stm32f4xx.o
ALL_OBJS += $(BUILDDIR)/libsodium/src/libsodium/.libs/libsodium.a
ALL_OBJS += $(BUILDDIR)/levmarq/levmarq.o
ALL_OBJS += $(BUILDDIR)/generated/gold_code_$(DSSS_GOLD_CODE_NBITS).o
ALL_OBJS += $(BUILDDIR)/generated/fmeas_fft_window.o
ALL_OBJS += $(BUILDDIR)/generated/dsss_cwt_wavelet.o 
ALL_OBJS += $(BUILDDIR)/generated/crypto_presig_data.o

########################################################################################################################
# Rules
########################################################################################################################

all: binsize

.PHONY: binsize
binsize: $(BUILDDIR)/$(BINARY) $(BUILDDIR)/$(BINARY:.elf=-symbol-sizes.pdf)
	$(LD) -T$(LDSCRIPT) $(LDFLAGS) -Wl,--print-memory-usage -o /dev/null $(ALL_OBJS) $(LIBS)
	@echo
	@echo "▐▬▬▬▌ SyMbOL sIzE HiGhScORe LiSt ▐▬▬▬▌"
	$(NM) --print-size --size-sort --radix=d $< | tail -n 20

# $(BUILDDIR)/generated/dsss_butter_filter.h
src/dsss_demod.c: $(BUILDDIR)/generated/dsss_gold_code.h 

$(BUILDDIR)/generated/dsss_gold_code.h: $(BUILDDIR)/generated/gold_code_$(DSSS_GOLD_CODE_NBITS).h
	ln -srf $< $@

.PRECIOUS: $(BUILDDIR)/generated/gold_code_%.c $(BUILDDIR)/generated/gold_code_%.h
$(BUILDDIR)/generated/gold_code_%.c $(BUILDDIR)/generated/gold_code_%.h&: | $(BUILDDIR)/generated
	$(PYTHON3) tools/gold_code_header_gen.py -v dsss_gold_code_table -c $* > $(BUILDDIR)/generated/gold_code_$*.c
	$(PYTHON3) tools/gold_code_header_gen.py -v dsss_gold_code_table -h $* > $(BUILDDIR)/generated/gold_code_$*.h

.PRECIOUS: $(BUILDDIR)/generated/fmeas_fft_window.c
$(BUILDDIR)/generated/fmeas_fft_window.c: | $(BUILDDIR)/generated
	$(PYTHON3) tools/fft_window_header_gen.py -v fmeas_fft_window_table $(FMEAS_FFT_WINDOW) $(FMEAS_FFT_LEN) $(FMEAS_FFT_WINDOW_SIGMA) > $@

.PRECIOUS: $(BUILDDIR)/generated/dsss_cwt_wavelet.c
$(BUILDDIR)/generated/dsss_cwt_wavelet.c: | $(BUILDDIR)/generated
	$(PYTHON3) tools/cwt_wavelet_header_gen.py -v dsss_cwt_wavelet_table $(DSSS_WAVELET_LUT_SIZE) $(DSSS_WAVELET_WIDTH) > $@

.PRECIOUS: $(BUILDDIR)/generated/dsss_butter_filter.h
$(BUILDDIR)/generated/dsss_butter_filter.h: | $(BUILDDIR)/generated
	$(PYTHON3) tools/butter_filter_gen.py -m dsss_filter $(DSSS_FILTER_FC) $(FMEAS_SAMPLING_RATE) $(DSSS_FILTER_ORDER) > $@

.PRECIOUS: $(BUILDDIR)/generated/crypto_presig_data.c
$(BUILDDIR)/generated/crypto_presig_data.c: $(PRESIG_KEYFILE) tools/presig_gen.py | $(BUILDDIR)/generated
	$(PYTHON3) tools/presig_gen.py $(PRESIG_KEYFILE) prekey > $@

.PRECIOUS: $(PRESIG_KEYFILE)
$(PRESIG_KEYFILE):
	$(PYTHON3) tools/presig_gen.py $@ keygen

$(BUILDDIR)/generated: ; mkdir -p $@

.PRECIOUS: $(BUILDDIR)/$(BINARY) 
$(BUILDDIR)/$(BINARY) $(BUILDDIR)/$(BINARY:.elf=.map) &: $(ALL_OBJS)
	$(LD) -T$(LDSCRIPT) $(LDFLAGS) -o $@ -Wl,-Map=$(BUILDDIR)/$(BINARY:.elf=.map) $^ $(LIBS)

build/$(BINARY:.elf=-symbol-sizes.dot): $(ALL_OBJS)
	$(PYTHON3) tools/linkmem.py $(LINKMEM_FLAGS) $(LD) -T$(LDSCRIPT) $(LDFLAGS) $^ $(LIBS) > $@

%.pdf: %.dot
	$(DOT) -T pdf $< -o $@

%.dot: %.elf
	r2 -a arm -qc 'aa;agRd' $< 2>/dev/null >$@

tools: $(BUILDDIR)/tools/freq_meas_test
$(BUILDDIR)/tools/freq_meas_test: tools/freq_meas_test.c src/freq_meas.c levmarq/levmarq.c $(BUILDDIR)/generated/fmeas_fft_window.c $(CMSIS_SOURCES)
	mkdir -p $(@D)
	$(HOST_CC) $(COMMON_CFLAGS) $(SIM_CFLAGS) -o $@ $^

tools: $(BUILDDIR)/tools/dsss_demod_test
$(BUILDDIR)/tools/dsss_demod_test: tools/dsss_demod_test.c src/dsss_demod.c $(BUILDDIR)/generated/dsss_cwt_wavelet.c $(BUILDDIR)/generated/gold_code_$(DSSS_GOLD_CODE_NBITS).c
	mkdir -p $(@D)
	$(HOST_CC) $(COMMON_CFLAGS) $(SIM_CFLAGS) -o $@ $^

tools: $(BUILDDIR)/tools/crypto_test
$(BUILDDIR)/tools/crypto_test: tools/crypto_test.c src/crypto.c tinyaes/aes.c $(BUILDDIR)/generated/crypto_presig_data.c
	mkdir -p $(@D)
	$(HOST_CC) $(COMMON_CFLAGS) $(SIM_CFLAGS) -lsodium -o $@ $^

tools: $(BUILDDIR)/tools/e2e_test
$(BUILDDIR)/tools/e2e_test: tools/e2e_test.c src/freq_meas.c levmarq/levmarq.c $(BUILDDIR)/generated/fmeas_fft_window.c $(CMSIS_SOURCES) src/dsss_demod.c $(BUILDDIR)/generated/dsss_cwt_wavelet.c $(BUILDDIR)/generated/gold_code_$(DSSS_GOLD_CODE_NBITS).c
	mkdir -p $(@D)
	$(HOST_CC) $(COMMON_CFLAGS) $(SIM_CFLAGS) -o $@ $^

$(BUILDDIR)/src/%.o: src/%.s
	mkdir -p $(@D)
	$(CC) $(COMMON_CFLAGS) $(CFLAGS) $(INT_CFLAGS) -o $@ -c $<

$(BUILDDIR)/src/%.o: src/%.c
	mkdir -p $(@D)
	$(CC) $(COMMON_CFLAGS) $(CFLAGS) $(INT_CFLAGS) -o $@ -c $<

$(BUILDDIR)/src/%.o: src/%.cpp
	mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -o $@ -c $<

$(BUILDDIR)/generated/%.o: $(BUILDDIR)/generated/%.c
	mkdir -p $(@D)
	$(CC) $(COMMON_CFLAGS) $(CFLAGS) $(INT_CFLAGS) -o $@ -c $<

$(BUILDDIR)/%.o: %.c
	mkdir -p $(@D)
	$(CC) $(COMMON_CFLAGS) $(CFLAGS) $(EXT_CFLAGS) -o $@ -c $<

$(BUILDDIR)/src/crypto.o: $(BUILDDIR)/libsodium/src/libsodium/include/sodium/version.h

$(BUILDDIR)/libsodium/src/libsodium/.libs/libsodium.a $(BUILDDIR)/libsodium/src/libsodium/include/sodium/version.h &:
	mkdir -p $(BUILDDIR)/libsodium
	cd $(BUILDDIR)/libsodium && CFLAGS="$(COMMON_CFLAGS) $(CFLAGS) -DDEV_MODE=1" $(LIBSODIUM_DIR_ABS)/configure --host=arm-none-eabi && $(MAKE) -j $(shell nproc)

$(BUILDDIR)/tinyaes/aes.o:
	mkdir -p $(@D)
	make -C $(@D) -f $(TINYAES_DIR_ABS)/Makefile VPATH=$(TINYAES_DIR_ABS) CFLAGS="$(COMMON_CFLAGS) $(CFLAGS) -c" CC=$(CC) LD=$(LD) AR=$(AR) aes.o

$(BUILDDIR)/musl/lib/libc.a:
	mkdir -p $(BUILDDIR)/musl
	cd $(BUILDDIR)/musl && CFLAGS="$(SIM_CFLAGS) $(COMMON_CFLAGS)" CC=$(CC) LD=$(LD) AR=$(AR) $(MUSL_DIR_ABS)/configure && $(MAKE) TARGET=arm-linux-musleabihf GCC_CONFIG="-mcpu=cortex-m4 -mfloat-abi=hard" -j $(shell nproc)

build/rslib.so: $(RSLIB_SOURCES) src/rslib.c
	gcc -fPIC -shared -Wall -Wextra -Wpedantic -std=gnu11 -O0 -g -o $@ -Isrc -I$(RSLIB_DIR)/src $^

clean:
	rm -rf $(BUILDDIR)/src
	rm -rf $(BUILDDIR)/generated
	rm -f $(BUILDDIR)/$(BINARY)
	rm -f $(BUILDDIR)/$(BINARY:.elf=.map)
	rm -f $(BUILDDIR)/$(BINARY:.elf=-symbol-sizes.dot)
	rm -f $(BUILDDIR)/$(BINARY:.elf=-symbol-sizes.pdf)
	rm -f $(BUILDDIR)/tools/freq_meas_test

mrproper: clean
	rm -rf build

.PHONY: clean mrproper

-include $(OBJS:.o=.d)
