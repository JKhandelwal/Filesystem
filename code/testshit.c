#include <stdio.h>
#include <string.h>
struct temp{
  int test;
};

struct temp initial;

void tokeniseString(const char* testy){
  // s first toke
  char test[strlen(testy)+1];
  strcpy(test,testy);
  char *token = strtok(test, "/");

  // Keep printing tokens while one of the
  // delimiters present in str[].

  int counter = 0;
  char* old;
  char* oldest;
  while (token != NULL)
  {
oldest = (char*)old;
old= token;
      printf("%s\n", token);
      token = strtok(NULL, "/");
  }

  char*  potato = "h";
  if (strtok(NULL,"/") == NULL){
    printf("%s\n","HELLO" );
  }
  printf("old token is %s %s\n",oldest,potato);
}

int testy(struct temp* p){
  struct temp o = initial;
  // p = *initial;
  printf("%d\n",o.test );
  o.test = initial.test+1;
  *p = o;
  return 0;
}


int main(void){
  initial.test = 54;
  // struct temp* p = &initial;
  // printf("%d\n",p->test );
  struct temp blob;
  testy(&blob);
  printf("%d\n",blob.test);
  printf("%d\n",initial.test );
  const char* str = "/cs/st-andrews/ac/uk";
  tokeniseString(str);
   // Return

   return 0;
}
