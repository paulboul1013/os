//read a byte from specified port
unsigned char port_byte_in(unsigned short port){
    unsigned char result;
    //inline assembler syntax

    //the source and destination is form nasm
    
    //"=a" (result) ; set '=' the c variable to the value of al register 
    //"d" (port) ; map the c variable (port) into dx register

    __asm__("in %%dx, %%al" : "=a" (result) : "d"(port));
    return result;
}

void port_byte_out(unsigned short port, unsigned char data){
    //nothing is return ，no '=' here
    //can see ','  since there are two variables in the input area
    __asm__("out %%al, %%dx" : : "a" (data), "d" (port));
}


unsigned short port_word_in(unsigned short port){
    unsigned short result;
    __asm__("in %%dx, %%ax" : "=a" (result) : "d"(port));
    return result;
}

void port_word_out(unsigned short port, unsigned short data){
    __asm__("out %%ax, %%dx" : : "a" (data), "d" (port));
}