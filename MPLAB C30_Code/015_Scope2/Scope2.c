// AjScope2 with 3byteCommands with "Reset"
// 02 Aug 2013: Basic Command Decode Matrix
//				Read VRef AN2 AN3 and return result as 4 bytes
//				Set Offset OC1 & OC2 PWM
// 03 Aug 2013: Added the Auto Mode Capture with Sample rate Settings
// 				Added the LED Test Mode
//				Added the Interrupt Mode for Normal sampling
//		        Added the Interrupt Mode for Sliding CH1
// 				Added the FFT mode with seperate functions resolving __delay32() problem
// 04 Aug 2013: Changed to Tx in interrupt mode
// 06 Aug 2013: Single Byte Command Identifier and Variable Length Commands
//				Abort Single Byte Restarts Main Program
//				Sends Busy "B" and Clears Busy "b"
// 08 Aug 2013  Removed the ACK for Capture , Data &Busy
// 09 Aug 2013  Removed the initial valies from char capture_mode,sampling_mode, error,fft;
//				Added back the busy complete "Done"
//				Added 100usec delay between sent data
// 10 Aug 2013  Checksum 0X71dd First Version
// 11 Aug 2013  Introduced split read mode for 400 Data FFT 1/2 
//              Changed back rs232 to TX polled mode (Really not required)
//				Added back all the answerbacks except "C" & "D"
//
// 20 Mar 2016 Cleaned up indenting for readability
// ------------------------------------------------------------
// Problems:
// VB has errors when reading 400 data continuously
// 
// 
// 
//-------------------------------------------------------------
#include <p30f2020.h>
#include <rs232_C30.c>
#include <libpic30.h>
#include <adc_comparator.c>
#include <OCxPWM.c>
#include <simple_spi.c>

//Fuzes 16 MHz crystal
_FOSC(CSW_FSCM_OFF & OSC2_IO & HS);//No Clock Switching,OSC2 is an output, HS Crystal
_FOSCSEL(PRIOSC_PLL); 	// Primary Osc With PLL FPWM= Primary Osc X 32 ,
						//FOSC = Primary Osc X 4 FADC = Primary Osc X 16 ;
						//FCY = Primary Osc X 2 
_FWDT(FWDTEN_OFF);                            //Turn off WatchDog Timer
_FGS(CODE_PROT_OFF);                          //Turn off code protect
_FPOR( PWRT_OFF );                            //Turn off power up timer




//Function Prototypes
void readVref(void);
void capture_data(void);
void set_sampling(unsigned char rate);	
void get_normal_data(void);
void get_fft1_data(void);
void get_fft2_data(void);

unsigned char input_string[3],AN[405],data;
unsigned char capture_mode,sampling_mode, error,fft,sampling_rate;
int data_index;
unsigned long delay_cycles=10,sliding_delay;
unsigned long sliding_delay_cycles, adc_index;


#define busy   		LATEbits.LATE4 //  Busy
#define hw_reset	LATEbits.LATE5 //  HW_Reset


