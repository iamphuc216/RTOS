#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

typedef struct {
  int frame;
  int count;
} frame_t;

int finished = 0, fault = 0;

void handler(int signal) {
  printf("Page fault: %d\n", fault);
  finished = 1;
}

int main() {
  int i,j,x,f,flag,buff,m,b;
  int string[] = {7,0,1,2,0,3,0,4,2,3,0,3,0,3,2,1,2,0,1,7,0,1,7,5};
	printf("Enter the frame size: ");
	
	scanf("%d",&f);	 
	frame_t memory[10];

	x = sizeof(string)/sizeof(string[0]);
	printf("Reference string size = %d\n",x);
 	//int y = sizeof(memory)/sizeof(memory[0]);
	printf("Frame size = %d\n",f);
	
//initializer
  for (j = 0; j < f; j++) {	
   memory[j].count = -1;
   memory[j].frame = -1;
}
m = 0;
	for (b = 0; b < x; b++)
	{
	buff = string[b];
	
	flag = 0;
		for (i = 0; i < f; i++)
		{
			if (memory[i].frame == buff)
				{
				printf("%d already exist in page frame\n", buff);
				flag = 1;
				}
		}
	
		if(flag == 0)
		{
			memory[m].frame = buff;
			printf("%d added to page frame\n", memory[m].frame);
			m = (m+1)%f;
			fault++;
		
		}	
	
		
    for (j = 0; j < f; j++) {
      printf("frame %d   %d \n",j, memory[j].frame);
	//printf("frame %d %d %d\n",j, memory[j].frame, memory[j].count);
    }
    printf("-----------------------------\n");

  
}
	printf("Ctrl+c to continue...\n");
  signal(SIGINT, handler);
  while(finished == 0);
  return 0;	//return
}
