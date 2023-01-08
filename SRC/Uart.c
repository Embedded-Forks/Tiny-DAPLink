#pragma WARNINGLEVEL (1)
#include "CH552.H"
#include "Uart.H"
#include "Debug.H"

#define ENDP1_SIZE 64

UINT8 FIFO_Write_Pointer;
UINT8 FIFO_Read_Pointer;
UINT8 UART_RX_Data_Pointer;
BOOL EP1_TX_BUSY;
BOOL UART_TX_BUSY;

UINT8I UART_RX_Data_Buff[ENDP1_SIZE];
UINT8X UART_TX_Data_Buff[256]_at_ 0x0300;

void UART_Setup(void)
{
	FIFO_Write_Pointer=0;
	FIFO_Read_Pointer=0;	
	UART_RX_Data_Pointer=0;
	UART_TX_BUSY=FALSE;	
	EP1_TX_BUSY=FALSE;		
	
	P3_MOD_OC|=0x03;
	P3_DIR_PU|=0x03;
	
	//ʹ��Timer1��Ϊ�����ʷ�����	
	RCLK = 0; //UART0����ʱ��
	TCLK = 0; //UART0����ʱ��
	PCON |= SMOD;

	TMOD = TMOD & ~bT1_GATE & ~bT1_CT & ~MASK_T1_MOD | bT1_M1; //0X20��Timer1��Ϊ8λ�Զ����ض�ʱ��
	T2MOD = T2MOD | bTMR_CLK | bT1_CLK;                        //Timer1ʱ��ѡ��
	TH1 = 0 - (UINT32)(FREQ_SYS+UART_BUAD*8) / UART_BUAD / 16;//12MHz����,buad/12Ϊʵ�������ò�����
	TR1 = 1; 																	//������ʱ��1
	SCON = 0x50;//����0ʹ��ģʽ1    TI = 1;    REN = 1;       
	ES = 1;	
	
}


/*
//��ȡ��USB�������� ע�� �˺��������ܲ��ܰ��� ��ҪӲ��ͷƤ˵����
void UART_Get_USB_Data(uint8_t Nums)//ͨѶ�е��յ�������
{
	uint8_t i;
	uint8_t Buff_Last_nums;//����ʣ������
	Buff_Last_nums=FIFO_Read_Pointer-FIFO_Write_Pointer;		
	Buff_Last_nums--;
	if(Buff_Last_nums>=i)//ͨѶ�е��յ������� ��������㹻
	{
		for(i=0;i<Nums;i++)
		{
			UART_TX_Data_Buff[FIFO_Write_Pointer]=Ep1Buffer[i];
			FIFO_Write_Pointer++;
		}	
		if(UART_TX_BUSY==FALSE)//���ڿ���
		{
			SBUF1=UART_TX_Data_Buff[FIFO_Read_Pointer];//��ʼ�͵�һ��
			FIFO_Read_Pointer++;
			UART_TX_BUSY=TRUE;		
		}
		if(Buff_Last_nums>=64*2)
		{
			UEP1_CTRL = UEP1_CTRL & ~ MASK_UEP_R_RES | UEP_R_RES_ACK;
			return;
		}
	}
	UEP1_CTRL = UEP1_CTRL & ~ MASK_UEP_R_RES | UEP_R_RES_NAK;//�������� ��Ҫ��Ҫ	
}
*/
//R7 = ��������
void UART_Get_USB_Data(UINT8 Nums)
{
#pragma asm	
	//MOV   A,R0//��¼R0��ֻ �³��
	//MOV   R6,A

	MOV  	DPTR,#0040H

	CLR  	C
	MOV  	A,FIFO_Read_Pointer //���㻺��ʣ��
	SUBB 	A,FIFO_Write_Pointer
	//CLR  	C
	DEC  	A
	CLR  	C
	SUBB 	A,R7
	JC   	ERR_OUT//���治��
	//��������� C=0
	//��ʼ������
	//INC DPTR //���õ���Դ���ݿ�ʼλ��
	MOV	P2,#03H
	MOV R0,FIFO_Write_Pointer //дָ�����  p2�ڳ���ʼ�Ѿ��øߵ�ַ0x03 ��Ϊ�����Ѿ����� ����дָ����ǻ����ַ�ĵ͵�ַ
DATA_LOP:
	MOVX A,@DPTR //��ȡ����Դ XRAM 1T
	MOVX @R0,A //д�� XRAM 1T
	INC DPTR// 1T
	INC R0// 1T
	DJNZ R7,DATA_LOP//���ʣ��������Ϊ0 ������ 4-6T
	MOV FIFO_Write_Pointer,R0 //����ѭ�� ��д

	JB   	UART_TX_BUSY,NEXT_NAK //����æ ���� ���ڲ�æ ��ʼ�µķ���
	MOV 	R0,FIFO_Read_Pointer//�õ���ָ��
	MOVX 	A,@R0
#if USE_UART0 == 1	
	MOV  	SBUF,A //Uart0
#elif	USE_UART1 == 1	
	MOV  	SBUF1,A //Uart1
#else
	#Error "no UARTx interface define"
#endif		
	INC 	FIFO_Read_Pointer
	SETB 	UART_TX_BUSY

NEXT_NAK:
		//����64->�´λ����Ƿ�����
	CLR  	C
	MOV  	A,FIFO_Read_Pointer //���㻺��ʣ��
	SUBB 	A,FIFO_Write_Pointer
	DEC  	A
	CLR  	C
	SUBB 	A,#040H
	JC   	ERR_OUT
	ANL  	UEP1_CTRL,#0F3H //����USB״̬ΪASK 	
	RET
ERR_OUT:
	MOV  	A,UEP1_CTRL  //nak
	ANL  	A,#0F3H
	ORL  	A,#08H
	MOV  	UEP1_CTRL,A
	
	//MOV   A,R6
	//MOV   R0,A
	RET  
#pragma endasm	
}