//ISR for CMP3
void __attribute__((__interrupt__, __auto_psv__)) _CMP3Interrupt(void){
	//Normal Triggered Non-Sliding Mode
	if (sampling_mode==0){ 
		if(fft==0){ //AN0 & AN1
			get_normal_data();
			busy=0;
			putHex(68);putHex(111);putHex(110);putHex(101);
			IEC1bits.AC3IE =0; //Disable interrupt
			//IFS1bits.AC3IF =0; //clear interrupt flag=0
		}
		else if(fft==1){ //AN0 
			get_fft1_data();
			busy=0;
			putHex(68);putHex(111);putHex(110);putHex(101);
			IEC1bits.AC3IE =0; //Disable interrupt
			//IFS1bits.AC3IF =0; //clear interrupt flag=0
		}
		else if(fft==2){ // AN1
			get_fft2_data();
			busy=0;
			putHex(68);putHex(111);putHex(110);putHex(101);
			IEC1bits.AC3IE =0; //Disable interrupt
			//IFS1bits.AC3IF =0; //clear interrupt flag=0
		}
	}
	//Triggered Sliding Mode	
	else if (sampling_mode==1){ //Sliding Mode
		if(fft==0){ //AN0 & AN1
			__delay32(sliding_delay_cycles);
			ADCPC0bits.SWTRG0 = 1; //start conversion of AN0 and AN1 store 200 samples each
			while(ADCPC0bits.PEND0){} //conv pending becomes 0 when conv complete
				AN[adc_index]=ADCBUF0>>2;
				adc_index++;
				AN[adc_index]=ADCBUF1>>2;
				adc_index++;
	
			IEC1bits.AC3IE =0; //Disable interrupt
		}
		else if(fft==1){ //AN0 
			__delay32(sliding_delay_cycles);
			ADCPC0bits.SWTRG0 = 1; //start conversion of AN0 and AN1 store 200 samples each
			while(ADCPC0bits.PEND0){} //conv pending becomes 0 when conv complete
				AN[adc_index]=ADCBUF0>>2;
				adc_index++;
			IEC1bits.AC3IE =0; //Disable interrupt
		}
		if(fft==2){ // AN1
			__delay32(sliding_delay_cycles);
			ADCPC0bits.SWTRG0 = 1; //start conversion of AN0 and AN1 store 200 samples each
			while(ADCPC0bits.PEND0){} //conv pending becomes 0 when conv complete
				AN[adc_index]=ADCBUF1>>2;
				adc_index++;
			IEC1bits.AC3IE =0; //Disable interrupt
		}
	}		
}

