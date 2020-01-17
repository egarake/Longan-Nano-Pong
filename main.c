#include "lcd/lcd.h"

void Inp_init(void)
{
	gpio_init(GPIOA, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, GPIO_PIN_8);
}

void Adc_init(void) 
{
    rcu_periph_clock_enable(RCU_GPIOA);
    gpio_init(GPIOA, GPIO_MODE_AIN, GPIO_OSPEED_50MHZ, GPIO_PIN_0|GPIO_PIN_1);
    RCU_CFG0|=(0b10<<14)|(1<<28);
    rcu_periph_clock_enable(RCU_ADC0);
    ADC_CTL1(ADC0)|=ADC_CTL1_ADCON;
}

void IO_init(void)
{
	Inp_init();	// inport init
	Adc_init();	// A/D init
	Lcd_Init();	// LCD init
}

FlagStatus Get_BOOT0SW(void)
{
	return(gpio_input_bit_get(GPIOA, GPIO_PIN_8));
}

uint16_t Get_adc(int ch)
{
    ADC_RSQ2(ADC0)=0;
    ADC_RSQ2(ADC0)=ch;
    ADC_CTL1(ADC0)|=ADC_CTL1_ADCON;
    while(!(ADC_STAT(ADC0)&ADC_STAT_EOC));
    uint16_t ret=ADC_RDATA(ADC0)&0xFFFF;
    ADC_STAT(ADC0)&=~ADC_STAT_EOC;
    return(ret);
}


#define Court_Xs    0
#define Court_Ys    0
#define Court_Xe  159
#define Court_Ye   79
#define	Court_X   (Court_Xe-Court_Xs+1)
#define	Court_Y   (Court_Ye-Court_Ys+1)
#define Court_Xl  (Court_X/2)
#define Court_lp    8

#define	adc_max 4096
#define dev_adc (adc_max/Court_Y)

#define Ball_X     3
#define Ball_Y     3

#define Paddle_X   3
#define Paddle_Y  15

#define Player     2
#define Play_L     0
#define Play_R     1

//#define Angle_max  7	//-3~+3
#define Angle_max 13	//-6~+6

#define Dir_L 0
#define Dir_R 1

#define Speed_min 10
#define Speed_max  1

#define Mode_max  3
#define Mode_demo 0
#define Mode_1P   1
#define Mode_2P   2

#define	Wait_tim 500

#define Score_max 99

u16 P_mode;

u16 X_ball=Court_X/2, Y_ball=Court_Y/2;
u16 X_paddle[Player]={Court_Xs+(Paddle_X-1)/2, Court_Xe-(Paddle_X-1)/2};
u16 Y_paddle[Player]={Court_Y/2, Court_Y/2};
u16 Angle;
u16 Dir;

//u16 R_count=0;
uint32_t S_time=Speed_min;

u16 S_count[Player]={0, 0};

u16 C_draw=WHITE, C_clear=BLACK;

u16 X_ball_bak=Court_X/2+1, Y_ball_bak=Court_Y/2+1;
u16 X_paddle_bak[Player]={Court_Xs+(Paddle_X-1)/2+1, Court_Xe-(Paddle_X-1)/2+1};
u16	Y_paddle_bak[Player]={Court_Y/2+1, Court_Y/2+1};


void Court_draw(void)
{
	int i;
	LCD_DrawLine(Court_Xs, Court_Ys, Court_Xe, Court_Ys, C_draw);
	LCD_DrawLine(Court_Xs, Court_Ye, Court_Xe, Court_Ye, C_draw);
	for(i=Court_Ys; i<Court_Y/Court_lp; i++){
		LCD_DrawLine(Court_Xl, Court_lp*i+1, Court_Xl, Court_lp*i+1+Court_lp/2, C_draw);
	}
}

