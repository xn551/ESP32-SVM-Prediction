#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include <math.h>

#include "svm.h"

static const char *TAG = "example";
static char *line = NULL;
static int max_line_len;

static const char *svm_type_table[] =
{
	"c_svc","nu_svc","one_class","epsilon_svr","nu_svr",NULL
};

static const char *kernel_type_table[]=
{
	"linear","polynomial","rbf","sigmoid","precomputed",NULL
};


static char* readline(FILE *input)
{
	int len;

	if(fgets(line,max_line_len,input) == NULL)
		return NULL;

	while(strrchr(line,'\n') == NULL)
	{
		max_line_len *= 2;
		line = (char *) realloc(line,max_line_len);
		len = (int) strlen(line);
		if(fgets(line+len,max_line_len-len,input) == NULL)
			break;
	}
	return line;
}



struct svm_model *svm_load_model(const char *model_file_name)
{
	int64_t t_fopen_before = esp_timer_get_time();

	FILE *fp = fopen(model_file_name,"rb");

	int64_t t_fopen_after = esp_timer_get_time();

	if (fp == NULL) {
			ESP_LOGE(TAG, "Failed to open file for writing");
			return NULL;
	}

	printf("open file in SD card. using time %lld us.\n", t_fopen_after - t_fopen_before );


	struct svm_model *model= pvPortMalloc(sizeof(struct svm_model));
	struct svm_parameter *param = &(model->param);

	model->rho = NULL;
	model->probA = NULL;
	model->probB = NULL;
	model->label = NULL;
	model->nSV = NULL;

	char cmd[81];
	while(1)
	{
		fscanf(fp,"%80s",cmd);
		printf("1 scan\n");

		if(strcmp(cmd,"svm_type")==0)
		{
				fscanf(fp,"%80s",cmd);
				int i;
				for(i=0;svm_type_table[i];i++)
				{
						if(strcmp(svm_type_table[i],cmd)==0)
						{
							param->svm_type=i;
							printf("svm_type:%d\n", param->svm_type );
							printf("svm_type:%d\n", (model->param).svm_type );
							break;
						}
				}
				if(svm_type_table[i] == NULL)
				{
					printf("unknown svm type.\n");
					vPortFree(model->rho);
					vPortFree(model->label);
					vPortFree(model->nSV);
					vPortFree(model);
					return NULL;
				}
		}

		else if(strcmp(cmd,"kernel_type")==0)
		{
			fscanf(fp,"%80s",cmd);
			int i;
			for(i=0;kernel_type_table[i];i++)
			{
				if(strcmp(kernel_type_table[i],cmd)==0)
				{
					param->kernel_type=i;
					break;
				}
			}
			if(kernel_type_table[i] == NULL)
			{
				fprintf(stderr,"unknown kernel function.\n");
				vPortFree(model->rho);
				vPortFree(model->label);
				vPortFree(model->nSV);
				vPortFree(model);
				return NULL;
			}
		}
		else if(strcmp(cmd,"degree")==0)
			fscanf(fp,"%d",&param->degree);
		else if(strcmp(cmd,"gamma")==0)
			fscanf(fp,"%lf",&param->gamma);
		else if(strcmp(cmd,"coef0")==0)
			fscanf(fp,"%lf",&param->coef0);
		else if(strcmp(cmd,"nr_class")==0)
			fscanf(fp,"%d",&model->nr_class);
		else if(strcmp(cmd,"total_sv")==0)
			fscanf(fp,"%d",&model->l);
		else if(strcmp(cmd,"rho")==0)
		{
			int n = model->nr_class * (model->nr_class-1)/2;
			model->rho = pvPortMalloc(sizeof(double)*n);
			for(int i=0;i<n;i++)
				fscanf(fp,"%lf",&model->rho[i]);
		}
		else if(strcmp(cmd,"label")==0)
		{
			int n = model->nr_class;
			model->label = pvPortMalloc(sizeof(int)*n);
			for(int i=0;i<n;i++)
				fscanf(fp,"%d",&model->label[i]);
		}
		else if(strcmp(cmd,"probA")==0)
		{
			int n = model->nr_class * (model->nr_class-1)/2;
			model->probA = pvPortMalloc(sizeof(double)*n);
			for(int i=0;i<n;i++)
				fscanf(fp,"%lf",&model->probA[i]);
		}
		else if(strcmp(cmd,"probB")==0)
		{
			int n = model->nr_class * (model->nr_class-1)/2;
			model->probB = pvPortMalloc(sizeof(double)*n);
			for(int i=0;i<n;i++)
				fscanf(fp,"%lf",&model->probB[i]);
		}
		else if(strcmp(cmd,"nr_sv")==0)
		{
			int n = model->nr_class;
			model->nSV = pvPortMalloc(sizeof(int)*n);
			for(int i=0;i<n;i++)
				fscanf(fp,"%d",&model->nSV[i]);
		}
		else if(strcmp(cmd,"SV")==0)
		{
			while(1)
			{
				int c = getc(fp);
				if(c==EOF || c=='\n') break;
			}
			break;
		}
		else
		{
			fprintf(stderr,"unknown text in model file: [%s]\n",cmd);
			vPortFree(model->rho);
			vPortFree(model->label);
			vPortFree(model->nSV);
			vPortFree(model);
			return NULL;
		}

	}

//----------------------read data:support vector--------------------------

// read sv_coef and SV