//----------------------------------------------------------------------
//Main Program	
int main( ){
	__delay32(3200);//initial delay 100uSec
	initUart(); //Initialise the UART
	__delay32(3200);//initial delay 100uSec

	hw_reset=0;
	TRISA=1; //PortA all inputs
	TRISE=0; //PortE all outputs
	initialiseADC ();
	initialiseComp();
	setupOC1PWM();
	setupOC2PWM();

	busy=1;

	//AjScope
	putHex(65);putHex(106);putHex(83);putHex(99);putHex(111);putHex(112);
	putHex(101);	putHex(13);putHex(10);

	__delay32(3200000); //initial delay 1Sec

	busy=0;
	
 	while(1){	
		input_string[0] =getHex();
		
		if ( error==0) { 
			if (input_string[0]==73){ 		//"I" Identify
				putHex(65);putHex(106);putHex(32);putHex(83);putHex(99);putHex(111);putHex(112);
				putHex(101);putHex(32);
				putHex(82);putHex(101);putHex(97);putHex(100);putHex(121);putHex(32);
				putHex(13);putHex(10);
            		}
            		else if (input_string[0]==65){  	//"A" Abort
				putHex(65);
				goto restart;
			}
			else if (input_string[0]==66){  	//"B" Read Busy
				putHex(66);
			}
        		else if  (input_string[0]==82){		//"R" Read Vref
				putHex(82);
				readVref();			
			}
			else if  (input_string[0]==68){  	//"D" Send Data
				//------putHex(68);
				input_string[1]=getHex();
				if(input_string[1]==1){ 	//CH1 200
					for(adc_index=0;adc_index<400;adc_index++){ //CH1 Data
						putHex(AN[adc_index]);	//8 bit AN0 Value
						adc_index++;
						__delay32(3200); 	//100us delay
					}
				}
				else if (input_string[1]==2){		//CH2 200
					for(adc_index=0;adc_index<400;adc_index++){ //Ch2 Data
						putHex(AN[adc_index+1]);//8 bit AN0 Value
						adc_index++;
						__delay32(3200); 	//100us delay
					}
				}
				else if (input_string[1]==3){		//First 200 of 400 samples CH1/CH2/FFT
					for(adc_index=0;adc_index<200;adc_index++){ //All Data
						putHex(AN[adc_index]);	//8 bit AN0 Value
						__delay32(3200); 	//100us delay
					}	
				}
				else if (input_string[1]==4){//Second 200 of 400 samples CH1/CH2/FFT
					for(adc_index=200;adc_index<400;adc_index++){ //All Data
						putHex(AN[adc_index]);//8 bit AN0 Value
						__delay32(3200); //100us delay
					}	
				}
			}
			else if (input_string[0]==83){  	//"S" Sample Rate
				putHex(83);
				input_string[1]=getHex();
				sampling_rate=input_string[1];
				set_sampling(sampling_rate);
			}
        		else if  (input_string[0]==78){  	//"N" Noise Filter
				putHex(78);
			}
			else if  (input_string[0]==67){  	//"C" Capture Data
				//----------putHex(67);
				capture_data();
			}
			else if (input_string[0]==76){  	//"L" Trig Level
				putHex(76);
				input_string[1]=getHex();
				input_string[2]=getHex();
				CMPDAC3bits.CMREF=input_string[1]*256 + input_string[2];//Comp3ref
			}
        		else if  (input_string[0]==84){  	//"T" Trig Source
				putHex(84);
				input_string[1]=getHex();
				capture_mode=input_string[1]; // 0/1/2 auto/Ch1/Ch2
			}
			else if  (input_string[0]==80){  	//"P" Trig Polarity
				putHex(80);
				input_string[1]=getHex();
				CMPCON3bits.CMPPOL=input_string[1]; // 0/1 =Normal /Inverted polarity
			}
			else if  (input_string[0]==71){  	//"G" Gains
				putHex(71);
				input_string[1]=getHex();
				input_string[2]=getHex();
				//Gain 1/2/5 Setting 0/1/3
				//Pga 1/2 Setting 1/2
				setpga(input_string[1], 1); //gain, pga= 1 
				setpga(input_string[2], 2); //gain, pga= 2 				
			}
			else if (input_string[0]==79){   	//"O" Offset Ch1
				putHex(79);
				input_string[1]=getHex();
				input_string[2]=getHex();
				OC1RS=input_string[1]*256 + input_string[2];//Set PWM1
   			}         
        		else if  (input_string[0]==111){  	//"o" Offset Ch2
				putHex(111);
				input_string[1]=getHex();
				input_string[2]=getHex();
				OC2RS=input_string[1]*256 + input_string[2];//Set PWM2
        		}
			else if  (input_string[0]==70){  	//"F" FFT mode
				putHex(70);
				input_string[1]=getHex();
				fft=input_string[1]; 		// 0/1/2  AN0&1/AN0/AN1
			}
			else if  (input_string[0]==100){  	//"d" Delay Post Trigger
				putHex(100);
			}
			else if  (input_string[0]==116){  	//"t" Test LED
				putHex(116);
				busy = ~ busy;
			}
			else { 					//"E" Error
				putHex(69);
				goto restart; 
   			}
		}
		//loop
	}
	restart:
	return(0);	
}
//Read Voltage Reference 10Bit
void readVref(void){
	//Reads the value of AN2 and AN3 and returns the 10bit Value
	// AN2msb);putHex(AN2lsb);putHex(AN3msb);putHex(AN3lsb);
	unsigned char AN2msb,AN2lsb,AN3msb,AN3lsb; 
	ADCPC0bits.SWTRG1 = 1;			//start conversion of AN3 and AN2
	Nop();Nop();Nop();
	while(ADCPC0bits.PEND1){} 		//conv pending becomes 0 when conv complete
       	AN2lsb = ADCBUF2 &0x00ff;       	// lsb of the ADC result
        AN2msb=(ADCBUF2 &0xff00)>>8;		// msb of the ADC result
	AN3lsb = ADCBUF3 &0x00ff;		// lsb of the ADC result
        AN3msb=(ADCBUF3 &0xff00)>>8;  		// msb of the ADC result
	putHex(AN2msb);putHex(AN2lsb);putHex(AN3msb);putHex(AN3lsb);
}

