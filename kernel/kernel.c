#include "../drivers/ports.h"


void main(){


    //screen cursor position: ask vga control register (0x3d4) for bytes
    // 14 is high byte, 15 is low byte
    port_byte_out(0x3d4, 14);//request the high byte

    int position =port_byte_in(0x3d5);//data is returned in vga register(0x3d5)
    position = position << 8; //high byte

    port_byte_out(0x3d4, 15);//request the low byte
    position+=port_byte_in(0x3d5);


    //vga cells consist of the character and color attributes
    int offset_from_vga=position*2;


    char *vga =(char*)0xb8000;
    vga[offset_from_vga]='X';
    vga[offset_from_vga+1]=0x0f;//white text on black


}