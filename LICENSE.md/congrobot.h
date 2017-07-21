/** \file CONGROBOT.h
 *
 *	\brief congrobot.h,　common header file for basic type define.
 *
 */

#ifndef _CONGROBOT_H_
#define _CONGROBOT_H_

/**	\def IN
 *
 *	\brief	IN，用于描述函数形参的数据为：进入函数
 */
#define IN

/**	\def OUT
 *
 *	\brief	IN，用于描述函数形参的数据为：从函数出
 */
#define OUT

/** 
 *  数据类型定义
 *
 *  定义新的数据类型，在项目中，尽可能使用这些数据类型
 *
 */

/**	\typedef UINT64
 *
 *	\brief 定义64bit的数据。ar should begin with u64, e.g UINT64 u64Len
 */
typedef unsigned long long 	UINT64;

/** \typedef unsigned long UINT32 
 *	\brief var should begin with u32, e.g UINT32 u32Len
 */

typedef unsigned long 	UINT32;		

/** \typedef long INT32 
 *	\brief var should begin with i32, e.g INT32 i32Len
 */
typedef long			INT32;

/** \typedef unsigned long UINT16 
 *	\brief var should begin with u16, e.g UINT32 u16Len
 */
typedef unsigned short	UINT16;
/** \typedef long INT16 
 *	\brief var should begin with i32, e.g INT16 i16Len
 */
typedef short			INT16;

/** \typedef unsigned long UINT8 
 *	\brief var should begin with u16, e.g UINT8 u8Len
 */
typedef unsigned char	UINT8;
/** \typedef long INT8 
 *	\brief var should begin with i8, e.g INT8 i8Len
 */
//typedef char			INT8;

/** \typedef UCHAR
 * 	\brief var should begin with uc. e.g UCHAR ucLen
 */
typedef unsigned char 	UCHAR;

/** \typedef CHAR
 * 	\brief var should begin with c. e.g UCHAR cTmp
 */
typedef char			CHAR;

/**	\typedef STRING
 * 	\brief var should begin with sz. e.g STRING szFineName
 */
typedef char*			STRING;
#endif
