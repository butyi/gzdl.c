#include <ctype.h>
#include <errno.h>
#include <fcntl.h> 
#include <string.h>
//#include <stropts.h>
#include <termios.h>
//#include <asm-generic/termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

//--------------------------------------------------------------------------------   
// Config (may to be changed for different controllers or bootloader revisions)
//--------------------------------------------------------------------------------   

#define ADDRESS_RESET_IT_PAGE 0xFF80


//--------------------------------------------------------------------------------   
// Global variables
//--------------------------------------------------------------------------------   

unsigned char connect_request[4];
unsigned char connect_response[4];
unsigned char trigger1_r;
unsigned char trigger2_r;
unsigned char trigger1_a;
unsigned char trigger2_a;

int old_protocol = 0;
int com_dump = 0;
int terminal = 0;
int mem_dump = 0;
int bflag = 0;
char *baudrate_str = NULL;
char *filename = NULL;
char *port = "ttyS0";

//--------------------------------------------------------------------------------   
// Local subroutines  
//--------------------------------------------------------------------------------   

void PrintHelp(){
  printf ("Description:\n");
  printf (" The tool can download software into my HC908GZ60 based hardware through RS232\n");
  printf (" port and after remaing in serial terminal mode. The terminal mode can be used\n");
  printf (" alone if no file is given for download. The tool communicates with bootloader called gzbl\n");
  printf (" already preloaded into the hardware. Usually system programs does reset to jump\n");
  printf (" back into bootloader if break character received from RS232. Therefore send break\n");
  printf (" is first stap to connect to bootloader. Connection checked by four connect\n");
  printf (" characters and bootloader responses also by four characters. If positive response\n");
  printf (" received, download frames can be sent one by one. When download succes and terminal\n");
  printf (" was requested, the tool remains in terminal mode to interact with the system\n");
  printf (" software through serial port. In all loops ESC key is the abort key.\n");
  printf (" There is possible to save a memory map file with the parsed S19 file (hmdl.bin).\n");
  printf (" There is possible to save a communication log file (hmdl.com).\n");
  printf (" There are two protocols supported. Old and new.\n");
  printf ("Options:\n");
  printf (" -o - Old protocol. Default is new protocol.\n");
  printf (" -t - Terminal after download.\n");
  printf (" -m - Memory dump file hmdl.bin created for debug purposes (See with 'xxd hmdl.bin').\n");
  printf (" -c - Communication dump file hmdl.com created for debug purposes (See with 'cat hmdl.com').\n");
  printf (" -b baud - Baudrate setting. Example: '57600'\n");
  printf (" -p path - Port to open. Example: 'ttyS0' for on board port /dev/ttyS0\n");
  printf (" -f path - File to download. Example: '/home/username/prg.s19'\n");
  printf ("License:\n");
  printf (" Code of tool is free for everyting you like. Reuse, modify, sell, everything.\n");
  printf ("Contact:\n");
  printf (" gzdl@butyi.hu  www.butyi.hu\n");
}

void printf_hex(FILE *fcd, char *prefix, unsigned char *buffer, int len){
  fwrite(prefix, strlen(prefix), 1, fcd);
  while(len--){
    fprintf(fcd, "%02X ",*buffer);
    buffer++;
  }
  fprintf(fcd, "\n");
}




//--------------------------------------------------------------------------------   
// Read S19 file and store data into global memory image (from Freescale AN2295SW)
//--------------------------------------------------------------------------------   

#define MAX_ADDRESS 0x10000 

struct t_byte{
  unsigned char d[MAX_ADDRESS]; //data byte
  unsigned char f[MAX_ADDRESS]; //flag for usage
} ;

struct t_byte image;

unsigned char buff[0x1000]; //for file line (one S record)


