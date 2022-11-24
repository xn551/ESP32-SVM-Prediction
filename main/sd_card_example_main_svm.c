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

void app_main(void)
{
	
//******************Part 1. initalize sd card in Ai-thinker-Cam**************	
    sdmmc_card_t *card = pvPortMalloc(sizeof(sdmmc_card_t));  //pvPortMalloc()
    char mount_point[] = MOUNT_POINT;
	
//  initialize sd card and mount file system
    SD_initial(MOUNT_POINT, card);

	

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

		// edit your own test data for predction.
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


//********************Part 3 free source*********************************************
    SD_stop(mount_point, card);
    vPortFree(card);
	
}