void Ball_draw(void)
{
	int i;
	if(X_ball!=X_ball_bak
	|| Y_ball!=Y_ball_bak){
		LCD_Fill(X_ball_bak-(Ball_X-1)/2, Y_ball_bak-(Ball_Y-1)/2, X_ball_bak+(Ball_X-1)/2, Y_ball_bak+(Ball_Y-1)/2, C_clear);
		if(Y_ball_bak<=Court_Ys+(Ball_Y-1)/2){LCD_DrawLine(Court_Xs, Court_Ys, Court_Xe, Court_Ys, C_draw);}
		if(Y_ball_bak>=Court_Ye-(Ball_Y-1)/2){LCD_DrawLine(Court_Xs, Court_Ye, Court_Xe, Court_Ye, C_draw);}
		if(X_ball_bak>=Court_Xl-(Ball_X-1)/2&&X_ball_bak<=Court_Xl+(Ball_X-1)/2){
			for(i=Court_Ys; i<Court_Y/Court_lp; i++){
				LCD_DrawLine(Court_Xl, Court_lp*i+1, Court_Xl, Court_lp*i+1+Court_lp/2, C_draw);
			}
		}
		X_ball_bak=X_ball;
		Y_ball_bak=Y_ball;
	}
	LCD_Fill(X_ball-(Ball_X-1)/2, Y_ball-(Ball_Y-1)/2, X_ball+(Ball_X-1)/2, Y_ball+(Ball_Y-1)/2, C_draw);
}

void Paddle_draw(int s)
{
	if(X_paddle[s]!=X_paddle_bak[s]
	|| Y_paddle[s]!=Y_paddle_bak[s]){
		LCD_Fill(X_paddle_bak[s]-(Paddle_X-1)/2, Y_paddle_bak[s]-(Paddle_Y-1)/2, X_paddle_bak[s]+(Paddle_X-1)/2, Y_paddle_bak[s]+(Paddle_Y-1)/2, C_clear);
		X_paddle_bak[s]=X_paddle[s];
		Y_paddle_bak[s]=Y_paddle[s];
	}
	LCD_Fill(X_paddle[s]-(Paddle_X-1)/2, Y_paddle[s]-(Paddle_Y-1)/2, X_paddle[s]+(Paddle_X-1)/2, Y_paddle[s]+(Paddle_Y-1)/2, C_draw);
}

void Score_draw(u16 c)
{
	LCD_ShowNum(8* 7, 8*1, S_count[Play_R], 2, c);
	LCD_ShowNum(8*11, 8*1, S_count[Play_L], 2, c);
}


void Y_paddle_set(void)
{
	switch(P_mode){
		case Mode_demo : 
			Y_paddle[Play_L]=Court_Y/2;
			Y_paddle[Play_R]=Court_Y/2;
			break;
		case Mode_1P   :
			Y_paddle[Play_L]=Court_Y/2;
			Y_paddle[Play_R]=Get_adc(Play_R)/dev_adc;
			break;
		case Mode_2P   : 
			Y_paddle[Play_L]=Get_adc(Play_L)/dev_adc;
			Y_paddle[Play_R]=Get_adc(Play_R)/dev_adc;
			break;
		default :
			Y_paddle[Play_L]=Court_Y/2;
			Y_paddle[Play_R]=Court_Y/2;
			break;
	}
}

/*
void Speeed_set(int s)
{
	if(!s){
		R_count=0;
		S_time=Speed_min;
	}
	else{
		if(++R_count>10&&S_time>Speed_max){R_count=0;--S_time;}
	}
}
*/

void Start_set(void)
{
	Court_draw();
	Ball_draw();
	Paddle_draw(Play_L);
	Paddle_draw(Play_R);
	Score_draw(C_draw);
	delay_1ms(Wait_tim);
	Score_draw(C_clear);
	X_ball=Court_X/2;
	Y_ball=Court_Y/2;
	Y_paddle_set();
	Ball_draw();
	Paddle_draw(Play_L);
	Paddle_draw(Play_R);
	delay_1ms(Wait_tim);
//	Speed_set(0);
//	Angle=(rand()%(Angle_max-1)/2)+(Angle_max-1)/2;
	Angle=(Angle_max-1)/2;
	Dir=rand()%2;
}


void ball_add_y(u16 y)
{
	if(Y_ball+y> Court_Ye-(Ball_Y-1)/2){
		Y_ball=Court_Ye-(Ball_Y-1)/2;
		Angle=(Angle_max-1)-Angle;
	}
	else{
		Y_ball=Y_ball+y;
	}
}

void ball_sub_y(u16 y)
{
	if(Y_ball< y+Court_Ys+(Ball_Y-1)/2){
		Y_ball=Court_Ys+(Ball_Y-1)/2;
		Angle=(Angle_max-1)-Angle;
	}
	else{
		Y_ball=Y_ball-y;
	}
}

