//this is force us to create a kernel entry routinue which does not point to byte 0x0 in our kernel，but to an actual label which we know that launches it，the case is `main()`
void dummy_test_entrypoint(){}


void main(){
    char *video_memory = (char*)0xb8000;
    *video_memory = 'X';
}