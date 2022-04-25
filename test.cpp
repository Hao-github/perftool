#include "perfTool.cpp"
int main(void)
{
    PerfTool reportTest = PerfTool("the report test", 30, 10, 90, true, 1, false);
        PerfTool slaveTest = PerfTool("the slave test", &reportTest);
    for (int i = 0; i < 180; ++i)
    {
        reportTest.begin();
        slaveTest.begin();
        for (int j = 0; j < 1000; ++j)
         ;
        reportTest.end();
        slaveTest.end();
        reportTest.report();
        slaveTest.report();
    }
    reportTest.report(true);
    slaveTest.report(true);


    PerfTool CPUCLOCKTest = PerfTool("the cpu clock test",  30, 10 , 80, true, 0, true);
        for (int i = 0; i < 180; ++i)
    {
        CPUCLOCKTest.begin();
        for (int j = 0; j < 1000; ++j)
         ;
        CPUCLOCKTest.end();
        CPUCLOCKTest.report();
    }
    CPUCLOCKTest.report(true);
    
    // 
    return 0;
}