	int elements = 0;
	long pos = ftell(fp);

	max_line_len = 1024;
	line = pvPortMalloc(sizeof(char)*max_line_len);
	char *p,*endptr,*idx,*val;

	while(readline(fp)!=NULL)
	{
		p = strtok(line,":");
		while(1)
		{
			p = strtok(NULL,":");
			if(p == NULL)
				break;
			++elements;
		}
	}
	elements += model->l;

	fseek(fp,pos,SEEK_SET);

	int m = model->nr_class - 1;
	int l = model->l;
	model->sv_coef = pvPortMalloc(sizeof(double *)*m);
	int i;
	for(i=0;i<m;i++)
		model->sv_coef[i] = pvPortMalloc(sizeof(double)*l);
	model->SV = pvPortMalloc(sizeof(struct svm_node*)*l);
	struct svm_node *x_space = NULL;
	if(l>0) x_space = pvPortMalloc(sizeof(struct svm_node)*elements);

	int j=0;
	for(i=0;i<l;i++)
	{
		readline(fp);
		model->SV[i] = &x_space[j];

		p = strtok(line, " \t");
		model->sv_coef[0][i] = strtod(p,&endptr);
		for(int k=1;k<m;k++)
		{
			p = strtok(NULL, " \t");
			model->sv_coef[k][i] = strtod(p,&endptr);
		}

		while(1)
		{
			idx = strtok(NULL, ":");
			val = strtok(NULL, " \t");

			if(val == NULL)
				break;
			x_space[j].index = (int) strtol(idx,&endptr,10);
			x_space[j].value = strtod(val,&endptr);

			++j;
		}
		x_space[j++].index = -1;
	}
	vPortFree(line);

	if (ferror(fp) != 0 || fclose(fp) != 0)
		return NULL;

	model->free_sv = 1;	// XXX
	return model;
}


void svm_free_model_content(struct svm_model* model_ptr)
{
	if(model_ptr->free_sv && model_ptr->l > 0 && model_ptr->SV != NULL)
		vPortFree((void *)(model_ptr->SV[0]));
	if(model_ptr->sv_coef)
	{
		for(int i=0;i<model_ptr->nr_class-1;i++)
			vPortFree(model_ptr->sv_coef[i]);
	}

	vPortFree(model_ptr->SV);
	model_ptr->SV = NULL;

	vPortFree(model_ptr->sv_coef);
	model_ptr->sv_coef = NULL;

	vPortFree(model_ptr->rho);
	model_ptr->rho = NULL;

	vPortFree(model_ptr->label);
	model_ptr->label= NULL;

	vPortFree(model_ptr->probA);
	model_ptr->probA = NULL;

	vPortFree(model_ptr->probB);
	model_ptr->probB= NULL;

	vPortFree(model_ptr->nSV);
	model_ptr->nSV = NULL;
}

void svm_free_and_destroy_model(struct svm_model** model_ptr_ptr)
{
	if(model_ptr_ptr != NULL && *model_ptr_ptr != NULL)
	{
		svm_free_model_content(*model_ptr_ptr);
		vPortFree(*model_ptr_ptr);
		*model_ptr_ptr = NULL;
	}
}

void svm_destroy_param(struct svm_parameter* param)
{
	vPortFree(param->weight_label);
	vPortFree(param->weight);
}

double dot(const struct svm_node *px, const struct svm_node *py)
{
	double sum = 0;
	while(px->index != -1 && py->index != -1)
	{
		//printf("dot index: x:%d, y:%d\n",px->index,py->index);
		//printf("dot result: x:%f, y:%f, sum:%f\n\n",px->value,py->value,sum);

		if(px->index == py->index)
		{
			printf("dot index: x:%d, y:%d\n",px->index,py->index);
			printf("dot result: x:%f, y:%f, sum:%f\n",px->value,py->value,sum);
			sum += px->value * py->value;
			//px += sizeof(struct svm_node);
			//py += sizeof(struct svm_node);
			++px;
			++py;

		}
		else
		{
			if(px->index > py->index)
				//py += sizeof(struct svm_node);
				++py;
			else
				//px += sizeof(struct svm_node);
				++px;
		}
	}
	//printf("dot finished\n");
	printf("dot sum:%f\n",sum);
	return sum;
}

static inline double powi(double base, int times)
{
	double tmp = base, ret = 1.0;

	for(int t=times; t>0; t/=2)
	{
		if(t%2==1) ret*=tmp;
		tmp = tmp * tmp;
	}
	return ret;
}