int read_s19(char *fn)   
{   
  FILE *f;   
  int max_address = MAX_ADDRESS;
  char c;   
  char* pc;   
  char afmt[7];   
  int u, b, sum, len, alen;   
  int addr, total = 0, addr_lo = MAX_ADDRESS, addr_hi = 0;   
  int line = 0, terminate = 0;   
  
  if((f = *fn ? fopen(fn, "r") : stdin) == NULL)   
  {   
    fprintf(stderr, "Can't open input file %s\n", fn);   
    return -1;   
  }   
   
  // initialize g_image   
  memset(&image.d, 0xff, sizeof(image.d));   
  memset(&image.f, 0, sizeof(image.f));   
   
  printf(" Parsing S-record...\n");   

  while(!terminate && (pc = fgets(buff, sizeof(buff), f)) != NULL)   
  {   
    line++;   
   
    // S-records only   
    if(*pc++ != 'S')    
      continue;   
   
    // record type   
    switch(c = *pc++)   
    {   
        case '0':   // S0 is skipped   
            continue;   
        case '1':   // S1 is accepted   
            alen = 4;   
            break;   
        case '2':   // S2 is accepted   
            alen = 6;   
            break;   
        case '3':   // S3 is accepted   
            alen = 8;   
            break;   
        case '9':   // S9 terminates   
        case '8':   // S8 terminates   
        case '7':   // S7 terminates   
            terminate = 1;   
            continue;   
        default:    // others are reported and skipped   
            fprintf(stderr, "Skipping S%c record at line %d", c, line);   
            continue;   
    }   
   
    // prepare len & addr scan format   
    sprintf(afmt, "%%2x%%%dx", alen);   
   
    // scan len & address   
    if(sscanf(pc, afmt, &len, &addr)!=2 || len >= 0x100)   
    {   
      fprintf(stderr, "S-record parse error at line %d\n", line);   
      return -1;   
    }   
   
    if(addr >= max_address)   
    {   
      fprintf(stderr, "Address out of range (%d >= %d) at line %d\n", addr, max_address, line);   
      return -1;   
    }   
   
   
    // skip len & address   
    pc += alen+2;   
       
    // init checksum   
    sum = len;   
    for(u=0; u<4; u++)   
      sum += (addr >> (u*8)) & 0xff;   
  
    // len & addr processed in record   
    len -= alen/2 + 1;   
   
    for(u=0; u<len; u++)   
    {   
      if(sscanf(pc, "%2x", &b) != 1 || b >= 0x100)   
      {   
        fprintf(stderr, "S-record data parse error at line %d\n", line);   
        return -1;   
      }   
      pc += 2;   
   
      image.d[addr+u] = b;   
      image.f[addr+u] = 1;   
      sum += b;   
      total++;   
    }   
   
    //printf("Bytes: %d", (int)total);   
   
    // test CS   
    if(sscanf(pc, "%2x", &b) != 1 || b >= 0x100 || ((sum+b)&0xff) != 0xff)   
    {   
      fprintf(stderr, "S-record checksum error at line %d\n", line);   
      return -1;   
    }   
  }   
   
  if (total == 0)   
  {   
    fprintf(stderr, "S-record contains no valid data!\n");   
    return -1;   
  }

  //print memory map info
  printf(" S-record is parsed. (Lines:%d Bytes:%d Areas:", line, (int)total);   
  b=0;
  for(u=0;u<=MAX_ADDRESS;u++){
    if(image.f[u] == 1 && addr_lo == MAX_ADDRESS){
      addr_lo = u; //begin of used block
    }
    if(image.f[u] == 0 && addr_lo != MAX_ADDRESS){  //end of used block
      if(0 < b)printf(",");
      printf("0x%04X-0x%04X", (int)addr_lo, (int)u-1);   
      addr_lo = MAX_ADDRESS;
      b++;
    }
  }
  printf(")\n");   

  fclose(f);   
  return 0;   
}   


//--------------------------------------------------------------------------------   
// Keyboard handling  
//--------------------------------------------------------------------------------   

int pressed_key;
int kbhit(void)
{
  struct termios oldt, newt;
  int oldf;
 
  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
 
  pressed_key = getchar();
 
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  fcntl(STDIN_FILENO, F_SETFL, oldf);
 
  if(pressed_key != EOF)
  {
    //ungetc(pressed_key, stdin);
    return 1;
  }
 
  return 0;
}

//--------------------------------------------------------------------------------   
// Serial port handling  
//--------------------------------------------------------------------------------   
int fd = 0;
FILE *fcd;
int baudrate=38400;
unsigned char answer[256];

