
#ifndef __CONTROL_H
#define __CONTROL_H

/**
 * @brief  业务数据上报占位接口
 * @param  laber: 数据标签（预留参数）
 * @param  flag: 数据值（预留参数）
 * @note   当前实现为空操作，仅用于兼容原有调用链。
 */
void send_data(const char *laber, char *flag);

#endif
