#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

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
	

int RR()
{
	int i,j,k,tq,time,flag;
	float sum_wait=0,sum_tat=0;
	int p[] = {1,2,3,4,5,6,7};
	int a[] = {8,10,14,9,16,21,26};
	int b[] = {10,3,7,5,4,6,2};

	
	
	FILE *f;
	
	
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
   
	
	printf("\n\nProcess\t|Turnaround time|waiting time\n\n");
	for(i=0,time=prcss[0].At; rp!=0;)
	{
		printf("%d     ",i);
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
			
			printf("%d rt %d\n",prcss[i].Pid,prcss[i].Rt);
		}
		if(prcss[i].Rt == 0 && flag == 1)
		{
		rp--; 
		//prcss[i].tempc = 0;
		prcss[i].Ct = time;
		prcss[i].TAt = time - prcss[i].At;
		prcss[i].Wt = prcss[i].TAt - prcss[i].Bt;
		printf("\np[%d]\t|\t%d\t|\t%d Ct %d\n",i+1,prcss[i].TAt,prcss[i].Wt,time);
		
		sum_wait += prcss[i].Wt;
		sum_tat += prcss[i].TAt;
		flag = 0;
		}
		if(i == n-1)
		i=0;
		
		else if(prcss[i+1].tempc<=time  && prcss[i].At<prcss[i+1].At)
		i++;
		else
		i--;
		
		
	}
	
		
	printf("Avg Tat = %f\nAvg Wt =%f\n",sum_tat/n, sum_wait/n);
	
	
	f = fopen("test.txt","w");	// writing in file 
	fprintf(f,"testing %d miliseconds\n", tq);
	fclose(f);
	
}

int main()
{
	RR();
}
