#include <stdio.h>
 
//说明：RDTSC (就是ReaD TimeStamp Count) 其精度可以达到ns(纳秒)级别。(准确地说，其精度是1/F，F为你的CPU的时钟频率，这也是极限精度了)
 
//备注：RDTSC指令的机器码为 0x0F 0x31
 
inline __int64_t RDTSC()
{
    __int64_t TimeStamp;
 
    unsigned long highDword;
    unsigned long lowDword;
 
    __asm
    {
        //第一种写法
        rdtsc;
 
        //第二种写法（直接嵌入机器码）
        /*
        _emit 0x0F;
        _emit 0x31;
        */
 
        mov highDword,EDX;
        mov lowDword, EAX;
    }
 
    TimeStamp = highDword;
 
    TimeStamp <<= 32;
    TimeStamp |= lowDword;
 
    return TimeStamp;
}
 
int main(void)
{
    __int64_t t1,t2;
 
    t1 = RDTSC();
 
    //空循环，用于延时
    for ( volatile int n = 0; n < 1; ++n )
    {
    }
 
    t2 = RDTSC();
 
    printf("[%I64u]",(t2-t1));
 
    getchar();
    return 0;
}