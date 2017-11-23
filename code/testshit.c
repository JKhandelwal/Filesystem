#include <stdio.h>
#include <string.h>
#include <libgen.h>

struct temp{
  int test;
  char t[256];
};

struct temp initial;

void tokeniseString(const char* testy, char *string){
  // s first
  printf("hello\n" );
  char test[strlen(testy)+1];
  strcpy(test,testy);
  printf("copies\n" );
  char *token =  NULL;
  token = strtok(test,"/");
  if (token == NULL){
    printf("It picks up that it is null\n" );
  }

  // if (token = strtok(test, "/") != NULL){
  //   printf("%s\n",token);
  // }
  // printf("%s\n",token);
  // Keep printing tokens while one of the
  // delimiters present in str[].

  int counter = 0;
  char* old = NULL;
  char* oldest = NULL;
  while (token != NULL)
  {
        oldest =  old;
        old =token;
      printf("token is %s\n", token);
      token = strtok(NULL, "/");
}
  printf("before copying\n" );
  // strcpy(string,old);
  printf("after copying\n" );
  printf("old token is %s\n",oldest);
}

int testy(struct temp* p){
  struct temp o = initial;
  // *t = malloc(10);
  // *t = "helloworld\0";
  // p = *initial;
  printf("%d\n",o.test );
  o.test = initial.test+1;

  *p = o;
  return 0;
}


int main(void){
  initial.test = 54;
  strcpy(initial.t,"hello world");
  // char* tesy;
  // struct temp* p = &initial;
  // printf("%d\n",p->test );
  struct temp blob;

  // testy(&blob);
  // printf("%s\n",testy );
  // printf("%d\n",blob.test);
  // printf("%d\n",initial.test );
  const char* ccpy = "/hello/hello1/hello3/hello2";
  char str  [strlen(ccpy) + 1];
  strcpy(str,ccpy);
  char* dir = dirname(str);
  printf("THE DIR IS %s\n",dir );
  char* twoDir = dirname(dir);
  printf("2 dir is %s\n",twoDir);
  char* threeDir = dirname(twoDir);
  printf("3 dir is %s\n",threeDir );
  char* basenamer = basename(threeDir);
  printf("basename is %s\n",basenamer );


  // strcpy(str,ccpy);
  // char* path = basename(str);
  // printf("%s %s\n",dir,path );
  // char* t = malloc(sizeof(char) * 256);
  // tokeniseString(ccpy,t);
  // printf("It fucking works %s\n",t );
  // char temp[256];
  // strcpy(blob.t,t);
  //
  // free(t);
  // // printf("%s\n",blob.t );
  // // free(tesy);
  //  // Return

   return 0;
}
