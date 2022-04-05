
#include "NanoLog.hpp"
#include <bits/stdc++.h>
#include <unistd.h>
#include <linux/types.h>

#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <fstream>

static __u64 rdtsc()
{
    __u32 lo, hi;
    __asm__ __volatile__("rdtsc"
                         : "=a"(lo), "=d"(hi));
    return (__u64)hi << 32 | lo;
}

static bool operator<(const timespec &lhs, const timespec &rhs)
{
    if (lhs.tv_sec == rhs.tv_sec)
        return lhs.tv_nsec < rhs.tv_nsec;
    else
        return lhs.tv_sec < rhs.tv_sec;
}
static timespec operator+(const timespec &lhs, const timespec &rhs)
{
    timespec sum = {lhs.tv_sec + rhs.tv_sec, lhs.tv_nsec + rhs.tv_nsec};
    if (sum.tv_nsec > 1e9)
    {
        sum.tv_nsec -= 1e9;
        sum.tv_sec += 1;
    }
    return sum;
}
static timespec operator-(const timespec &lhs, const timespec &rhs)
{
    timespec delta = {lhs.tv_sec - rhs.tv_sec, lhs.tv_nsec - rhs.tv_nsec};
    if (delta.tv_nsec < 0)
    {
        delta.tv_sec -= 1;
        delta.tv_nsec += 1e9;
    }
    return delta;
}
static timespec operator/(const timespec &lhs, const int &rhs)
{

    double timeSum = (lhs.tv_sec + 1e-9 * lhs.tv_nsec) / rhs;
    return {long(timeSum), long((timeSum - long(timeSum)) * 1e9)};
}

class PerfTool
{
private:
    timespec beginTime = {0, 0}, endTime = {0, 0};
    // Paramaters initialize
    const std::string describe;
    const bool bUseCPUClock;
    const int reportTimes, subReportTimes;
    int windowSize;

    // control output
    long timeScale;
    std::string timeMessage;

    // metrics initialize
    std::vector<timespec> partOfTimeList;
    std::list<std::vector<timespec>> rollingTimeList;
    int reportTimesCounter = 0, subReportTimesCounter = 0;
    timespec sum, maxDeltaTime, minDeltaTime;

    // log Description information
    // subReport: report online metrics information
    void logDescribeInfo(bool subReport = false)
    {
        time_t tm;
        time(&tm);
        char tmp[64];
        strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", localtime(&tm));
        LOG_INFO << '<' << std::string(tmp) << "> " << describe << (subReport ? " sub report " : " ") << "statistics";
    }

    // log Metrics information
    // metricName: the name of the metric
    // metricData: the data of the metric
    void logMetricInfo(const std::string metricName, const timespec &metricData)
    {
        LOG_INFO << metricName << ":" << metricData.tv_sec << "s" << metricData.tv_nsec / timeScale << timeMessage;
    }

    // log Online information
    void logOnlineInfo(void)
    {
        logDescribeInfo(true);
        logMetricInfo("Max", maxDeltaTime);
        logMetricInfo("Min", minDeltaTime);
        logMetricInfo("Mean", sum / subReportTimes);
    }

    // log All information
    void logInfo()
    {
        std::vector<timespec> allOfTimeList = getTimeList();
        // caculate mean
        timespec timeSum = {0, 0};
        // 去除最大值与最小值
        for (auto iter = allOfTimeList.begin(); iter != allOfTimeList.end(); ++iter)
        {
            timeSum = timeSum + *iter;
        }

        logMetricInfo("Max", allOfTimeList[allOfTimeList.size() - 1]);
        logMetricInfo("Min", allOfTimeList[0]);
        logMetricInfo("Mean", timeSum / allOfTimeList.size());
        logMetricInfo("95%", allOfTimeList[long(allOfTimeList.size() * 0.95)]);
        logMetricInfo("75%", allOfTimeList[long(allOfTimeList.size() * 0.75)]);
        logMetricInfo("50%", allOfTimeList[long(allOfTimeList.size() * 0.50)]);
        logMetricInfo("25%", allOfTimeList[long(allOfTimeList.size() * 0.25)]);
    }

    // init all Online Metrics
    void initOnlineMetrics(void)
    {
        sum = {0, 0}, maxDeltaTime = {0, 0}, minDeltaTime = {LONG_MAX, 0};
    }

    // update the Online Metrics and add the counter
    void updateOnlineMetrics(void)
    {
        const struct timespec deltaTime = endTime - beginTime;
        partOfTimeList[reportTimesCounter] = deltaTime;

        sum = sum + deltaTime;
        maxDeltaTime = maxDeltaTime < deltaTime ? deltaTime : maxDeltaTime;
        minDeltaTime = deltaTime < minDeltaTime ? deltaTime : minDeltaTime;

        if (++reportTimesCounter == reportTimes)
        {
            reportTimesCounter = 0;
        }
        if (++subReportTimesCounter == subReportTimes)
        {
            subReportTimesCounter = 0;
        }
    }

    // be used in begin() and end() to get the time by function clock_gettime()
    // selfTime: which paramater to store the time
    // time: use parameter time if passed
    void getTimeByFunction(timespec &selfTime, u_int64_t time = 0)
    {
        if (time == 0)
        {
            clock_gettime(CLOCK_REALTIME, &selfTime);
        }
        else
        {
            selfTime = {long(time), 0};
        }
    }

