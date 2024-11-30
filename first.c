#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<stdarg.h>
#include<time.h>

#include<unistd.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<dirent.h>
#include<fcntl.h>
#include<ftw.h>
#include<signal.h>
#include<sys/mman.h>
#include<sys/ipc.h>
#include<sys/msg.h>
#include<sys/shm.h>
#include<sys/sem.h>
#include<errno.h>

int queue_size, window_size;
int tgt_read_count, tgt_write_count;
int cur_read_count, cur_write_count;
int data_pipe[2], meta_pipe[2];

void setMetaData(){
	int meta[3];
	read(meta_pipe[0], meta, sizeof(int)*3);
	meta[0] = cur_read_count, meta[1] = cur_write_count, meta[2] = window_size;
	write(meta_pipe[1], &meta, sizeof(int)*3);
}

void r_scanf(const char *format, void *p){
	int sz = 0;
	if(strcmp(format, "%d")==0) sz = sizeof(int); else
	if(strcmp(format, "%ld")==0) sz = sizeof(long); else
	if(strcmp(format, "%f")==0) sz = sizeof(float); else
	if(strcmp(format, "%lf")==0) sz = sizeof(double); else
	if(strcmp(format, "%c")==0) sz = sizeof(char); else
	{printf("wrong format handled!\n"); exit(1);}

	if(tgt_read_count<=cur_read_count){ //normal mode
		scanf(format, p);
		tgt_write_count = -1;
	}
	else{ //recovery mode
		read(data_pipe[0], p, sz); queue_size-=sz;
	}
	write(data_pipe[1], p, sz); window_size+=sz;
	cur_read_count++;
	/*shrink to fit*/
	if(cur_read_count==tgt_read_count||cur_read_count==1&&tgt_read_count==0){
		void *dummy = malloc(queue_size);
		read(data_pipe[0], dummy, queue_size);
		free(dummy);
	}
	setMetaData();
}

void r_printf(const char *format, ...){
	if(tgt_write_count<=cur_write_count){
		va_list args;
		va_start(args, format);
		vprintf(format, args);
		va_end(args);
	}
	cur_write_count++;
	setMetaData();
}

void superInit(){
	/*pipe for logging*/
	pipe(data_pipe); pipe(meta_pipe);
	/*ignore SIGINT*/
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);
}
void childInit(){
	/*init meta_data*/
	int meta[3];
	cur_read_count = cur_write_count = 0;
	meta[0] = cur_read_count, meta[1] = cur_write_count, meta[2] = window_size;
	write(meta_pipe[1], &meta, sizeof(int)*3);
	/*default SIGINT*/
	struct sigaction sa;
	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);
}

void yourCode();
int main(){
	superInit();
	int pid, status;
	while(1){
		pid = fork();
		if(pid==0){
			childInit();
			yourCode();
			exit(0);
		}
		waitpid(pid, &status, 0);
		if(!WIFSIGNALED(status))break;
		/*recovery process*/
		int meta[3]; read(meta_pipe[0], &meta, sizeof(int)*3);
		cur_read_count = meta[0], tgt_write_count = meta[1], queue_size = meta[2];
		printf("입력번호는 %d; 원하는 입력번호는?\n", cur_read_count);
		scanf("%d", &tgt_read_count);getchar();
		/*boundary handling*/
		if(tgt_read_count<0)tgt_read_count = 0;
		if(cur_read_count<tgt_read_count)tgt_read_count = cur_read_count;
		printf(" ... recovery completed ...\n");
	}
	return 0;
}

void sum10int(){
	int i, n, sum = 0;
	for(i=0;i<10;i++){
		r_scanf("%d", &n);
		sum+=n;
		r_printf("sum=%d\n", sum);
	}
}

void sum10real(){
	int i;
	double sum = 0, n;
	for(i=0;i<10;i++){
		r_scanf("%lf", &n);
		sum+=n;
		r_printf("sum=%lf\n", sum);
	}
}

void sum10char(){
	int i;
	char sum[1<<5] = {0, }, n, d;
	for(i=0;i<10;i++){
		r_scanf("%c", &n); r_scanf("%c", &d); //do not use getchar() to handle '\n'
		sum[i] = n;
		r_printf("sum=%s\n", sum);
	}
}

void yourCode(){
	sum10int();
	/*sum10real();*/
	/*sum10char();*/
}