//Capture data
void capture_data(void){
	busy=1;//---------putHex(66);
	//Auto Mode "Free Running"
	if (capture_mode==0) {	
		if(fft==0){ //AN0 & AN1
			get_normal_data();
			busy=0;
			putHex(68);putHex(111);putHex(110);putHex(101);// Done for busy over
		}
		else if(fft==1){ //AN1 
			get_fft1_data();
			busy=0;
			putHex(68);putHex(111);putHex(110);putHex(101);
		}
		else if(fft==2){ //AN2 
			get_fft2_data();
			busy=0;
			putHex(68);putHex(111);putHex(110);putHex(101);
		}
 	}
	//CH1 Triggered 
	else if (capture_mode==1){ // Trigger by Channel 1  Comp3A
		// Trig Polarity set by "P"	
		// Trig Level set by "L"
		CMPCON3bits.INSEL=0; // 3A selected
		CMPCON3bits.CMPON=1; //Comparator ON 
		if (sampling_mode==0){ //Normal Non-Sliding Mode
			//ISR Collects the Data based on fft=0/1/2 modes
			IFS1bits.AC3IF =0; //clear interrupt flag=0
			IEC1bits.AC3IE =1; //Enable CMP3 interrupt
		}
		//CH1 Triggered Sliding Mode		
		else if (sampling_mode==1){ //Sliding Mode
			adc_index=0;
			while (adc_index <400){
				if(fft==0){
					sliding_delay_cycles =(sliding_delay * (adc_index >>1)/10 ); 
				}
				else {
					sliding_delay_cycles =(sliding_delay * (adc_index )/10 );
				}
				IFS1bits.AC3IF =0; //clear interrupt flag=0
				//ISR Collects the Data based on fft=0/1/2 modes
				IEC1bits.AC3IE =1; //Enable CMP3A interrupt
			}
			busy=0;
			putHex(68);putHex(111);putHex(110);putHex(101);
			
		}

	}	
	//CH2 Triggered
	else if (capture_mode==2){ // Trigger by Channel 2  Comp3B
		// Trig Polarity set by "P"		
		// Trig Level set by "L"
		CMPCON3bits.INSEL=1; // 3B selected
		CMPCON3bits.CMPON=1; //Comparator ON 
		if (sampling_mode==0){ //Normal Mode
			//ISR Collects the Data based on fft=0/1/2 modes
			IFS1bits.AC3IF =0; //clear interrupt flag=0
			IEC1bits.AC3IE =1; //Enable CMP3 interrupt
		}
		else if (sampling_mode==1){ //Sliding Mode
			adc_index=0;
			while (adc_index <400){
				if(fft==0){
					sliding_delay_cycles =(sliding_delay * (adc_index >>1)/10 ); 
				}
				else {
					sliding_delay_cycles =(sliding_delay * (adc_index )/10 ) ;
				}
				IFS1bits.AC3IF =0; //clear interrupt flag=0
				//ISR Collects the Data based on fft=0/1/2 modes
				IEC1bits.AC3IE =1; //Enable CMP3A interrupt
			}
			busy=0;
			putHex(68);putHex(111);putHex(110);putHex(101);
		}
	}
}
void set_sampling(unsigned char rate){
//	11 for rounding off to 2usec +64*N  for additional 2*N usec
	if(rate == 1){		// 2 us/sample		500kbps 	20us/div
		delay_cycles=11;sampling_mode=0;}
	else if(rate == 2){	// 5 us/sample		200kbps 	50us/div
		delay_cycles=11+96;sampling_mode=0;} 
	else if(rate == 3){	// 10 us/sample 	100kbps  	0.1ms/div
		delay_cycles=11+256;sampling_mode=0; }
	else if(rate == 4){	// 20 us/sample 	50kbps  	0.2ms/div
		delay_cycles=11+576;sampling_mode=0;} 
	else if(rate == 5){  // 50 us/sample 		20kbps  	0.5ms/div
		delay_cycles=11+1536;sampling_mode=0;} 
	else if(rate == 6){  // 100 us/sample 		10kbps 		1ms/div
		delay_cycles=11+3136;sampling_mode=0;} 
	else if(rate == 7){  // 200 us/sample 		5kbps 		2ms/div
		delay_cycles=11+6336;sampling_mode=0;} 
	else if(rate == 8){  // 500 us/sample 		2kbps 		5ms/div
		delay_cycles=11+15936;sampling_mode=0;} 
	else if(rate == 9){  // 1000 us/sample 		1kbps 		10ms/div
		delay_cycles=11+31936;sampling_mode=0;} 
	else if(rate == 10){  // 2000 us/sample 	500bps 		20ms/div
		delay_cycles=11+63936;sampling_mode=0;} 
	else if(rate == 11){  // 5000 us/sample 	200ps 		50ms/div
		delay_cycles=11+159936;sampling_mode=0;} 
	else if(rate == 12){  // 10000 us/sample 	100bps 		0.1s/div
		delay_cycles=11+319936;sampling_mode=0;} 
	else if(rate == 13){  // 20000 us/sample 	50bps 		0.2s/div
		delay_cycles=11+639936;sampling_mode=0;} 
	else if(rate == 14){  // 50000 us/sample  	20bps 		0.5s/div
		delay_cycles=11+1599936;sampling_mode=0;} 
	else if(rate == 15){  // 100000 us/sample 	10bps 		1.0s/div
		delay_cycles=11+3199936;sampling_mode=0;}
	//----------------------------------Sliding Mode	
	else if(rate == 16){  // 1us shift/sample 	1msps 		10us/div
		sliding_delay=320;sampling_mode=1;} 		
	else if(rate == 17){  // 0.5us shift/sample 2msps 		5us/div
		sliding_delay=160;sampling_mode=1;} 
	else if(rate == 18){  // 0.2us shift/sample 5msps 		2us/div
		sliding_delay=64;sampling_mode=1;} 
	else if(rate == 19){  // 0.1us shift/sample 10msps 		1us/div
		sliding_delay=32;sampling_mode=1;} 
	else if(rate == 20){  // 0.05us shift/sample 20msps 	0.5us/div
		sliding_delay=16;sampling_mode=1;} 
		
		
	//for sliding_delay = 320  1.0usec increments every 	32 =  1Mbps
	//                  = 160  0.5usec increment every  	16 =  2Mbps
	//                  = 64  0.2usec increment every   	6.4 = 5Mbps
	//                  = 32     0.1usec increment every  	3.2 = 10Mbps
	//                  = 16     0.05usec increment every 	1.6 = 20Mbps	
}
void get_normal_data(void){
	int temp=0;	
	ADCPC0bits.SWTRG0 = 1; //start conversion of AN0 and AN1 store 200 samples each
	while(temp<400){    
		while(ADCPC0bits.PEND0){} //conv pending becomes 0 when conv complete
		AN[temp]=ADCBUF0>>2;
		temp++;
		AN[temp]=ADCBUF1>>2;
		temp++;
		__delay32(delay_cycles);
		ADCPC0bits.SWTRG0 = 1; //start conversion of AN3 and AN2
	}
}
void get_fft1_data(void){
	int temp=0;	
	ADCPC0bits.SWTRG0 = 1; //start conversion of AN0 and store 400 samples 
	while(temp<400){    
		while(ADCPC0bits.PEND0){} //conv pending becomes 0 when conv complete
		AN[temp]=ADCBUF0>>2;
		temp++;
		__delay32(delay_cycles+6);
		ADCPC0bits.SWTRG0 = 1; //start conversion of AN0 and AN1
							// sets PEND0 to 1
	}
}
void get_fft2_data(void){
	int temp=0;	
	ADCPC0bits.SWTRG0 = 1; //start conversion of AN1 store 400 samples 
	while(temp<400){    
		while(ADCPC0bits.PEND0){} //conv pending becomes 0 when conv complete
		AN[temp]=ADCBUF1>>2;
		temp++;
		__delay32(delay_cycles+6);
		ADCPC0bits.SWTRG0 = 1; //start conversion of AN0 and AN1
								// sets PEND0 to 1
	}
}
