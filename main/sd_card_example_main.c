/*

		direct read file without mouning file system.

		svm.c is not used in this file.
*/

// This example uses SDMMC peripheral to communicate with SD card.

#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"


#include "svm.h"
#include "sd.h"

#define MOUNT_POINT "/sdcard"

static const char *TAG = "example";
struct svm_model* model;

static void fill_buffer(uint32_t seed, uint8_t* dst, size_t count) {
    srand(seed);
    for (size_t i = 0; i < count; ++i) {
        uint32_t val = rand();
        memcpy(dst + i * sizeof(uint32_t), &val, sizeof(val));
    }
}


void app_main(void)
{
		sdmmc_card_t *card = pvPortMalloc(sizeof(sdmmc_card_t));  //pvPortMalloc()
		char mount_point[] = MOUNT_POINT;


	  // Options for mounting the filesystem.
	  // If format_if_mount_failed is set to true, SD card will be partitioned and
	  // formatted in case when mounting fails.
	  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
	#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
	      .format_if_mount_failed = true,
	#else
	      .format_if_mount_failed = false,
	#endif // EXAMPLE_FORMAT_IF_MOUNT_FAILED
	      .max_files = 5,
	      .allocation_unit_size = 16 * 1024
	  };


	  ESP_LOGI(TAG, "Initializing SD card");

	  // Use settings defined above to initialize SD card and mount FAT filesystem.
	  // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
	  // Please check its source code and implement error recovery when developing
	  // production applications.

	  ESP_LOGI(TAG, "Using SDMMC peripheral");
	  sdmmc_host_t host = SDMMC_HOST_DEFAULT();

	  // This initializes the slot without card detect (CD) and write protect (WP) signals.
	  // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
	  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

	  // To use 1-line SD mode, change this to 1:
	  slot_config.width = 4;

	  // On chips where the GPIOs used for SD card can be configured, set them in
	  // the slot_config structure:
	#ifdef SOC_SDMMC_USE_GPIO_MATRIX
	  slot_config.clk = GPIO_NUM_14;
	  slot_config.cmd = GPIO_NUM_15;
	  slot_config.d0 = GPIO_NUM_2;
	  slot_config.d1 = GPIO_NUM_4;
	  slot_config.d2 = GPIO_NUM_12;
	  slot_config.d3 = GPIO_NUM_13;
	#endif

	  // Enable internal pullups on enabled pins. The internal pullups
	  // are insufficient however, please make sure 10k external pullups are
	  // connected on the bus. This is for debug / example purpose only.
	  slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;


//  initialize sdmmc host and card
    sdmmc_host_init();
    sdmmc_host_init_slot(SDMMC_HOST_SLOT_1, &slot_config);
    sdmmc_card_init(&host, card);


    const size_t buffer_size = 4096*4;
    const size_t block_count = buffer_size / 512;
    const size_t extra = 4;
    uint8_t* buffer = heap_caps_malloc(buffer_size + extra, MALLOC_CAP_DMA);

//    fill_buffer(seed, buffer, buffer_size / sizeof(uint32_t));
//    sdmmc_write_sectors(card, buffer, 0, block_count);
    memset(buffer, 0xcc, buffer_size + extra);
		int64_t t1 = esp_timer_get_time();

		size_t start_block = 6080;     //   2.txt begin pysical sector
// sdmmc_card_t* card, const void* src, size_t start_block, size_t block_count
    esp_err_t ret = sdmmc_read_sectors(card, buffer + 1, start_block, block_count);
		int64_t t2 = esp_timer_get_time();
    //check_buffer(seed, buffer + 1, buffer_size / sizeof(uint32_t));
		printf("error:%d\n",ret);
		printf("time:%lld us\n",t2-t1);

		for (int i=0; i<512; i++) {
			printf("%x ", buffer[i]);
		}
		printf("\n");

		for (int i=0; i<512; i++) {
			printf("%c", buffer[i]);
		}
		printf("\n");


    SD_stop(mount_point, card);
		vPortFree(card);
}