void ball_mov(void)
{
	if(Dir==Dir_R){	++X_ball;}
	else{			--X_ball;}
/*
	if((Angle> (Angle_max-1)/2 && Y_ball+Angle-(Angle_max-1)/2> Court_Ye-(Ball_Y-1)/2)
	|| (Angle< (Angle_max-1)/2 && Y_ball< (Angle_max-1)/2-Angle+Court_Ys+(Ball_Y-1)/2)){
		if(Y_ball< Court_Y/2){	Y_ball=Court_Ys+(Ball_Y-1)/2;}
		else{					Y_ball=Court_Ye-(Ball_Y-1)/2;}
		Angle=(Angle_max-1)-Angle;
	}
	else{
		Y_ball=Y_ball+Angle-(Angle_max-1)/2;
	}
*/
	switch(Angle){
		case 12 : // 3
			ball_add_y(3); break;
		case 11 : // 2
			ball_add_y(2); break;
		case 10 : // 1
			ball_add_y(1); break;
		case  9 : // 0.5
			if(!(X_ball%2)){ball_add_y(1);}	else{ball_add_y(0);} break;
		case  8 : // 0.33
			if(!(X_ball%3)){ball_add_y(1);}	else{ball_add_y(0);} break;
		case  7 : // 0.25
			if(!(X_ball%4)){ball_add_y(1);}	else{ball_add_y(0);} break;
		case  6 : // 0.0
			ball_add_y(0); break;
		case  5 : //-0.25
			if(!(X_ball%4)){ball_sub_y(1);}	else{ball_sub_y(0);} break;
		case  4 : //-0.33
			if(!(X_ball%3)){ball_sub_y(1);}	else{ball_sub_y(0);} break;
		case  3 : //-0.5
			if(!(X_ball%2)){ball_sub_y(1);}	else{ball_sub_y(0);} break;
		case  2 : //-1
			ball_sub_y(1); break;
		case  1 : //-2
			ball_sub_y(2); break;
		case  0 : //-3
			ball_sub_y(3); break;
	}
	Ball_draw();
}

void Paddle_demo(int s)
{
	if(Y_paddle[s]!=Y_ball){
		if(Y_paddle[s]< Y_ball){Y_paddle[s]=Y_paddle[s]+1;}
		else{					Y_paddle[s]=Y_paddle[s]-1;}
	}
	if(Y_paddle[s]> Court_Ye-(Paddle_Y-1)/2){	Y_paddle[s]=Court_Ye-(Paddle_Y-1)/2;	}
	if(Y_paddle[s]< Court_Ys+(Paddle_Y-1)/2){	Y_paddle[s]=Court_Ys+(Paddle_Y-1)/2;	}
}

void Paddle_play(int s)
{
	Y_paddle[s]=Get_adc(s)/dev_adc;
	if(Y_paddle[s]> Court_Ye-(Paddle_Y-1)/2){	Y_paddle[s]=Court_Ye-(Paddle_Y-1)/2;	}
	if(Y_paddle[s]< Court_Ys+(Paddle_Y-1)/2){	Y_paddle[s]=Court_Ys+(Paddle_Y-1)/2;	}
}

void Paddle_mov(void)
{
	switch(P_mode){
		case Mode_demo : Paddle_demo(Play_L); Paddle_demo(Play_R); break;
		case Mode_1P   : Paddle_demo(Play_L); Paddle_play(Play_R); break;
		case Mode_2P   : Paddle_play(Play_L); Paddle_play(Play_R); break;
		default: 		 Paddle_demo(Play_L); Paddle_demo(Play_R); break;
	}
	Paddle_draw(Play_L);
	Paddle_draw(Play_R);

}

int Paddle_hit(int s)
{
	if(Y_ball> Y_paddle[s]-(Paddle_Y-1)/2
	&& Y_ball< Y_paddle[s]+(Paddle_Y-1)/2){
		return(1);
	}
	else{
		return(0);
	}
}