double k_function(const struct svm_node *x, const struct svm_node *y,
			  const struct svm_parameter *param)
{
	switch(param->kernel_type)
	{
		case LINEAR:
			return dot(x,y);
		case POLY:
			return powi(param->gamma*dot(x,y)+param->coef0,param->degree);
		case RBF:
		{
			double sum = 0;
			while(x->index != -1 && y->index !=-1)
			{
				//---------------test------------------
				//printf("1,x,y:%f,%f\n",x->value,y->value);

				if(x->index == y->index)
				{

					printf("rbf index: x:%d, y:%d\n",x->index,y->index);
					printf("rbf result: x:%f, y:%f, sum:%f\n",x->value,y->value,sum);
					double d = x->value - y->value;
					sum += d*d;
					//x += sizeof(struct svm_node);
					//y += sizeof(struct svm_node);
					++x;
					++y;
				}
				else
				{
					if(x->index > y->index)
					{
						sum += y->value * y->value;
						//y += sizeof(struct svm_node);
						++y;
					}
					else
					{
						sum += x->value * x->value;
						//x += sizeof(struct svm_node);
						++x;
					}
				}

			}

			while(x->index != -1)
			{
				sum += x->value * x->value;
				//x += sizeof(struct svm_node);
				++x;
			}

			while(y->index != -1)
			{
				sum += y->value * y->value;
				//y += sizeof(struct svm_node);
				++y;
			}
			printf("dot sum:%f\n",sum);
			return exp(-param->gamma*sum);
		}
		case SIGMOID:
			return tanh(param->gamma*dot(x,y)+param->coef0);
		case PRECOMPUTED:  //x: test (validation), y: SV
			return x[(int)(y->value)].value;
		default:
			return 0;  // Unreachable
	}
}


double svm_predict_values(const struct svm_model *model, const struct svm_node *x, double* dec_values)
{
	int i;
	if(model->param.svm_type == ONE_CLASS ||
	   model->param.svm_type == EPSILON_SVR ||
	   model->param.svm_type == NU_SVR)
	{
		double *sv_coef = model->sv_coef[0];
		double sum = 0;
	//	#pragma omp parallel for private(i) reduction(+:sum)
		for(i=0;i<model->l;i++) {
			sum += sv_coef[i] * k_function(x,model->SV[i],&(model->param) );
			printf("one sv finished!\n");
		}
		sum -= model->rho[0];
		*dec_values = sum;

		if(model->param.svm_type == ONE_CLASS)
			return (sum>0)?1:-1;
		else
			return sum;
	}
	else
	{
		int nr_class = model->nr_class;
		int l = model->l;

		double *kvalue = pvPortMalloc(sizeof(double)*l);;
		//#pragma omp parallel for private(i)
		for(i=0;i<l;i++)
			kvalue[i] = k_function(x,model->SV[i],&(model->param) );

		int *start = pvPortMalloc(sizeof(int)*nr_class);
		start[0] = 0;
		for(i=1;i<nr_class;i++)
			start[i] = start[i-1]+model->nSV[i-1];

		int *vote = pvPortMalloc(sizeof(int)*nr_class);
		for(i=0;i<nr_class;i++)
			vote[i] = 0;

		int p=0;
		for(i=0;i<nr_class;i++)
			for(int j=i+1;j<nr_class;j++)
			{
				double sum = 0;
				int si = start[i];
				int sj = start[j];
				int ci = model->nSV[i];
				int cj = model->nSV[j];

				int k;
				double *coef1 = model->sv_coef[j-1];
				double *coef2 = model->sv_coef[i];
				for(k=0;k<ci;k++) {
					sum += coef1[si+k] * kvalue[si+k];
					printf("one sv finished! sum:%f, ci index:%d\n", sum,k);
				}
				for(k=0;k<cj;k++) {
					sum += coef2[sj+k] * kvalue[sj+k];
					printf("one sv finished! sum:%f, cj index:%d\n", sum,k);
				}
				sum -= model->rho[p];
				dec_values[p] = sum;

				if(dec_values[p] > 0)
					++vote[i];
				else
					++vote[j];
				p++;
			}

		int vote_max_idx = 0;
		for(i=1;i<nr_class;i++)
			if(vote[i] > vote[vote_max_idx])
				vote_max_idx = i;

		free(kvalue);
		free(start);
		free(vote);
		return model->label[vote_max_idx];
	}
}

double svm_predict(const struct svm_model *model, const struct svm_node *x)
{
	int nr_class = model->nr_class;
	double *dec_values;
	if(model->param.svm_type == ONE_CLASS ||
	   model->param.svm_type == EPSILON_SVR ||
	   model->param.svm_type == NU_SVR)
		dec_values = pvPortMalloc(sizeof(double)*1);
	else
		dec_values = pvPortMalloc(sizeof(double)*nr_class*(nr_class-1)/2);
	double pred_result = svm_predict_values(model, x, dec_values);
	free(dec_values);
	return pred_result;
}
