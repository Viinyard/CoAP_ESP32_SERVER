deps_config := \
	/Users/VinYarD/esp32/esp-idf/components/app_trace/Kconfig \
	/Users/VinYarD/esp32/esp-idf/components/aws_iot/Kconfig \
	/Users/VinYarD/esp32/esp-idf/components/bt/Kconfig \
	/Users/VinYarD/esp32/esp-idf/components/esp32/Kconfig \
	/Users/VinYarD/esp32/esp-idf/components/esp_adc_cal/Kconfig \
	/Users/VinYarD/esp32/esp-idf/components/ethernet/Kconfig \
	/Users/VinYarD/esp32/esp-idf/components/fatfs/Kconfig \
	/Users/VinYarD/esp32/esp-idf/components/freertos/Kconfig \
	/Users/VinYarD/esp32/esp-idf/components/heap/Kconfig \
	/Users/VinYarD/esp32/esp-idf/components/libsodium/Kconfig \
	/Users/VinYarD/esp32/esp-idf/components/log/Kconfig \
	/Users/VinYarD/esp32/esp-idf/components/lwip/Kconfig \
	/Users/VinYarD/esp32/esp-idf/components/mbedtls/Kconfig \
	/Users/VinYarD/esp32/esp-idf/components/openssl/Kconfig \
	/Users/VinYarD/esp32/esp-idf/components/pthread/Kconfig \
	/Users/VinYarD/esp32/esp-idf/components/spi_flash/Kconfig \
	/Users/VinYarD/esp32/esp-idf/components/spiffs/Kconfig \
	/Users/VinYarD/esp32/esp-idf/components/tcpip_adapter/Kconfig \
	/Users/VinYarD/esp32/esp-idf/components/wear_levelling/Kconfig \
	/Users/VinYarD/esp32/esp-idf/components/bootloader/Kconfig.projbuild \
	/Users/VinYarD/esp32/esp-idf/components/esptool_py/Kconfig.projbuild \
	/Users/VinYarD/esp32/coap_server/main/Kconfig.projbuild \
	/Users/VinYarD/esp32/esp-idf/components/partition_table/Kconfig.projbuild \
	/Users/VinYarD/esp32/esp-idf/Kconfig

include/config/auto.conf: \
	$(deps_config)


$(deps_config): ;
