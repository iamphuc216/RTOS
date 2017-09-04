
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#define FILENAME_SIZE 85
#define FIFO_NAME "mydemofifo"
int addsum;
char outputfile[FILENAME_SIZE];
pthread_t Thread1, Thread2;
FILE *f;
	
void *RoundRobin(void *);
void *writeintxt(void *);
void writeinfifo(const char* const );
void create(const int sum);
void put (const int sum);

typedef struct
{
	int Pid;	//Process ID
	int At;		//Arrive time
	int Bt;		//Burst time
	int Rt;		//Remaining time
	int Ct;		//Complition time
	int TAt;	//Turn-Around time
	int Wt;		//Wait time
	int tempc;
} process;


void create (const int sum)
{
	addsum = sum; 		//copy user define sum
	if(pthread_create(&Thread1, NULL, (void *)RoundRobin,(void *)Thread1) !=0)
	{
		perror("pthread_create");
		exit(1);
	}
	
	if (mkfifo(FIFO_NAME, S_IRUSR | S_IWUSR) < 0)	//FIFO creation
	{
		perror("mkfifo");
		exit(1);
	}
	printf("FIFO was created\n");
	
	pthread_join(Thread1,NULL);	//wait for thread to end
}


void put (const int sum)
{
	int fd,d;
	char FIFOdata[FILENAME_SIZE];
	if(pthread_create(&Thread2,NULL,(void *)writeintxt ,(void *)Thread2) !=0)
	{
		perror("pthread_create 2");
		exit(1);
	}
	
	
	
	//
	if((fd = open(FIFO_NAME, O_RDONLY | O_NONBLOCK))<0)	
	{
		perror("open");
		exit(1);
	}

	d = read(fd,FIFOdata,FILENAME_SIZE);
	
	if(d==0)
		printf("FIFO is empty");
	else
	{
		FIFOdata[d] = '\0';
		//printf("messege: %s\n", FIFOdata);
	}
	
	f = fopen("output.txt","w");	// writing in file 
	fprintf(f,"%s\n", FIFOdata);
	fclose(f);
	
	
}
void writeinfifo(const char* const result)
{
	int fd;
	
	if((fd = open(FIFO_NAME, O_RDONLY | O_NONBLOCK))<0)	
	{
		perror("open");
		exit(1);
	}
	if((fd = open(FIFO_NAME, O_WRONLY | O_NONBLOCK))<0)
	{
		perror("open");
		exit(1);
	}
	
	write(fd, result, strlen(result));	//write into FIFO
	printf("Thread 1 result copied to FIFO");
	
	close(fd);
	
	//remove(FIFO_NAME);
}
void *writeintxt(void* threadID)
{
	int fd;
	printf("successfully reached to writeintxt");
	put(FIFO_NAME);
	
	close(fd);
	if(unlink(FIFO_NAME) < 0)
    {
	perror("unlink");
	exit(0);
    }
	return NULL;
}

void *RoundRobin(void* threadID)
{
 int i,j,tq,time,flag;
	float sum_wait=0,sum_tat=0;
	int p[] = {1,2,3,4,5,6,7};
	int a[] = {8,10,14,9,16,21,26};
	int b[] = {10,3,7,5,4,6,2};
	char data[FILENAME_SIZE];
	
	

	
	
	int n = sizeof(p)/sizeof(p[0]);		//number of processes
	printf("number of processes %d\n", n);
	int rp = n;
	printf("Time Quantum = ");
	scanf("%d",&tq);
	process prcss[n],tst;
	
	printf("\nPid\tAt\tBt\n\n");
	for(i=0; i<n ; i++)		//assigning values to structure 
	{
		prcss[i].Pid	=	p[i];
		prcss[i].At		=	a[i];
		prcss[i].Bt		=	b[i];
		prcss[i].Rt		=	b[i];
		prcss[i].Wt		=	0;
		prcss[i].TAt	=	0;
		prcss[i].Ct		=	0;
		prcss[i].tempc	=	a[i];
		
		printf("%d\t%d\t%d\n",prcss[i].Pid,prcss[i].At,prcss[i].Bt);
	}
	
	//RR Logic
	
	//sorting in ascending arrival time
	  for(i=0; i<n; i++)
    {
        for(j=i+1; j<n; j++)
        {
           
            if(prcss[j].At < prcss[i].At)
            {
                
                tst = prcss[i];
                prcss[i] = prcss[j];
                prcss[j] = tst;
            }
        }
    }
   
	
	printf("\n\nProcess\t|Turnaround time|waiting time|Complition time\n\n");
	for(i=0,time=prcss[0].At; rp!=0;)
	{

		if(prcss[i].Rt<=tq && prcss[i].Rt > 0)	
		{
			time += prcss[i].Rt;
			prcss[i].Rt = 0;
			flag = 1;
		}
		else if(prcss[i].Rt > 0)
		{
			prcss[i].Rt -= tq;
			time += tq;
			prcss[i].tempc = time;
			

		}
		if(prcss[i].Rt == 0 && flag == 1)
		{
		rp--; 
		
		prcss[i].Ct = time;
		prcss[i].TAt = time - prcss[i].At;
		prcss[i].Wt = prcss[i].TAt - prcss[i].Bt;
		printf("\nP[%d]\t|\t%d\t|\t%d\t%d\n",i+1,prcss[i].TAt,prcss[i].Wt,time);
		
		sum_wait += prcss[i].Wt;
		sum_tat += prcss[i].TAt;
		flag = 0;
		}
		if(i == n-1)
		i=0;
		
		else if(prcss[i+1].tempc<=time  && prcss[i].At<prcss[i+1].At)
		i++;
		else
		i++;
		
		
	}
	
		
	sprintf(data,"Avg Tat = %f\nAvg Wt =%f\n",sum_tat/n, sum_wait/n);
	

	

    
    
    writeinfifo(data);

    //return NULL;
}



int main(void)
{
	remove(FIFO_NAME);
 	 create(2);
 	 put(2);
  
}

/* The function for the threads to share. */

