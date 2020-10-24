#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/select.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>

#define MAX_LEN 256
#define SOCK_NAME "chat_server"
#define EXIT_MESSAGE "exit"

void tstp_handler(int signo);
void quit_handler(int signo);
void int_handler(int signo);
void move(int room_num, int option);
void create();
void room_list_print(int room_num);
void get_room_name(int sockfd, char buf[], size_t len, int flags);
void ifexit(void);
void lsls(char* dirname);
void cpcp(char *dirname, char *argv1, char *argv2);
void make_dirname(char* testfile);
int connect_cli(char * sock_name);

int sd;
char name[50];   // 사용자의 ID 
char serv_dirname[50];
char dirname[255];

int main() {
   struct sockaddr_un ser;
   struct dirent *dent;
   char buf[MAX_LEN];
   char * send_str;
   char testfile[50];
   int  len, pid, nlen;
   int room_num;   // 서버에 있는 방의 개수 저장하는 변수.
   fd_set read_fds;
   DIR *dp;

   atexit(ifexit);    // atexit 함수 등록
   signal(SIGTSTP, tstp_handler);  // 종료 안내메시지 출력. (ctrl + z로 종료할 수 없음)
   signal(SIGQUIT, quit_handler);   
   	     // ctrl + ￦를 입력하면 서버에게 EXIT_MESSAGE를 전달하여 
               // 연결되어 있는 클라이언트의 개수를 하나 줄이도록 하고, 
               // exit될 때 호출되는 함수를 실행하여 클라이언트가 종료될 때 처리해야할 작업을 수행한다.
   	
   printf("닉네임을 입력해주세요! : ");
   scanf("%s", name); getchar();   // 사용자 아이디 입력받음.
   sd = connect_cli(SOCK_NAME);   // "chat_server"를 소켓 파일의 이름으로하여 서버와 연결
   sprintf(testfile, "%s%s", "testfile_", name);
   sprintf(dirname, "%s%s", name, "_download");
   if ((dp = opendir("./")) == NULL) { //현재 dir open
      perror("opendir: ./");
      exit(1);
   }
   make_dirname(testfile);
   len = recv(sd, buf, MAX_LEN, 0); 
   buf[len] = '\0';   // 서버로부터 방 개수 전달받음.
   room_num = atoi(buf);   // 문자열로 된 방 개수를 정수로 바꿔서 변수에 저장.
	
   if (room_num == 0) {   // 처음 접속했을 때 방이 없는 경우
      send(sd, "2", strlen("2"), 0); // 서버에게 2번 기능임을 알림.
      create();   // 방을 개설하고 해당 방으로 이동.
   }
   else {   // 방이 있는 경우
      int option;
      send(sd, "room_list_print", strlen("room_list_print"), 0);   
            // 서버에게 "room_list_print"를 전달하여 채팅방 목록을 전달해줄 것을 요청
      room_list_print(room_num);   // 채팅방 목록을 출력한다.

      /* 방을 개설할지, 이동할지를 입력받고, 1번 또는 2번을 입력할 때까지 반복하여 입력 받음. */
      while (1) {   
         printf("\n1. 방 개설 2. 이동 \n");
         printf(">> 선택 : ");
         scanf("%d", &option); getchar();   

         if (!(option == 1 || option == 2))
            printf("1번 또는 2번을 선택해주세요 \n");
         else
            break;
         printf("\n\n");
      }
      if (option == 1) {   // 방을 개설하는 경우
         len = recv(sd, buf, MAX_LEN, 0); 
         buf[len] = '\0';   // 서버로부터 방 개수 전달받음.
         send(sd, "2", strlen("2"), 0);    // 서버에게 2번 기능임을 알림.
         create();   // 방을 개설하고 해당 방으로 이동.
      }
      else if (option == 2) {  // 이동할 방을 선택하고 해당 방으로 이동.
         len = recv(sd, buf, MAX_LEN, 0); 
         buf[len] = '\0';   // 서버로부터 방 개수 전달받음.
         move(room_num, 8);   // 옵션을 8로 주어 서버에 처음 연결되었을 때 방을 이동하는 동작을 수행한다.
      }
   }
   signal(SIGINT, int_handler);    
      // ctrl + c하면 기능을 선택할 수 있게 int_handler 호출
      // 자식 서버에 연결된 후에 시그널 핸들러를 등록하여 채팅방 들어가기 전에 사용할 수 없도록 했다. 
   FD_ZERO(&read_fds);
   while (1) {
      FD_SET(0, &read_fds);
      FD_SET(sd, &read_fds);
      /* 시그널 받아서 block되어 있던 select에서 에러가 발생하면 다시 select  */
      if (select(sd + 1, &read_fds, (fd_set *)0, (fd_set *)0, (struct timeval *)0) < 0) {
         select(sd + 1, &read_fds, (fd_set *)0, (fd_set *)0, (struct timeval *)0);
      }
      if (FD_ISSET(sd, &read_fds)) {   // 서버로부터 문자열이 전달되는 경우
         int size;
         if ((size = recv(sd, buf, MAX_LEN, 0)) > 0) {   
            buf[size] = '\0';   // 서버가 끝에 NULL을 넣어 전달하지 않으므로 맨 끝에 NULL 삽입.
              /* 서버가 종료될 때 exit 메시지를 전송하는데, 이 메시지를 전달받은 경우, 
                클라이언트도 같이 종료시켜 소켓 파일도 같이 삭제. */
            if (strcmp(buf, "exit") == 0) {  
                exit(1);
            }
            printf("%s \n", buf);   // 전달 받은 문자열 출력.
         }
      }
      if (FD_ISSET(0, &read_fds)) {   // 표준입력으로부터 문자열 입력받는 경우.
         send_str = (char *)malloc(MAX_LEN);   // 서버로 전달될 문자열을 저장할 변수.
         if (fgets(send_str, MAX_LEN, stdin) > 0) {   // 표준입력으로부터 문자열 입력 받음.
            send_str[strlen(send_str) - 1] = '\0';   
                // 개행을 삭제한다.(클라이언트와 서버는 각각 문자열에서 
                // 개행을 제거한 후 전달하고, 전달 받을 때는 뒤에 NULL을 붙여 사용한다.)
            int size = strlen(send_str);
            if (strcmp(send_str, "clear") == 0) {   // 사용자가 화면의 내용을 지우길 원하는 경우
               send(sd, "clear", strlen("clear"), 0);   // 서버에 "clear" 전달하여 clear 명령을 수행할 것을 알림.
               len = recv(sd, buf, MAX_LEN, 0); buf[len] = '\0';   // 서버로부터 채팅방 이름 전달 받음.
               system("clear");   // clear 명령 수행
               printf("\t\t**** %s 방에 입장하셨습니다 **** \n", buf);      // 채팅방 이름 출력.
            }
            else if (strcmp(send_str, EXIT_MESSAGE) == 0) {   // 사용자가 종료 메시지를 입력한 경우
               printf("종료하려면 ctrl + ￦를 눌러주세요\n");
            }
            else {
               nlen = strlen(name);
               sprintf(buf, "[%s] : %s", name, send_str);   // 서버에 보낼 문자열에 사용자의 이름을 붙여줌.
               if (send(sd, buf, size + nlen + 5, 0) != (size + nlen + 5))   // 에러 발생한 경우
                   printf("Error : error on socket.\n");   // 에러 메시지 출력.            
            }
       }
   }
   close(sd);
   closedir(dp);
   return 0;
}

/* 중복된 채팅방 이름인지 확인하기 위한 함수 */
void get_room_name(int sockfd, char buf[], size_t len, int flags) {   
   int flag = 0, length = len;
   char answer[MAX_LEN];
   do {
      	flag = 0;
      	fgets(buf, length, stdin); 
           buf[strlen(buf) - 1] = '\0';  // 개설할 방 이름 입력 받음.
      	send(sockfd, buf, strlen(buf), flags);   // 사용자가 입력한 방 이름을 서버에 전달한다.
      	len = recv(sockfd, answer, length, flags);   
           answer[len] = '\0';  
                      // 서버로부터 채팅방 이름이 유효한지 전달 받는다. 
                      // 유효하면, 클라이언트가 전달한 이름을 그대로, 실패하면, fail을 전송한다.
      	if (strcmp(buf, answer) != 0) {   // 채팅방 이름이 중복된 경우
         	    printf("채팅방 이름이 이미 존재합니다. \n");
         	    printf("재입력 >> ");
         	    flag = 1;
      	}
   } while (flag == 1);
   send(sockfd, buf, strlen(buf), 0);   // 채팅방명을 다시 서버에 전송한다.
}

/* 방을 생성하고 클라이언트를 생성한 방으로 연결해주는 함수. 
(부모와의 연결을 끊고, 자식 서버와 연결해줌, 부모는
클라이언트의 방 생성/이동을 관리하고, 자식 서버는 채팅방 서비스를 처리한다. */
void create() {
   char buf[MAX_LEN];
   int len;
   printf("개설할 방 이름 : ");
   get_room_name(sd, buf, MAX_LEN, 0);   
                   // 사용자가 중복되지 않은 방 이름을 입력할 때까지 
                   // 서버에 전달하여 유효성을 검사한다. 
                   // 방 목록은 부모 서버가 관리한다. 
   len = recv(sd, buf, MAX_LEN, 0);   
   buf[len] = '\0';  // 부모 서버로부터 소켓 파일 이름 전달 받음.
   close(sd);   // 부모 서버와 연결 끊음.
   sd = connect_cli(buf);   // 부모 서버로부터 전달받은 소켓 파일명으로 자식 서버와 연결.
   send(sd, name, strlen(name), 0);   // 사용자명을 서버에 전달한다.
   len = recv(sd, buf, MAX_LEN, 0);   
   buf[len] = '\0';   // 방 이름 전달 받음.
   system("clear");   // 채팅화면 clear
   /* 받은 방 이름을 파일 업/다운로드에 사용하도록 serv_dirname에 저장한다. */
   strcpy(serv_dirname, buf);
   printf("\t\t**** %s 방에 입장하셨습니다 **** \n", buf);   // 안내 메시지 출력.
}

/* 채팅방 목록을 출력해준다. */
void room_list_print(int room_num) {
   int len;
   char buf[MAX_LEN];
   printf("\n*************** 채팅방 목록 ***************\n\n");
   /* 방의 개수만큼 서버로부터 채팅방에 대한 정보를 전달 받아서 출력. */
   for (int i = 0; i < room_num; i++) {
      if ((len = recv(sd, buf, MAX_LEN, 0)) > 0) {
         	buf[len] = '\0';
         	printf("%s \n", buf);
      }
   }
   printf("\n******************************************\n");
}

/* 이미 만들어진 방으로 이동하는 함수이다. */
void move(int room_num, int option) {   
   int len;
   char buf[MAX_LEN];
   sprintf(buf, "%d", option);
   send(sd, buf, strlen(buf), 0);   // 서버에게 3번 또는 8번 기능임을 알림.
   /* 만약 채팅 방이 개설되어 있는 상태에서 사용자가 입장했을 때 방 이동을 선택한 경우, 
      move함수에 3을 전달하여 메뉴를 출력하지않도록 한다.
      사용자가 처음 서버에 접속한 경우에은 move함수를 호출하기 전에 이미 list를 출력하기 때문이다. */
   if (option == 3) {
      printf("\n****이동할 방 번호를 선택해주세요.****\n\n");
      room_list_print(room_num);
      printf(">> 선택 : ");
   }
   else if (option == 8) {
      printf(">> 이동할 방 번호 선택 : ");
   }
   scanf("%s", buf); getchar();  // 사용자로부터 방 번호 입력 받음.
   /* 사용자가 방 번호를 입력하지 않으면 오류 방지를 위해 계속해서 입력받는다. */
   while ((atoi(buf) <= 0 || atoi(buf) > room_num)) {    
      printf("생성되어 있는 방 번호(0 ~ %d)를 입력해주세요! \n", room_num);
      scanf("%s", buf); getchar();  // 사용자로부터 방 번호 입력 받음.
   }
   send(sd, buf, strlen(buf), 0);   // 부모 서버에 방 번호 전달.
   len = recv(sd, buf, MAX_LEN, 0);
   buf[len] = '\0';   // 부모 서버로부터 소켓 파일 이름 전달 받음.
   close(sd);   // 부모 서버와의 연결 끊음.
   sd = connect_cli(buf);   // 자식 서버와 연결
   send(sd, name, strlen(name), 0);   // 사용자명을 서버에 전달한다.
   len = recv(sd, buf, MAX_LEN, 0);   
   buf[len] = '\0';   // 방 이름 전달 받음.
   strcpy(serv_dirname, buf);
   system("clear");
   printf("\t\t**** %s 방에 입장하셨습니다 **** \n", buf);   // 안내 메시지 출력.
}

/* SIGINT 시그널에 대한 handler 함수이다. */
void int_handler(int signo) {   
   struct dirent *dent;
   char buf[MAX_LEN];
   char filename[50];
   int search_flag = 0;
   int opt;
   int option, room_num, len;
   char chatfile[MAX_LEN];
   char tmpbuf[MAX_LEN];
   char searchbuf[MAX_LEN];
   FILE *fp;
   DIR *dp;

   printf("\n1. 채팅방 목록 확인 2. 채팅방 생성 3. 채팅방 이동 4. 파일 올리기 5. 파일 내려받기 6, 내 파일목록 7. 채팅 검색 \n");   
        // 기능 출력.
   printf("선택 : ");
   scanf("%d", &option);
   getchar();   // 어떤 기능을 사용할지 사용자로부터 입력받음.

   /* 방을 생성하거나 이동하는 경우 */
   if (option == 2 || option == 3) { 
      send(sd, EXIT_MESSAGE, strlen(EXIT_MESSAGE), 0);
      	// 자식 서버와 연결을 끊기전에 exit 메시지를 전달하여 클라이언트가 연결 해제했음을 알린다. 
      close(sd);   // 방을 이동할 것이기 때문에 자식 서버와의 연결 끊음.
      sd = connect_cli(SOCK_NAME);   // 방의 이동/생성을 관리하는 부모 서버와 연결
      len = recv(sd, buf, MAX_LEN, 0);   
      buf[len] = '\0';   // 부모 서버로부터 방 개수 전달받음.
      room_num = atoi(buf);   // 문자열로된 방 개수를 정수로 변환하여 저장.
      sprintf(buf, "%d", option);   // 서버로 전달하기 위해 사용자로부터 입력받은 option를 문자열로 변환.
      send(sd, buf, strlen(buf), 0);   // 서버에 옵션 전달.
   }
   
   /* 채팅방 목록을 출력하는 경우 */
   if (option == 1) {   
      send(sd, "room_list_print", strlen("room_list_printf"), 0);   
            // 서버에게 "room_list_print"를 전달하여 채팅방 목록을 전달해줄 것을 요청 
      len = recv(sd, buf, MAX_LEN, 0); buf[len] = '\0';   // 채팅방 개수 전달 받음.
      room_num = atoi(buf);   
      room_list_print(room_num);   // 방 개수만큼 채팅방 정보 출력.
   }
   
   /* 방 개설 */
   else if (option == 2) {
      create();   // 방 생성하고 해당 방으로 이동.
   }

   /* 방 이동 */
   else if (option == 3) {   
      move(room_num, 3);   // 이동할 방을 선택하고 해당 방으로 이동.
   }
   
   /* 파일 올리기 */
   else if (option == 4) { 
      lsls(dirname);  // 내 폴더의 파일 목록 보여주기 
      printf("업로드 할 파일 명 : ");
      scanf("%s", filename);  getchar();  // 업로드할 파일 명을 입력받아 filename에 저장함 
      if ((dp = opendir(dirname)) == NULL) { 
         perror("opendir: ./");
         exit(1);
      }
      while ((dent = readdir(dp)) != NULL) {  //폴더 안 파일들을 하나씩 추적
         if (strcmp(filename, dent->d_name) == 0) {  // 업로드할 파일이 제대로 있는 경우 
            search_flag = 1;
            printf("%s 을 전송합니다.\n", filename);
            cpcp(dirname, filename, serv_dirname);   // 서버 폴더로 cpcp
            printf("%s 전송 완료. \n", filename);
         }
      }
      if (search_flag != 1) {    // 업로드할 파일이 없는 경우 
         printf("%s 파일이 없습니다. \n", filename);
         printf("채팅방으로 돌아갑니다.\n");
         search_flag = 0;
      }
   closedir(dp);
   }
   
   /* 파일 내려받기 */
   else if (option == 5) { 
      lsls(serv_dirname);   // 서버의 폴더 목록 보여주기 
      printf("다운로드 할 파일 명 : ");
      scanf("%s", filename); getchar();  // 다운로드할 파일 명을 입력받아 filename에 저장 
      if ((dp = opendir(serv_dirname)) == NULL) {   //서버의  폴더 open
         perror("opendir: ./");
         exit(1);
      }
      while ((dent = readdir(dp)) != NULL) {
         if (strcmp(filename, dent->d_name) == 0) {   // 파일이 제대로 있는 경우
            search_flag = 1;
            printf("%s 을 다운로드 합니다.\n", filename);
            cpcp(serv_dirname, filename, dirname);   // 서버폴더에서 내 폴더로 cpcp
            printf("%s 다운로드 완료. \n", filename);
         }
      }
      if (search_flag != 1) {
         printf("%s 파일이 없습니다. \n", filename);
         printf("채팅방으로 돌아갑니다.\n");
         search_flag = 0;
      }
   closedir(dp);
   }

/* 내 파일 목록 보여주기 */
   else if (option == 6) {
      lsls(dirname); 
}

/* 대화 내용 검색 */
   else if (option == 7) {
      sprintf(chatfile, "%s_%s.txt", serv_dirname, "chat");   // 대화 내용 저장 파일 이름을 chatfile 로 한다. 
      chdir(serv_dirname);  // 현재 디렉토리를 공유 폴더로 변경한다. 
      if ((fp = fopen(chatfile, "r")) == NULL) {
         perror("fopen");
         exit(1);
      }
      printf("===============================================\n");
      printf("[옵션을 선택해주세요]\n");
      printf(">> 1.시간 검색 2.단어 검색\n");
      printf(">> ");
      scanf("%d", &opt);   getchar();
      if (opt == 1) {
         printf(">> 검색할 시간 입력 (hh:mm) : ");
         scanf("%s", searchbuf);
         search_flag = 0;
         while (fgets(tmpbuf, sizeof(tmpbuf), fp) != NULL) {
            if (search_flag != 0)
               printf("%s", tmpbuf);
                 // search_flage가 0이 아니라면, 검색이 이전에 성공하였음을 의미함으로
                 // 이후의 문장은 전부 출력한다. 
            else {
               if (strstr(tmpbuf, searchbuf) != NULL) {
                  printf("%s", tmpbuf);  // 처음 일치하는 경우에 해당한다. 
                  search_flag = 1;
               }
            }
         }
      }
      else {
         printf(">> 검색할 단어 입력 : ");
         scanf("%s", searchbuf);   getchar();
         while (fgets(tmpbuf, sizeof(tmpbuf), fp) != NULL) {
            if (strstr(tmpbuf, searchbuf) != NULL) {
               printf("%s", tmpbuf);
               search_flag = 1;
            }
         }
      }
      if (search_flag == 0)
         printf(">> 문자열을 찾지 못했습니다.\n");
      printf("===============================================\n");
      search_flag = 0;  // 기능 종료 후 search_flag를 초기화한다. 
      chdir("..");
    fclose(fp);
   }

   /* 잘못된 옵션 */
   else {
       printf("잘못된 옵션입니다. 채팅방으로 돌아갑니다.\n");
   }
}

/* 소켓 파일명을 sock_name으로 하여 서버와 연결해주는 함수이다. */
int connect_cli(char * sock_name) {  
   struct sockaddr_un ser, cli;
   int sd;
   int len, clen;
   if ((sd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
      perror("socket");
      exit(1);
   }
   memset((char *)&ser, 0, sizeof(struct sockaddr_un));
   ser.sun_family = AF_UNIX;
   strcpy(ser.sun_path, sock_name);
   len = sizeof(ser.sun_family) + strlen(ser.sun_path);
   if (connect(sd, (struct sockaddr *)&ser, len) < 0) {
      perror("bind");
      exit(1);
   }
   return sd;
}

/* ls 명령을 수행하는 함수이다. */
void lsls(char* dirname) {
   struct dirent **namelist;
   int n;
   int i;
   char is_chat[50];
   sprintf(is_chat, "%s%s", serv_dirname, "_chat.txt");
   n = scandir(dirname, &namelist, NULL, alphasort);
   printf("\n");
   printf("----- %s 폴더의 파일들 -----\n", dirname);
   for (i = 0; i < n; i++) {
      if (namelist[i]->d_name[0] == '.' || strcmp(namelist[i]->d_name, is_chat) == 0) 
      	continue;
      printf("%-1s  ", namelist[i]->d_name);
   }
   printf("\n");
   printf("--------------------------------------\n");
   printf("\n");
}

/* cp 명령어를 수행하는 함수이다. */
void cpcp(char *dirname, char *argv1, char *argv2) {
   int fd1, fd2;
   int rd;
   char buf[10];
   struct stat st;
   struct stat st2;

   chdir(dirname);
   if ((fd1 = open(argv1, O_RDONLY)) == -1) { // fd1 : open argv1
      perror("open");
      exit(0);
   }
   stat(argv1, &st);   //st에 argv1에 대한 stat 저장
   stat(argv2, &st2);   //st2에 argv2에 대한 stat 저장
   chdir("..");
   chdir(argv2);   // 해당 디렉토리로 이동 후 open 

   if ((fd2 = open(argv2, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO))) == -1) {
      perror("open2");
      exit(0);
         // “argv2, st.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO” 는 권한 복사를 위함
   }
   rename(argv2, argv1); // 이후 argv1로 이름을 변경한다. (디렉토리 명과 같은 이름으로 생성되는 것 방지하기 위해) 
   chdir("..");
   while ((rd = read(fd1, buf, 10)) > 0) {
      if (write(fd2, buf, rd) != rd) 
      	perror("Write");   // fd1 -> fd2 로 복사 진행
   close(fd1);
   close(fd2);
   }
}

/* 폴더를 만드는 함수이다. 폴더명이 중복되는지 확인하는 코드를 포함한다. */
void make_dirname(char* testfile) {
   int i = 1;
   char dir_buf[50];
   char file_buf[50];
   char name_buf[50];
   while (1) {
      if (access(dirname, F_OK) == 0) {
         i++;
         sprintf(name_buf, "%s(%d)", name, i);
         sprintf(dir_buf, "%s%s", name_buf, "_download");
         sprintf(file_buf, "%s%s", "testfile_", name_buf);
         strcpy(dirname, dir_buf);
         strcpy(testfile, file_buf);
      }
      else
         break;
   }

   if (i != 1) {
      printf("이미 있는 닉네임이라서 개인 폴더를 -%s- 로 생성합니다.\n", dirname);
   }
   if (mkdir(dirname, 0766) == -1) {
      perror("mkdir");
      exit(1);
   }
   chdir(dirname);
   creat(testfile, 0644);
   chdir("..");
}

/* 클라이언트가 종료되면서 처리해야할 동작을 수행하는 함수 */
void ifexit(void) {   
   struct dirent *dent;
   DIR *dp;
   /* 만들어놓은 디렉토리 지우기 */
   if ((dp = opendir(dirname)) == NULL) { 
      perror("opendir: ./");
      exit(1);
   }
   chdir(dirname);
   	
   while ((dent = readdir(dp)) != NULL) {
      remove(dent->d_name); // 폴더 내 파일들도 제거 
   }
   chdir("..");
   rmdir(dirname); // 디렉토리 제거 
   closedir(dp);
}

/* 종료 조건의 시그널을 재정의 했음을 알리는 handler */
void tstp_handler(int signo) {
   	printf("\n종료하시려면 \"ctrl + ￦\"를 입력해주세요 \n"); 
}

/* 클라이언트 종료를 위한 handler */
void quit_handler(int signo) {
   	send(sd, EXIT_MESSAGE, strlen(EXIT_MESSAGE), 0);   // 서버에 종료 메시지를 전달하여, 클라이언트가 접속 해제됨을 알림.
   	printf("\n클라이언트를 종료합니다...\n");
   	exit(1);   // 클라이언트가 종료되면서 처리해야할 동작을 수행하는 함수가 호출되고, 클라이언트가 종료된다.
}