int set_interface_attribs (int fd, int speed, int timeout)
{
  struct termios tty;
  int bspeed = 0;
  memset (&tty, 0, sizeof tty);
  if (tcgetattr (fd, &tty) != 0)
  {
    fprintf(stderr, "error %d from tcgetattr\n", errno);
    return -1;
  }

  switch(speed){
    case 115200: bspeed=B115200; break;
    case 57600: bspeed=B57600; break;
    case 38400: bspeed=B38400; break;
    case 19200: bspeed=B19200; break;
    //case 14400: bspeed=B14400; break; //unfortunately do not work :(
    case 9600: bspeed=B9600; break;
    case 4800: bspeed=B4800; break;
    case 2400: bspeed=B2400; break;
    case 1200: bspeed=B1200; break;
    default: bspeed=0; break;
  }
  
  if(bspeed){//usual baud rates
    cfsetospeed (&tty, bspeed);
    cfsetispeed (&tty, bspeed);
  } else {//strange baud rates
    tty.c_cflag &= ~CBAUD;
    //tty.c_cflag |= BOTHER; //may not work without this (error: ‘BOTHER’ undeclared)
    tty.c_ispeed = speed;
    tty.c_ospeed = speed;
  }

  tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
  // disable IGNBRK for mismatched speed tests; otherwise receive break
  // as \000 chars
  tty.c_iflag &= ~IGNBRK;         // disable break processing
  tty.c_lflag = 0;                // no signaling chars, no echo,
                                        // no canonical processing
  tty.c_oflag = 0;                // no remapping, no delays
  tty.c_cc[VMIN]  = 0;            // read doesn't block
  tty.c_cc[VTIME] = timeout;      // seconds read timeout

  tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

  tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
                                        // enable reading
  tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
  tty.c_cflag |= 0;
  tty.c_cflag &= ~CSTOPB;
  tty.c_cflag &= ~CRTSCTS;

  if (tcsetattr (fd, TCSANOW, &tty) != 0)
  {
    fprintf(stderr, "error %d from tcsetattr\n", errno);
    return -1;
  }
  return 0;
}

void set_blocking (int fd, int should_block)
{
  struct termios tty;
  memset (&tty, 0, sizeof tty);
  if (tcgetattr (fd, &tty) != 0)
  {
    fprintf(stderr, "error %d from tggetattr\n", errno);
    return;
  }

  tty.c_cc[VMIN]  = should_block ? 1 : 0;
  tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

  if (tcsetattr (fd, TCSANOW, &tty) != 0)fprintf(stderr, "error %d setting term attributes\n", errno);
}


int ConnectDevice(){  
  int answer_len;
  int i, j, k;
  i=0;
  
  fflush(stdout);
  while(1){
    if(i==10)printf("\r Please reset the device.\n");
    if(i==20)printf("\r Have you connected the device?\n");
    if(i==30)printf("\r Have you selected the proper port?\n");
    if(i==40)printf("\r Have you selected the proper baudrate?\n");

    memset(&answer, 0, 4); //clear answer array
    read(fd, answer, sizeof(answer));// clear rx buffer

    if(com_dump)printf_hex(fcd, "Tx: ", connect_request, sizeof(connect_request));
    tcsendbreak(fd, 0);
    write(fd, connect_request, sizeof(connect_request));
    answer_len = read(fd, answer, 4);  // read up to 4 characters if ready to read
    if(answer_len){
      if(com_dump)printf_hex(fcd, "Rx: ", answer, answer_len);

      //check answer if it is proper
      if(answer_len==sizeof(connect_response)){
        k=0;
        for(j=0;j<sizeof(connect_response);j++){//check answer bytes
          if(answer[j] == connect_response[j])k++;
        }
        if(k==sizeof(connect_response)){
          break; //Connection successfull
        }  
      }
    }

    if(i&1)printf("\r . . . "); else printf("\r  . . .");

    if(kbhit() && pressed_key == 27){ //ESC was pressed
      printf("\r User abort.\n");
      return 1; //User interrupt
    }
    i++;
  }
  printf ("\r Device connected\n");
  return 0; //OK
}

	
int SendPage( unsigned short address, unsigned char len, unsigned char or_byte ){
  int answer_len_need = 5;
  int answer_len;
  int x, k;
  int n=0;
  int cs=0;
  
  buff[n++]=trigger1_r;
  buff[n++]=trigger2_r;
  if(!old_protocol){ //new protocol
    //len, only in new protocol
    cs+=buff[n++]=0; //len high. Not used in GZ family because page size is just 128 bytes
    cs+=buff[n++]=len; //len low
  } else {
    address &= 0xFF80; //round to 128
  }
  cs+=buff[n++]=(address >> 8); //cim hi
  cs+=buff[n++]=(address & 0xFF); //cim lo
  for( x=0; x < len; x++ ){
    cs+=buff[n++]=(image.d[address+x] | or_byte); //data empty
  }
  buff[n++]=cs;

  memset(&answer, 0, 10); //clear answer array
  //printf("Send page %04X...\n",address);
  if(com_dump)printf_hex(fcd, "Tx: ", buff, n);
  write(fd, buff, n);


  answer_len = read(fd, answer, answer_len_need);  // read up to 5 or 3 characters if ready to read

  if(answer_len == 0){
    fprintf(stderr, "There was no answer for page %04X\n", address);
    return 1;
  }
  
  if(com_dump)printf_hex(fcd, "Rx: ", answer, answer_len);
    
  //check answer if it is proper
  if(answer_len < answer_len_need){
    fprintf(stderr, "There was too short answer for page %04X\n", address);
    return 1;
  }
    
  k=0;
  if(answer[k++] != trigger1_a){
    fprintf(stderr, "Wrong answer first header character for page %04X. %02X instead of %02X\n", address, answer[k-1], trigger1_a);
    return 1;
  }
  if(answer[k++] != trigger2_a){
    fprintf(stderr, "Wrong answer second header character for page %04X. %02X instead of %02X\n", address, answer[k-1], trigger2_a);
    return 1;
  }
  if(answer[k++] != (address >> 8)){
    fprintf(stderr, "Wrong answer address high byte for page %04X\n", address);
    return 1;
  }
  if(answer[k++] != (address & 0xFF)){
    fprintf(stderr, "Wrong answer address low byte for page %04X\n", address);
    return 1;
  }

  switch(answer[k]){
    case 0:
      printf(" Area from %04X with len %d is OK.\n",address,len);
      return 0; //Successfull
    case 1:
      printf(" Checksum error on area from %04X with len %d.\n",address,len);
      break;
    case 2:
      printf(" Address error on area from %04X with len %d.\n",address,len);
      break;
    case 3:
      printf(" Timeout error on area from %04X with len %d.\n",address,len);
      break;
    case 4://only on HM
      printf(" Len error on area from %04X with len %d.\n",address,len);
      break;
    default:
      printf(" Unknown error %d on area from %04X with len %d.\n",answer[k],address,len);
      break;
  }

  return 1; //Error

}

