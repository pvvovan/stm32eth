TARGET = f407disc1
DEBUG = 1

BUILD_DIR = build
LWIPBUILD_DIR = $(BUILD_DIR)/lwIPbuild

C_SOURCES = \
Src/main.c \
Src/sys_arch.c \
Src/syscalls.c \
Src/sysmem.c \
Src/ethif.c \
cmsis_device_f4/Source/Templates/system_stm32f4xx.c \
stm32f4xx_hal_driver/Src/stm32f4xx_hal.c \
stm32f4xx_hal_driver/Src/stm32f4xx_hal_cortex.c \
stm32f4xx_hal_driver/Src/stm32f4xx_hal_rcc.c \
stm32f4xx_hal_driver/Src/stm32f4xx_hal_gpio.c \
stm32f4xx_hal_driver/Src/stm32f4xx_hal_eth.c \
stm32f4xx_hal_driver/Src/stm32f4xx_hal_rng.c \
FreeRTOS-Kernel/croutine.c \
FreeRTOS-Kernel/event_groups.c \
FreeRTOS-Kernel/list.c \
FreeRTOS-Kernel/queue.c \
FreeRTOS-Kernel/stream_buffer.c \
FreeRTOS-Kernel/tasks.c \
FreeRTOS-Kernel/timers.c \
FreeRTOS-Kernel/portable/GCC/ARM_CM4F/port.c \
FreeRTOS-Kernel/portable/MemMang/heap_4.c

ASM_SOURCES = \
Src/startup_stm32f407xx.s

PREFIX = arm-none-eabi-
# The gcc compiler bin path can be either defined in make command via GCC_PATH variable (> make GCC_PATH=xxx)
# either it can be added to the PATH environment variable.
ifdef GCC_PATH
CC = $(GCC_PATH)/$(PREFIX)gcc
AS = $(GCC_PATH)/$(PREFIX)gcc -x assembler-with-cpp
CP = $(GCC_PATH)/$(PREFIX)objcopy
SZ = $(GCC_PATH)/$(PREFIX)size
else
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size
endif
HEX = $(CP) -O ihex
BIN = $(CP) -O binary -S

CPU = -mcpu=cortex-m4
FPU = -mfpu=fpv4-sp-d16
FLOAT-ABI = -mfloat-abi=hard
MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)

# AS defines
AS_DEFS =

# C defines
C_DEFS = \
-D USE_HAL_DRIVER \
-D STM32F407xx

# AS includes
AS_INCLUDES =

# C includes
C_INCLUDES = \
-I cmsis_device_f4/Include \
-I CMSIS_5/CMSIS/Core/Include \
-I stm32f4xx_hal_driver/Inc \
-I Inc \
-I FreeRTOS-Kernel/include \
-I FreeRTOS-Kernel/portable/GCC/ARM_CM4F \
-I lwip/src/include

ifeq ($(DEBUG), 1)
OPT = -O0 -g3 -gdwarf-5
else
OPT = -O2 -g0
endif

COMMON_FLAGS = -Wall -Wextra -fdata-sections -ffunction-sections
C_STD = -std=c99

ASFLAGS = $(MCU) $(AS_DEFS) $(AS_INCLUDES) $(OPT) $(COMMON_FLAGS)
CFLAGS = $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -Wconversion -Wsign-conversion -Wpedantic $(C_STD) $(COMMON_FLAGS)

# Generate dependency information
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"

LDSCRIPT = Src/STM32F407VGTx_FLASH.ld

# libraries
LIBS = -lc -lm -L$(LWIPBUILD_DIR) -llwipcore -lerrtrap
LDFLAGS = --specs=nano.specs $(MCU) -T$(LDSCRIPT) $(LIBS) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections

# default action: build all
all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin


#######################################
# build the application
#######################################
# list of objects
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
# list of ASM program objects
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(ASFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) FORCE
	cmake -B $(LWIPBUILD_DIR) -S ./ -DCMAKE_C_FLAGS="$(MCU) $(OPT) $(COMMON_FLAGS) $(C_STD)" -DCMAKE_C_COMPILER=$(CC)
	$(MAKE) -C $(LWIPBUILD_DIR)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(HEX) $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) $< $@

$(BUILD_DIR):
	mkdir $@


.PHOHY:
FORCE:

#######################################
# clean up
#######################################
clean:
	-rm -fR $(BUILD_DIR)


#######################################
# dependencies
#######################################
-include $(wildcard $(BUILD_DIR)/*.d)
