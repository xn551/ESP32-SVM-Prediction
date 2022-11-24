/* SD card and FAT filesystem example.
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
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

/*
static void fill_buffer(uint32_t seed, uint8_t* dst, size_t count) {
    srand(seed);
    for (size_t i = 0; i < count; ++i) {
        uint32_t val = rand();
        memcpy(dst + i * sizeof(uint32_t), &val, sizeof(val));
    }
}
*/

void app_main(void)
{
		sdmmc_card_t *card = pvPortMalloc(sizeof(sdmmc_card_t));  //pvPortMalloc()
		char mount_point[] = MOUNT_POINT;
	//	esp_err_t ret;

//  initialize sd card and mount file system
    SD_initial(MOUNT_POINT, card);




//***************read 1.txt and measure the sd read time************************

    const char *file_hello = MOUNT_POINT"/1.TXT";

		int64_t t_fopen_bef = esp_timer_get_time();  // time before open file.

    FILE *f = fopen(file_hello, "r");

		int64_t t_fopen_aft = esp_timer_get_time(); // time after open file.

    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

		int64_t t_fopen_check = esp_timer_get_time();

		printf("open file in SD card. using time %lld us.\n", t_fopen_aft - t_fopen_bef );
		printf("open file in SD card. check time %lld us.\n", t_fopen_check - t_fopen_aft );


    ESP_LOGE(TAG, "Open 1.txt");

    char line[200];

		int64_t t_read_bef = esp_timer_get_time();   // time bofore

    while ( fgets(line, sizeof(line), f) !=NULL ) {
      // Strip newline
      char *pos = strchr(line, '\n');
      if (pos) {
        *pos = '\0';
         //ESP_LOGI(TAG, "Read from file: '%s'", line);
      }
    }

		int64_t t_read_aft = esp_timer_get_time();  // time after

    fclose(f);

		int64_t t_fclose_aft = esp_timer_get_time();

		printf("read file in SD card. using time %lld us.\n", t_read_aft - t_read_bef );
		printf("close file in SD card. check time %lld us.\n", t_fclose_aft - t_read_aft );

//********************************Part 1 end*******************************/




//**********************Part 2. read 2.text and do svm predict**************
    const char *model_file_name = MOUNT_POINT"/2.TXT";
		model=svm_load_model(model_file_name);

		if (model == NULL) {
        ESP_LOGE(TAG, "Failed to open svm model!\n");
        return;
    }

		printf("number of SVs:%d\n", model->l);

		int node_size = sizeof(struct svm_node);
		struct svm_node* x= pvPortMalloc(node_size*4);

		int index[] = {1,2,3,4};
		double value[] = {1,3,4,5};

		for (int i=0;i<4;i++) {
			(	( struct svm_node*) ( x + i ) ) ->index = index[i];
			(	( struct svm_node*) ( x + i ) ) ->value = value[i];
		}

		printf("kernel:%d\n", (model->param).kernel_type);
		(model->param).kernel_type = 0;

		printf("kernel:%d\n", (model->param).kernel_type);

		double z = k_function(x, x,	&(model->param) );

		printf("Z:%f\n",z);

		double p = svm_predict(model, x);
		printf("F:%f\n",p);
//***********************Part 2 end*******************************************/


    SD_stop(mount_point, card);
		vPortFree(card);
}
