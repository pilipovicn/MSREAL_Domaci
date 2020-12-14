#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main(){
  FILE *fifo;
  char buffer[90];
  char input[5];
  char garbage[1];
  int num;
  int len, prev_len;
  int choice;

  while(1){
    printf("\t1: Upisi u FIFO bafer\n");
    printf("\t2: Procitaj iz FIFO bafera\n");
    printf("\t3: Izadji\n");
    printf("#:");
    scanf("%d", &choice);
    num = 0;
    input[0] = 0;
    len = 0;
    prev_len = 0;
    buffer[0] = '\0';

    switch (choice) {
    case 1: {
        printf("Upisati clanove u heksadecimalnom formatu. Izlazak sa Q.\n");
        do{
          scanf("%s", input);
          if((strncmp(input, "0x", 2) != 0) && (strncmp(input, "Q", 2) != 0)){
            printf("Neispravan format, ocekuje se heksadecimalan!\n");
          }else if(strncmp(input, "Q", 2) == 0){
            buffer[len+prev_len]=0;
          }else{
            num = (int)strtol(input, NULL, 0);
            if( num<0 || num>255){
              printf("Ocekuje se broj izmedju 0 i 255!\n");
            }else{
              prev_len = strlen(buffer);
              len = snprintf(buffer+prev_len, 82, "%#04x;", num);                   // Koristi se prev_len zbog fwrite ispod (ispravka, zbog null terminatora iznad)
            }
          }
        }while(strcmp(input, "Q") != 0);
        printf("Niz za upis: %s\n", buffer);

        fifo = fopen("/dev/fifo", "w");
        fprintf(fifo,"%s",buffer);
        //fwrite(buffer, 1, prev_len+len, fifo);
        fflush(fifo);
        fclose(fifo);

        printf("Vrednosti upisane!\n\n");

        break;
    }
    case 2: {
        printf("Koliko brojeva zelite procitati iz fifo-a?\n");
        scanf("%d", &num);
        len = sprintf(buffer, "num=%d", num);
        fifo = fopen("/dev/fifo", "w");
        fwrite(buffer, 1, len+1, fifo);
        fflush(fifo);
        fclose(fifo);
        fifo = fopen("/dev/fifo", "r");
        fgets(buffer, 5*num, fifo);
        fgets(garbage, 5*num, fifo);                                                //emulira drugi poziv cat komande koji je resen sa secondPass
        fclose(fifo);
        buffer[5*num] = 0;
        printf("%s\n", buffer);
        break;
    }
    case 3: {
        printf("Kraj.\n");
        fclose(fifo);
        return 0;
    }
    default:
        printf("Nepoznata komanda!\n");
    }
  }
  return 0;
}
