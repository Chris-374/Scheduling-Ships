# ============================================================
# Scheduling Ships - Makefile auxiliar
# Proyecto CE4303 Principios de Sistemas Operativos
#
# Este Makefile NO reemplaza ESP-IDF.
# Solo simplifica los comandos frecuentes de build, flash,
# monitor, GUI y limpieza.
# ============================================================

# Ajustar si cambia la instalación de ESP-IDF
IDF_EXPORT ?= /home/mauricio/.espressif/v6.0.1/esp-idf/export.sh

# Puerto serial del ESP32-C6
PORT ?= /dev/ttyUSB0

# Baudrate del monitor y GUI
BAUD ?= 115200

# Rutas del proyecto
PROJECT_ROOT := $(CURDIR)
ESP_PROJECT := $(PROJECT_ROOT)/hardware/canal_esp_test
GUI_DIR := $(PROJECT_ROOT)/gui

# Python del entorno ESP-IDF, útil porque normalmente ya tiene pyserial
IDF_PYTHON ?= /home/mauricio/.espressif/python_env/idf6.0_py3.12_env/bin/python

.PHONY: help env build fullclean flash monitor flash-monitor gui config clean

help:
	@echo "Scheduling Ships - comandos disponibles"
	@echo ""
	@echo "  make build              Compila el firmware ESP32"
	@echo "  make fullclean          Limpieza completa ESP-IDF"
	@echo "  make flash              Flashea el ESP32"
	@echo "  make monitor            Abre monitor serial"
	@echo "  make flash-monitor      Flashea y abre monitor"
	@echo "  make gui                Abre la interfaz grafica"
	@echo "  make config             Abre config.txt con nano"
	@echo "  make clean              Limpieza normal ESP-IDF"
	@echo ""
	@echo "Variables opcionales:"
	@echo "  PORT=/dev/ttyUSB0"
	@echo "  BAUD=115200"
	@echo ""
	@echo "Ejemplos:"
	@echo "  make build"
	@echo "  make flash-monitor PORT=/dev/ttyUSB0"
	@echo "  make gui PORT=/dev/ttyUSB0"

env:
	@echo "Para activar ESP-IDF manualmente:"
	@echo "source $(IDF_EXPORT)"

build:
	bash -lc 'source "$(IDF_EXPORT)" && cd "$(ESP_PROJECT)" && idf.py build'

fullclean:
	bash -lc 'source "$(IDF_EXPORT)" && cd "$(ESP_PROJECT)" && idf.py fullclean'

clean:
	bash -lc 'source "$(IDF_EXPORT)" && cd "$(ESP_PROJECT)" && idf.py clean'

flash:
	bash -lc 'source "$(IDF_EXPORT)" && cd "$(ESP_PROJECT)" && idf.py -p "$(PORT)" flash'

monitor:
	bash -lc 'source "$(IDF_EXPORT)" && cd "$(ESP_PROJECT)" && idf.py -p "$(PORT)" monitor'

flash-monitor:
	bash -lc 'source "$(IDF_EXPORT)" && cd "$(ESP_PROJECT)" && idf.py -p "$(PORT)" flash monitor'

gui:
	cd "$(GUI_DIR)" && "$(IDF_PYTHON)" gui.py --port "$(PORT)" --baud "$(BAUD)"

config:
	nano "$(PROJECT_ROOT)/config.txt"