//--------------------------------------------------------------------------------   
// Main 
//--------------------------------------------------------------------------------   

int main (int argc, char *argv[]){

  int c, i, j, k;
  int answer_len = 0;

  printf("Linux downloader for MC68HC908GZ60 V0.00 2019.06.08. (github.com/butyi/gzdl.c)\n");
  
  //Parse command line parameters
  opterr = 0; j=0;
  while ((c = getopt (argc, argv, "ocmtb:f:p:")) != -1){
    j++;
    switch (c){
      case 'o':
        old_protocol = 1;
        printf (" Old protocol.\n");
        break;
      case 't':
        terminal = 1;
        printf (" Terminal after download.\n");
        break;
      case 'm':
        mem_dump = 1;
        printf (" Create memory dump file hmdl.bin (see with 'xxd hmdl.bin')\n");
        break;
      case 'c':
        com_dump = 1;
        printf (" Create communication dump file hmdl.com (See with 'cat hmdl.com')\n");
        break;
      case 'b':
        baudrate_str = optarg;
        sscanf(baudrate_str,"%d",&baudrate); 
        printf (" Baudrate is %d.\n", baudrate);
        break;
      case 'f':
        filename = optarg;
        break;
      case 'p':
        port = optarg;
        printf (" Port to open is %s.\n", port);
        break;
      case '?':
        if (optopt == 'c')
          fprintf (stderr, "Option -%c requires an argument.\n", optopt);
        else if (isprint (optopt))
          fprintf (stderr, "Unknown option `-%c'.\n", optopt);
        else
          fprintf (stderr,
                   "Unknown option character `\\x%x'.\n",
                   optopt);
        return 1;
      default:
        abort ();
    }
  }
  for (i = optind; i < argc; i++){
    printf ("Non-option argument %s\n", argv[i]);
  }  

  //Protol specific initialization
  if(old_protocol){
    memset(&connect_request, 0x0C, 4);
    memset(&connect_response, 0xF3, 4);
    trigger1_r = 0x55;
    trigger2_r = 0xAA;
    trigger1_a = 0xAA;
    trigger2_a = 0x55;
  } else {
    memset(&connect_request, 0x1C, 4);
    memset(&connect_response, 0xE3, 4);
    trigger1_r = 0x56;
    trigger2_r = 0xAB;
    trigger1_a = 0xBA;
    trigger2_a = 0x65;
  }  

  //Print help if no option is given 
  if(!j){
    PrintHelp();
    return 0;
  }


  //Parse S19 file 
  if(filename != NULL){
    printf ("S19 file to download is %s.\n", filename);
    if(0 != read_s19(filename))return 1;

    //Dump parsed s19 data as memory layout. Use linux command 'xxd' to see its content in hex dump format.
    if(mem_dump){
      FILE* f = fopen( "hmdl.bin", "wb" );
      fwrite( image.d, 1, MAX_ADDRESS, f );        
      fclose( f );
    }
  }


  //Open communication dump file
  if(com_dump){
    fcd = fopen( "hmdl.com", "wb" );
  }


  //Open and init serial port
  char file[256];
  sprintf(file,"/dev/%s", port);
  fd = open (file, O_RDWR | O_NOCTTY | O_SYNC);
  if (fd < 0){
    fprintf(stderr, "error %d opening /dev/%s: %s\n", errno, port, strerror (errno));
    goto exit_label;
  }
  set_interface_attribs (fd, baudrate, 5);  // set speed, 0.5s timeout
  set_blocking (fd, 0);                // set no blocking


  if(filename != NULL){//if download needed

  //Connecting to devive
  printf ("Attemp to connect to device. Press ESC to abort.\n");
  if(ConnectDevice() == 1){
    goto exit_label;
  }

  //Erase start vector page
  if(old_protocol){//only complete page with len 128
    printf ("Erase start vector page %04X first.\n",ADDRESS_RESET_IT_PAGE);
    if(SendPage( ADDRESS_RESET_IT_PAGE, 128, 0xFF )){//here it is not problem, that the complete page is erased. Content will be written again during download.
      goto exit_label;
    }
  } else {
    printf ("Erase start vector first on address 0xFFFE.\n");
    if(SendPage( 0xFFFE, 2, 0xFF )){
      goto exit_label;
    }
  }
  

  //Download data packets, last the new start vector
  if(old_protocol){//only complete page with len 128
    printf ("Download pages with old protocol.\n");
    for( i=0; i < MAX_ADDRESS; i+=128 ){ //for all pages
      //check if page contains data to be downloaded
      j=0;//start
      for( k=0; k < 128; k++ ){
        if( image.f[i + k]){
          j=1;
          break;
        }
      }
      if( !j ){
        continue; //next page
      }
    
      //page contains data, send it
      if(SendPage( i, 128, 0 )){
        goto exit_label;
      }
    
    }
  } else {//code snippets with correct len inside the page
    printf ("Download pages with new protocol.\n");
    for( i=0; i < MAX_ADDRESS; i+=128 ){ //for all pages

      j=0;//start address
      for( k=0; k < 128; k++ ){
        if( image.f[i+k] && !j){//start of snippet
          j=i+k;
          continue;
        }
        if( !image.f[i+k] && j){//en of snippet
          if(SendPage( j, i+k-j, 0 )){//this send only executes if there is empty space at end of page
            goto exit_label;
          }
          j=0;//administrate snippet was sent: clear start address of snippet
        }
      }
      
      if(j){//Snippet started but not yet sent
        if(SendPage( j, i+k-j, 0 )){//this send executes when last byte of page is also used 
          goto exit_label;
        }
      }
      
    }
  }
  printf ("Download success.\n");
  
  }//if download needed
    
  //Terminal
  if(terminal){
    printf ("Terminal started.\n");
    if(com_dump)fprintf(fcd, "Terminal started.\n");

    set_interface_attribs (fd, baudrate, 0);  // set speed, no timeout
    fflush(stdout);
    while(1){
      
      if(read(fd, answer, 1)){
        putc(answer[0],stdout);
        if(com_dump)putc(answer[0],fcd);
      }
      
      if(kbhit()){
        if(pressed_key == 27){ //ESC was pressed
          printf("User abort.\n");
          return 1; //User interrupt
        }
        buff[0]=pressed_key;
        //if(com_dump)putc(pressed_key,fcd); //commented out, usually sent characters are not needed to log
        write(fd, buff, 1);
      }
    }

  }


exit_label:
  
  //Close communication dump file
  if(com_dump){
    fclose( fcd );
  }

  
  return(0);
}