void ball_hit(void)
{
	if(Dir==Dir_R){ // Dir right
		if(X_ball+(Ball_X-1)/2>=Court_Xe-(Paddle_X-1)/2){
			if(X_ball+(Ball_X-1)/2==Court_Xe){ // Miss
				if(S_count[Dir]< Score_max){++S_count[Dir];}
				Start_set();
//				Speed_set(0);
				Dir=Dir_L;
			}
			else{
				if(Paddle_hit(Dir)){ // Hit
//					Angle=(rand()%Angle_max);
					if(Y_ball==Y_paddle[Dir]){	if(rand()%4){	Angle=(rand()%5)+4;}
												else{		 	Angle=rand()%Angle_max;}
					}
					else{
					if(Y_ball> Y_paddle[Dir]){
						if(			Y_ball-Y_paddle[Dir]> 5){	Angle=rand()%Angle_max;	}
						else{	if(	Y_ball-Y_paddle[Dir]> 2){	Angle=(rand()%9)+2;		}
								else{							Angle=(rand()%5)+4;		}}}
					if(				Y_ball< Y_paddle[Dir]){
						if(			Y_paddle[Dir]-Y_ball> 5){	Angle=rand()%Angle_max;	}
						else{	if(	Y_paddle[Dir]-Y_ball> 2){	Angle=(rand()%9)+2;		}
								else{							Angle=(rand()%5)+4;		}}}
					}
//					Speed_set(1); 
					Dir=Dir_L;
				}
			}
		}
	}
	else{ // Dir left
		if(X_ball-(Ball_X-1)/2<=Court_Xs+(Paddle_X-1)/2){
			if(X_ball-(Ball_X-1)/2==Court_Xs){ // Miss
				if(S_count[Dir]< Score_max){++S_count[Dir];}	
				Start_set();	
//				Speed_set(0); 
				Dir=Dir_R;
			}
			else{
				if(Paddle_hit(Dir)){ // Hit
//					Angle=(rand()%Angle_max);
					if(Y_ball==Y_paddle[Dir]){if(rand()%4){	Angle=(rand()%5)+4;}
											  else{			Angle=rand()%Angle_max;}
					}
					else{
					if(Y_ball> Y_paddle[Dir]){
						if(		Y_ball-Y_paddle[Dir]> 5){	Angle=rand()%Angle_max;	}
						else{if(Y_ball-Y_paddle[Dir]> 2){	Angle=(rand()%9)+2;		}
							else{							Angle=(rand()%5)+4;		}}}
					if(			Y_ball< Y_paddle[Dir]){
						if(		Y_paddle[Dir]-Y_ball> 5){	Angle=rand()%Angle_max;	}
						else{if(Y_paddle[Dir]-Y_ball> 2){	Angle=(rand()%9)+2;		}
							else{							Angle=(rand()%5)+4;		}}}
					}
//					Speed_set(1);
					Dir=Dir_R;
				}
			}
		}
	}
}

void pm_draw(u16 c0, u16 c1, u16 c2)
{
	LCD_ShowString( 8* 1, 8*4, (u8*)"Demo ", c0);
	LCD_ShowString( 8* 7, 8*4, (u8*)"1Play", c1);
	LCD_ShowString( 8*13, 8*4, (u8*)"2Play", c2);
}

void playmode(void)
{
	LCD_ShowString( 8*0, 8*1, (u8*)"* LonganNano Pong *", WHITE);
	LCD_ShowString( 8*0, 8*8, (u8*)"    egarake.work   ", WHITE);
	do{
		P_mode=(Mode_max-1)-Get_adc(1)/(4096/Mode_max);
		if(P_mode>Mode_max-1){P_mode=Mode_max-1;}
		switch(P_mode){
			case Mode_demo :	pm_draw(GREEN,RED,RED);	break;
			case Mode_1P   :	pm_draw(RED,GREEN,RED);	break;
			case Mode_2P   :	pm_draw(RED,RED,GREEN);	break;
			default:			pm_draw(GREEN,RED,RED);	break;
		}
	}while(!Get_BOOT0SW());
}


/* Longan Nano Pong */
int main( void ) 
{
	IO_init();	// IO init

	BACK_COLOR=C_clear;
	LCD_Clear(C_clear);
	playmode();	// select mode

	LCD_Clear(C_clear);
	Start_set();	// start init
	while(1){
		ball_mov();			// Ball move
		Paddle_mov();		// Paddle move
		ball_hit();			// Ball hit check
		delay_1ms(S_time);	// Speed wait
	}
}
