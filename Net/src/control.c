#include "stm32f10x.h"
#include "control.h"

/*---------------------------------------------------------------*/
/*函数名：send_data(const char *laber, char *flag)               */
/*功  能：业务上报占位接口（当前版本不发送业务数据）             */
/*说  明：为兼容原有调用链保留该函数，当前实现为空操作(no-op)    */
/*参  数：1.const char *laber : 预留的数据标签                    */
/*        2.char *flag : 预留的数据值                            */
/*返回值：无                                                      */
/*---------------------------------------------------------------*/
void send_data(const char *laber, char *flag)
{	
	(void)laber;
	(void)flag;
}

