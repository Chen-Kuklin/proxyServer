//miri zakay 207566969
#include <stdio.h>  
#include <stdlib.h>    
#include <sys/types.h>
#include <sys/socket.h>  
#include <netinet/in.h>
#include <unistd.h>
#include "threadpool.h"
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netdb.h>

#define MAX_LENGTH 500

char** filter_array;
int filter_len;

//declerations
int Request_handling (void* sock);
char* error_files(int err);
int check_line(char * line);
int chek_host(char* temp_req,char* host);
void fill_filter_array(char * filter);
void free_arr();

int main(int argc, char* const argv[]){
    if(argc != 5){
        perror("Usage: server <port> <pool-size> <max-number-of-jobs> <filter>\n");
        exit(1);
    }
    int max_reqests=atoi(argv[3]);
    int p_size = atoi(argv[2]);
    int sockfd1, sockfd2,i;
    struct sockaddr_in server_addr;
    threadpool * th_pool = NULL;
    sockfd1=socket( AF_INET, SOCK_STREAM, 0);
    if( sockfd1 < 0){
        perror("error: <sys_call>\n");
        exit(1);
    }
    server_addr.sin_family = AF_INET;    
    server_addr.sin_addr.s_addr = INADDR_ANY;             
    server_addr.sin_port = htons( atoi(argv[1]) );    
    if( bind(sockfd1, (struct sockaddr*)&server_addr, sizeof(server_addr) ) < 0 ) {
            close(sockfd1);
            perror("error: <sys_call>\n");
            exit(1);
    }
    if( listen( sockfd1, max_reqests ) < 0 ){
            close(sockfd1);
            perror("error: <sys_call>\n");
            exit(1);
    }
    th_pool = create_threadpool(p_size);
    fill_filter_array(argv[4]);
    for(i=0;i<max_reqests;i++){
        sockfd2=accept(sockfd1,NULL,NULL);//create new socket
        if(sockfd2 < 0 ){  
            close(sockfd1);
            perror("error: <sys_call>\n");
            exit(1);
        } 
        dispatch(th_pool, Request_handling, &sockfd2);//The handling function
    }
    destroy_threadpool(th_pool);
    free_arr();
    close(sockfd1);
    return 0;
}
//--------------------------------------------------------------------------------------
int Request_handling (void* sock){//Request Handling Function                            
    int sockfd = *((int*)sock),sockf;
    int check,port;
    char http_req [MAX_LENGTH],temp_req[MAX_LENGTH],host[30],line[MAX_LENGTH],response[MAX_LENGTH];
    char *first_line,*err,*cport;
    struct hostent* server;
    int ret = read(sockfd, line, MAX_LENGTH);
    while(ret>0){
        strcat(http_req,line);
        if(strcmp(line, "\r\n")==0){
            break;
        }
        memset(line,0,strlen(line));
        ret = read(sockfd, line, MAX_LENGTH);  
    }
    if(ret<0){
        perror("error: <sys_call>\n");
        close(sockfd);
        return -1;
    }
    strcpy(temp_req,http_req);
    first_line=strtok(temp_req,"\r\n");
    check=check_line(first_line);//check the first line
    if(check!=0){
        err=error_files(check);
        write(sockfd,err,MAX_LENGTH);//error message
        memset(err,0,strlen(err));
        free(err);
        close(sockfd);
        return 0;
    }
    strcpy(temp_req,http_req);
    check=chek_host(temp_req,host);//check the host
    if(check!=0){
        err=error_files(check);
        write(sockfd,err,MAX_LENGTH);//error message
        memset(err,0,strlen(err));
        free(err);
        close(sockfd);
        return 0;
    }
    server=gethostbyname(host);
    if (server==NULL){
        err=error_files(404);
        write(sockfd,err,MAX_LENGTH);//error message
        memset(err,0,strlen(err));
        free(err);
        close(sockfd);
        return 0;
    }
    cport=strstr(host,":");
    if(cport==NULL)
        port=80;
    else{
        cport=cport+1;
        port=atoi(cport);
    }
    sockf=socket(AF_INET,SOCK_STREAM,0); //connect to remote host
    if(sockf<0){
        perror("error: <sys_call>\n");
        exit(1);
    }
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    bcopy((char *)*(server->h_addr_list), (char *)&serv_addr.sin_addr.s_addr,(server->h_length));
    serv_addr.sin_port = htons(port);
    if(connect(sockf,(const struct sockaddr*)&serv_addr,sizeof(serv_addr))<0){
        perror("error: <sys_call>\n");
        exit(1);
    }
    write(sockf, http_req, strlen(http_req));
    while ((ret= read(sockf, response, MAX_LENGTH))>0){//read the respone from the remote host
        write(sockfd, response, strlen(response));//write to client
    }
    close(sockf); //remote host
    close(sockfd); //client
    return 0;
}
//--------------------------------------
char* error_files(int err){//Creates the error message
    char* error=malloc(sizeof(char)*700);
    if(error==NULL)
        return NULL;
    char* one = "HTTP/1.0 " ;
    char* two = "\r\nServer: webserver/1.0\r\nContent-Type: text/html\r\nContent-Length: " ;
    char* three = "\r\nConnection: close\r\n\r\n<HTML><HEAD><TITLE>";
    char* four ="</TITLE></HEAD>\r\n<BODY><H4>" ;
    char* five = "</H4>\r\n";
    char* six= "\r\n</BODY></HTML>\r\n";
    char kind[30];
    char length[10];
    char special[30];
    if(err==400){
        strcpy(kind,"400 Bad Request");
        strcpy(length,"113");
        strcpy(special,"Bad Request.");
    }
    else if(err==403){
        strcpy(kind,"403 Forbidden");
        strcpy(length,"111");
        strcpy(special,"Access denied.");
    }
    else if(err==404){
        strcpy(kind,"404 Not Found");
        strcpy(length,"112");
        strcpy(special,"File not found.");
    }
    else if(err==500){
        strcpy(kind,"500 Internal Server Error");
        strcpy(length,"144");
        strcpy(special,"Some server side error.");
    }
    else if(err==501){
        strcpy(kind,"501 Not supported");
        strcpy(length,"129");
        strcpy(special,"Method is not supported.");
    }
    strcat(error,one);  
    strcat(error,kind);
    strcat(error,two);
    strcat(error,length);
    strcat(error,three);
    strcat(error,kind);
    strcat(error,four);
    strcat(error,kind);
    strcat(error,five);
    strcat(error,special);
    strcat(error,six);
    return error;
}
//-----------------------------------
int check_line(char * line){
    char* method,*protocol;
    method=strtok(line," ");
    protocol=strtok(NULL," ");
    protocol=strtok(NULL," ");
    if(strtok(NULL," ")!=NULL||protocol==NULL)//Make sure the first row consists of 3 sections
        return 400;
    if(strcmp(method,"GET")!=0)//Only GET METHOD should be supported
        return 501;
    if(strcmp(protocol,"HTTP/1.1")!=0 && strcmp(protocol,"HTTP/1.0")!=0)//Check that the protocol is one of the HTTP versions
        return 400;
    return 0;
}
//-----------------------------------
int chek_host(char* temp_req,char* const host){
    int i;
    char* Host="Host:";
    char* line=strtok(temp_req,"\r\n");
    memset(host,0,strlen(host));
    while(line!=NULL){
        for(i=0;i<5;i++){
            if(Host[i]!=line[i])
            break;
        }
        if(i==5){//We found the HOST line
            while(line[i]==' ')
                i++;
            line+=i;
            strcpy(host,line);
            break;
        }
        line=strtok(NULL,"\r\n");
    }
    if(host[0]==0){//If there is no HOST
        return 400;
    }
    for(i=0;i<filter_len;i++){//Check if the host is in the filter
        if(strcmp(host,filter_array[i])==0){
            return 403;
        }
    }
    return 0;
}
//-------------------------------------
void fill_filter_array(char * filter){//Fills the array according to the filter file
    int i=0;
    ssize_t read;
    FILE * file = NULL;
    char* line=malloc(sizeof(char)*50);
    size_t len;
    file = fopen(filter, "r");//Read only and write to the socket.
    if( file == NULL ){
        perror("error: <sys_call>\n");
        exit(1);
    }
    while ((read = getline(&line, &len, file)) != -1)
       filter_len++;
    rewind(file);
    filter_array=malloc(filter_len*sizeof(char*));
    for(i=0;i<filter_len;i++){
        getline(&line, &len, file);
        filter_array[i]=malloc(sizeof(char)*strlen(line));
        strcpy(filter_array[i], line);
        filter_array[i][strlen(line)-2]=0;
    }
    fclose(file);
    free(line);
}
//-------------------------------
void free_arr(){//free the array
    int i;
    for(i=0;i<filter_len;i++){
        free(filter_array[i]);
    }
    free(filter_array);
}