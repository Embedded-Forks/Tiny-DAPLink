/**
 * @author Chi Zhang
 * @date 2023/2/7
 */

#include "main.h"
#include "Debug.H"
#include "DAP.h"
#include "Uart.h"
#include "Timer.H"
#include "AT.h"
#include "TouchKey.H"
#include "Keyboard.h"
#include "DataFlash.H"
#include "Usb.h"

typedef void( *goISP)( void );
goISP ISP_ADDR=0x3800;

UINT8 LED_Timer;

void main(void)
{
	CfgFsys();   //CH559ʱ��ѡ������
	mDelaymS(5); //�޸���Ƶ�ȴ��ڲ������ȶ�,�ؼ�

	USBDeviceInit(); //USB�豸ģʽ��ʼ��
	UART_Setup();

	//Timer2_Init();
	TK_Init();
	ReadDataFlash(0,1,&TargetKey);
	memset(Ep4Buffer,0,8);

	SAFE_MOD = 0x55;
	SAFE_MOD = 0xAA;
	WAKE_CTRL = bWAK_BY_USB | bWAK_RXD0_LO;	//USB or RXD0 can wake it up

	DAP_LED_BUSY = 0;
	LED_Timer = 0;

	EA = 1;          //����Ƭ���ж�

	while (!UsbConfig)
		;

	while (1)
	{
		DAP_Thread();

		if (Endp3Busy != 1 && Ep3Ii != Ep3Io){
			Endp3Busy = 1;
			UEP3_T_LEN = Ep3Is[0];//Ep3Io>>6];
			UEP3_DMA_L = Ep3Io;

			UEP3_CTRL = UEP3_CTRL & ~MASK_UEP_T_RES | UEP_T_RES_ACK; //������ʱ�ϴ����ݲ�Ӧ��ACK
			Ep3Io += 64;
		}


		if(DAP_LED_BUSY)
		{
			LED = 0;
		}
		else
		{
			LED_Timer++;
			if(LED_Timer==0x09)
			{
				LED = 1;
			}else if(LED_Timer==0xFF)
			{
				LED_Timer = 0;
				LED = 0;
			}
			TK_Measure();
			if (TK_FlashFreeFlag){
				TK_FlashKeyBuf();
				TK_FlashFreeFlag = 0;
			}
		}

		LED2 = !(XBUS_AUX & (bUART0_TX | bUART0_RX));

		if(TO_IAP) {
			EA = 0;
			USB_CTRL = 0;
			UDEV_CTRL = 0x80;
			mDelaymS(100);
			(ISP_ADDR)();
		}
	}
}
