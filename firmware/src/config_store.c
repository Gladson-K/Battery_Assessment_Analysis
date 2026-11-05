#include "config_store.h"

#include "app_config.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_flash_ex.h"

#include <string.h>

#define CONFIG_MAGIC 0x42415431u
#define CONFIG_VERSION 1u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t profile;
    ba_config_t config;
    uint32_t checksum;
} config_record_t;

static uint32_t checksum32(const uint8_t *data, size_t length)
{
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < length; ++i) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}

static uint32_t record_checksum(const config_record_t *record)
{
    return checksum32((const uint8_t *)record,
                      sizeof(config_record_t) - sizeof(uint32_t));
}

static bool flash_erase(void)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t sector_error = 0;
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    erase.Sector = FLASH_SECTOR_7;
    erase.NbSectors = 1;
    HAL_FLASH_Unlock();
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase, &sector_error);
    HAL_FLASH_Lock();
    return status == HAL_OK && sector_error == 0xFFFFFFFFu;
}

bool config_store_load(ba_config_t *config, bat_profile_t *profile)
{
    const config_record_t *record = (const config_record_t *)APP_CONFIG_FLASH_ADDRESS;
    if (record->magic != CONFIG_MAGIC || record->version != CONFIG_VERSION) {
        return false;
    }
    if (record->checksum != record_checksum(record)) {
        return false;
    }
    if (config != 0) {
        *config = record->config;
    }
    if (profile != 0) {
        *profile = (bat_profile_t)record->profile;
    }
    return true;
}

bool config_store_save(const ba_config_t *config, bat_profile_t profile)
{
    if (config == 0) {
        return false;
    }
    if (!flash_erase()) {
        return false;
    }
    config_record_t record;
    memset(&record, 0xFF, sizeof(record));
    record.magic = CONFIG_MAGIC;
    record.version = CONFIG_VERSION;
    record.profile = (uint32_t)profile;
    record.config = *config;
    record.checksum = record_checksum(&record);

    const uint32_t *words = (const uint32_t *)&record;
    size_t word_count = (sizeof(record) + 3u) / 4u;
    HAL_FLASH_Unlock();
    HAL_StatusTypeDef status = HAL_OK;
    for (size_t i = 0; i < word_count; ++i) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                                   APP_CONFIG_FLASH_ADDRESS + (i * 4u), words[i]);
        if (status != HAL_OK) {
            break;
        }
    }
    HAL_FLASH_Lock();
    return status == HAL_OK;
}

bool config_store_erase(void)
{
    return flash_erase();
}
