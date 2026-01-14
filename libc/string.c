#include "string.h"
#include <stdint.h>

void int_to_ascii(int n,char str[]){
    int i,sign;
    if ((sign=n)<0){
        n=-n;
    }
    i=0;
    do{
        str[i++]=n%10+'0';
    }while((n/=10)>0);

    if (sign<0) str[i++]='-';
    str[i]='\0';

    reverse(str);
}


void hex_to_ascii(int n,char str[]) {
    str[0] = '\0';
    append(str,'0');
    append(str,'x');
    char zeros=0;

    int32_t tmp;
    int i;
    for (i=28;i>0;i-=4){
        tmp=(n>>i)&0xF;
        if (tmp==0 && zeros==0) continue;
        zeros=1;
        if (tmp>0xA) append(str,tmp-0xA+'a');
        else append(str,tmp+'0');
    }

    tmp=n&0xF;
    if (tmp>=0xA) append(str,tmp-0xA+'a');
    else append(str,tmp+'0');
}

void reverse(char s[]){
    int c,i,j;
    for(i=0,j=strlen(s)-1;i<j;i++,j--){
        c=s[i];
        s[i]=s[j];
        s[j]=c;
    }
}


int strlen(const char s[]){
    int i=0;
    while (s[i]!='\0') ++i;
    return i;
}

void backspace(char s[]){
    int len=strlen(s);
    if (len > 0) {
        s[len-1]='\0';
    }
}
void append(char s[],char n){
    int len=strlen(s);
    s[len]=n;
    s[len+1]='\0';
}
int strcmp(char s1[],char s2[]){
    int i=0;
    for(i=0;s1[i]==s2[i];i++){
        if (s1[i]=='\0') return 0;
    }
    return s1[i]-s2[i];
}

// 檢查字串 s 是否以 prefix 開頭
// 返回 1 如果匹配，0 如果不匹配
int strstartswith(const char s[],const char prefix[]){
    int i=0;
    while(prefix[i]!='\0'){
        if(s[i]=='\0' || s[i]!=prefix[i]){
            return 0;
        }
        i++;
    }
    return 1;
}

void strcat(char dest[], const char src[]) {
    int i = 0;
    int j = strlen(dest);
    while (src[i] != '\0') {
        dest[j++] = src[i++];
    }
    dest[j] = '\0';
}

int atoi(char s[]) {
    int res = 0;
    int i = 0;
    while (s[i] >= '0' && s[i] <= '9') {
        res = res * 10 + (s[i] - '0');
        i++;
    }
    return res;
}