    // get all the time
    std::vector<timespec> getTimeList(void)

    {
        std::vector<timespec> allOfTimeList;
        for (std::vector<timespec> &partTL : rollingTimeList)
        {
            allOfTimeList.insert(allOfTimeList.end(), partTL.begin(), partTL.end());
        }
        std::sort(allOfTimeList.begin(), allOfTimeList.end());
        return allOfTimeList;
    }

public:
    // unit == 0: use ns; 1: use us; 2: use ms; 3: use second
    // bUse CPUClock == true: overwrite unit and use clock
    // report: report all metrics
    // subReport: report only online metrics
    // reportTimes: reportTimes is window and keep a window size of time data
    // rolkling: clear all time data if not rolling
    // rollingWindowWize: if use rolling, control the rolling wiodow size
    PerfTool(const char *describe,
             int reportTimes,
             int subReportTimes,
             int rollingWindowSize = 0,
             bool rolling = false,
             int unit = 0,
             bool bUseCPUClock = false)
        : reportTimes(reportTimes),
          subReportTimes(subReportTimes),
          bUseCPUClock(bUseCPUClock),
          describe(describe)
    {
        nanolog::initialize(nanolog::GuaranteedLogger(), std::string(get_current_dir_name()) + '/', "nanolog", 1);
        char timeMessageList[4][3] = {"ns", "us", "ms", "s"};
        timeScale = pow(1000, unit);
        timeMessage = timeMessageList[unit];
        windowSize = rolling ? ((rollingWindowSize - 1) / reportTimes + 1) : 1;
        partOfTimeList.resize(reportTimes);
        initOnlineMetrics();
    };

    // unit slave perf tool and append to master
    PerfTool(const char *describe,
             PerfTool *master)
        : reportTimes(master->reportTimes),
          subReportTimes(master->subReportTimes),
          timeScale(master->timeScale),
          timeMessage(master->timeMessage),
          bUseCPUClock(master->bUseCPUClock),
          windowSize(master->windowSize),
          describe(describe)
    {
        partOfTimeList.assign((master->partOfTimeList).begin(), (master->partOfTimeList).end());
        nanolog::initialize(nanolog::GuaranteedLogger(), std::string(get_current_dir_name()) + '/', "nanolog", 1);
        partOfTimeList.resize(reportTimes);
        initOnlineMetrics();
    };

    ~PerfTool()
    {
        std::vector<timespec>().swap(partOfTimeList);
    };

    // use parameter time if passed else get in function
    // need an extreme fast implementation
    void begin(uint64_t time = 0)
    {
        if (bUseCPUClock == false)
        {
            getTimeByFunction(beginTime, time);
        }
        else
        {
            __u64 timeFromRDTSC = rdtsc() / 2;
            beginTime = {long(timeFromRDTSC / int(1e9)), long(timeFromRDTSC % int(1e9))};
        }
    }
    void end(uint64_t time = 0)
    {
        if (bUseCPUClock == false)
        {
            getTimeByFunction(endTime, time);
        }
        else
        {
            __u64 timeFromRDTSC = rdtsc() / 2;
            endTime = {long(timeFromRDTSC / int(1e9)), long(timeFromRDTSC % int(1e9))};
        }
    };

    // report was actuallly perform when the call times reaches (subReportTimes or reportTimes)
    // bForce: caland report immediately
    void report(bool bForce = false)
    {
        updateOnlineMetrics();
        if (bForce == true || reportTimesCounter == 0)
        {
            rollingTimeList.push_back(partOfTimeList);
            if (rollingTimeList.size() > windowSize)
            {
                rollingTimeList.pop_front();
            }
            logInfo();
        }
        if (subReportTimesCounter == 0)
        {
            if (reportTimesCounter != 0)
            {
                logOnlineInfo();
            }
            initOnlineMetrics();
        }
    };

    // print with json format
    // print with csv format
    void analysisReport(bool bForce = false)
    {
        updateOnlineMetrics();
        if (bForce == true || reportTimesCounter == 0)
        {
            std::ofstream file("perfTool.json");
            cereal::JSONOutputArchive archive(file);

            std::vector<timespec> allOfTimeList = getTimeList();

            archive(cereal::make_nvp("Max", allOfTimeList[allOfTimeList.size() - 1].tv_sec),
                    cereal::make_nvp("Min", allOfTimeList[0].tv_sec),
                    cereal::make_nvp("95%", allOfTimeList[long(allOfTimeList.size() * 0.95)].tv_sec),
                    cereal::make_nvp("75%", allOfTimeList[long(allOfTimeList.size() * 0.75)].tv_sec),
                    cereal::make_nvp("50%", allOfTimeList[long(allOfTimeList.size() * 0.50)].tv_sec),
                    cereal::make_nvp("25%", allOfTimeList[long(allOfTimeList.size() * 0.25)].tv_sec));
            file.close();
        }
    };
};

int main(void)
{
    PerfTool ttt = PerfTool("test", 10, 5, 30, true, 0, false);
    for (int i = 0; i < 100; ++i)
    {
        ttt.begin();
        for (int j = 0; j < 1000; ++j)
            ;
        ttt.end();
        ttt.report();
    }
    ttt.analysisReport(true);
    return 0;
}