//��UART���ͻ��濽����usb���ͻ��� ֻ���볤�ȼ���
void memcpy_TXBUFF_USBBUFF(UINT8 data_len)//R7����
{
#pragma asm		//��ԼѰַ���� �ӿ��ٶ�
	MOV  	R0,#LOW (UART_RX_Data_Buff)
	INC XBUS_AUX
	MOV DPTR,#0080H //DPTR1 ep1���ͻ���
	DEC XBUS_AUX
LOOP:
	MOV A,@R0
	INC R0
	DB 0A5H  //MOVX @DPTR1,A & INC DPTR1
	DJNZ R7,LOOP
	RET
#pragma endasm			
}


void UART_Send_USB_Data(void)
{
//	if(UART_RX_Data_Pointer)//��Ҫ����
//	{			
	memcpy_TXBUFF_USBBUFF(UART_RX_Data_Pointer);
	UEP1_T_LEN=UART_RX_Data_Pointer;
	UART_RX_Data_Pointer=0;
	UEP1_CTRL = UEP1_CTRL & ~ MASK_UEP_T_RES | UEP_T_RES_ACK;//ʹ�ܷ���
	EP1_TX_BUSY = TRUE;
//	}
}

void Uart0_ISR(void) interrupt INT_NO_UART0 using 1
{
    if (RI)  //�յ�����
    {
        UART_RX_Data_Buff[UART_RX_Data_Pointer] = SBUF;
        UART_RX_Data_Pointer++;                    //��ǰ������ʣ���ȡ�ֽ���
        if (UART_RX_Data_Pointer > ENDP1_SIZE - 1)
			UART_RX_Data_Pointer--;//д��ָ��           
        RI = 0;
		if(EP1_TX_BUSY==FALSE)
			UART_Send_USB_Data();
    }
	if(TI)//������� 
	{
		if(FIFO_Read_Pointer!=FIFO_Write_Pointer)//������� ��ʾ��Ҫ����
		{				
#pragma asm		//��ԼѰַ���� �ӿ��ٶ�
//				SBUF=UART_TX_Data_Buff[FIFO_Read_Pointer];
//				FIFO_Read_Pointer++;	
				MOV		DPH,#03H
				MOV 	DPL,FIFO_Read_Pointer
				MOVX 	A,@DPTR
				MOV  	SBUF,A
				INC  	FIFO_Read_Pointer
#pragma endasm	
		}
		else//�������� ���һ�ν�����ж� ���ô��ڿ���
		{
			UART_TX_BUSY=FALSE;
		}
#pragma asm		
//			if(((UINT8)FIFO_Read_Pointer-FIFO_Write_Pointer-1)>64)
//			{
//				UEP1_CTRL = UEP1_CTRL & ~ MASK_UEP_R_RES | UEP_R_RES_ACK;//����� Ҫ
//			}
			CLR  	C
			MOV  	A,FIFO_Read_Pointer
			SUBB 	A,FIFO_Write_Pointer
			DEC		A
			CLR  	C
			SUBB 	A,#040H
			JC		T_NAK
			ANL  	UEP1_CTRL,#0F3H //����USB״̬ΪASK 
			T_NAK:
#pragma endasm
		TI =0;	
	}		
}
