#include "Function.h"
#include "wfEEPROM.h"
#include "Variables.h"
#include "..\..\..\WF_Device\wfDefine.h"
#include "..\..\..\WF_Device\Verify.h"

void GetROMState(void)
{
	uint8_t x;
	RomData_ReadBytes(0x9e,RomDatas,2);
	if(RomDatas[0]==0xff && RomDatas[1]==0xff)//尚未初始化
	{
		BAT_ON();
		RomDatas[0]=ROM_9E;
		RomDatas[1]=ROM_9F;
		RomData_WriteBytes(0x9e,RomDatas,2);//初始化ROM版本
	}
	RomData_ReadBytes(0x9e,RomDatas,2);
	x=RomDatas[0]+RomDatas[1];
	if(x==0)//校验成功
	{
		if(GetBit(RomDatas[0],2)!=0)//写过了
		{
			RomStateFlags.bRomWrited=1;
		}
	}
	RomStateFlags.bRomWrited=0;
}
//反转指定地址的数据，若为0xff则写为0，反之若不是0xff则写为0xff
void ReverseRom(uint8_t addr)
{
	if(RomData_ReadByte(addr)!=0xff)
	{
		RomData_WriteByte(addr,0xff);
	}
	else
	{
		RomData_WriteByte(addr,0);
	}
}
//获得69~70存储区中第一个不为0xff的单元，其数据为计算次数
//70-6f-6e-6d-6c-6b-6a-69-70
void GetCalcTimes(void)
{
	uint8_t i,x;
	RomData_WriteBytes(0x69,RomDatas,8);
	for(i=8;i!=0;i--)
	{
		x=i-1;
		if(RomDatas[x]!=0)
			break;
	}
	if(i==0)//没有非ff单元
	{
		CalcTimes=0;
		CalcTimesAddr=0x69;
		return;
	}
	else
	{
		//容错：再判断下一个是否也为非ff
		if(i==1)//无需再判断
		{
			CalcTimes=RomDatas[0];
			CalcTimesAddr=0x69;
			return;
		}
		else 
		{
			x=i-2;
			if(RomDatas[x]!=0xff)//出现了两个，使用下一个
			{
				CalcTimes=RomDatas[x];
				CalcTimesAddr=0x69+x;				
				ReverseRom(0x68+i);//修正rom
			}
			else//没错，则使用此值
			{
				x=i-1;
				CalcTimes=RomDatas[x];
				CalcTimesAddr=0x69+x;
			}
		}
		//rom容错:再判断前一个是否也为非ff,只有70单元时需要判断
		if(i==8)//70单元
		{
			if(RomDatas[0]!=0xff)
			{
				ReverseRom(0x69);
			}
		}
	}
}
//检查数据段校验
void CheckDataBlockVerify(uint8_t Addr)
{
	uint8_t x;
	gFlags.bFuncRet=0;
	if(RomData_ReadByte(Addr+8)>0x7f)
	{
		gFlags.bFuncRet=0;
		return;
	}
	RomData_ReadBytes(Addr,RomDatas,10);
	x=GetVerify_byteXOR(RomDatas,9);
	if(x==RomDatas[9])
		gFlags.bFuncRet=1;
}
//检查55级5f段数据，若均不成功则使用73地址数据，即初始hash值
void Check55and5fdata(void)
{
	uint8_t x,y;
	RomStateFlags.b55=0;
	RomStateFlags.b5f=0;
	GetCalcTimes();
	StepTimesAddr[2]=0x73;//备用数据，均不成功则使用73地址数据，即初始hash值
	CheckDataBlockVerify(0x55);
	if(gFlags.bFuncRet)
	{
		RomStateFlags.b55=1;
		x=RomData_ReadByte(0x55+8);
		y=CalcTimes&0xfc;
		if(x==y)
		{
			StepTimesAddr[2]=0x55;//使用55段
		}
	}
	CheckDataBlockVerify(0x5f);
	if(gFlags.bFuncRet)
	{
		RomStateFlags.b5f=1;
		x=RomData_ReadByte(0x5f+8);
		y=CalcTimes&0xfc;
		if(x==y)
		{
			StepTimesAddr[2]=0x5f;//使用5f段
		}
	}
}
void Check41and4bdata(void)
{
	uint8_t ee49,ee53;
	ee49=RomData_ReadByte(0x49);
	ee53=RomData_ReadByte(0x53);
	RomStateFlags.b41=0;
	RomStateFlags.b4b=0;
	CheckDataBlockVerify(0x41);
	if(gFlags.bFuncRet)
	{
		RomStateFlags.b41=1;
		if(ee49!=0)
		{
			CheckDataBlockVerify(0x4b);
			if(gFlags.bFuncRet)
			{
				RomStateFlags.b4b=1;
				if(ee53!=0)//两段均成功，且次数均不为0，则
				{
					if(ee53>=ee49)
					{
						StepTimesAddr[1]=0x4b;
					}
					else
					{
						StepTimesAddr[1]=0x41;
					}
				}
				else
				{
					if(ee49==0x1f)//00 1f 则使用00的
					{
						StepTimesAddr[1]=0x4b;
					}
					else
					{
						StepTimesAddr[1]=0x41;
					}
				}
			}
		}
		else
		{
			CheckDataBlockVerify(0x4b);
			if(gFlags.bFuncRet)
			{
				RomStateFlags.b4b=1;
				if(ee53==0x1f)//00 1f 则使用00的
				{
					StepTimesAddr[1]=0x41;
				}
				else
				{
					StepTimesAddr[1]=0x4b;
				}
			}
		}
	}
	else
	{
		CheckDataBlockVerify(0x4b);
		if(gFlags.bFuncRet)
		{
			RomStateFlags.b4b=1;
			StepTimesAddr[1]=0x4b;
		}
		else//两段都不匹配，容错
		{
			if(ee49!=0)
			{	
				if(ee49==0x1f && ee53==0)
				{
					StepTimesAddr[1]=0x49;
					return;
				}
				if(ee49<ee53)
				{
					StepTimesAddr[1]=0x41;
				}
				else
				{
					StepTimesAddr[1]=0x4b;
				} 
			}
			else
			{
				if(ee53==0x1f)
				{
					StepTimesAddr[1]=0x4b;
					return;
				}
				else
				{
					if(ee49<ee53)
					{
						StepTimesAddr[1]=0x41;
					}
					else
					{
						StepTimesAddr[1]=0x4b;
					} 
				}
			}
		}
	}	
}
void Check2Dand37data(void)
{
	uint8_t ee35,ee3f;
	ee35=RomData_ReadByte(0x35);
	ee3f=RomData_ReadByte(0x3f);
	RomStateFlags.b2d=0;
	RomStateFlags.b37=0;
	CheckDataBlockVerify(0x2d);
	if(gFlags.bFuncRet)
	{
		RomStateFlags.b2d=1;		
	}
	CheckDataBlockVerify(0x37);
	if(gFlags.bFuncRet)
	{
		RomStateFlags.b37=1;		
	}
	if(RomStateFlags.b2d || RomStateFlags.b37)//有符合存储规则的 
	{
		if(ee35>ee3f)//用大的
		{
			StepTimesAddr[0]=0x2d;
		}
		else
			StepTimesAddr[0]=0x37;
	}
	else
	{
		if(ee35<ee3f)//用小的
		{
			StepTimesAddr[0]=0x2d;
		}
		else
		{
			StepTimesAddr[0]=0x37;
		}
	}
}
