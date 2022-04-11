
#include "NanoLog.hpp"
#include <bits/stdc++.h>
#include <unistd.h>
#include <linux/types.h>

#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <iostream>
#include <fstream>

const int RDTSC_PARAMETER = 2 * 1e9;
const char TIME_MESSAGE_LIST[4][3] = {"ns", "us", "ms", "s"};

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
    // paramaters initialize
    const bool bUseCPUClock;
    const int reportTimes, subReportTimes;
    int windowSize;

    // control output
    const std::string describe, timeMessage;
    const long timeScale;

    // metrics initialize
    std::vector<timespec> partOfTimeList;
    std::list<std::vector<timespec>> rollingTimeList;
    int reportTimesCounter = 0, subReportTimesCounter = 0;
    timespec sum, maxDeltaTime, minDeltaTime;

    // log description information
    // subReport: report online metrics information
    void logDescribeInfo(const bool subReport = false)
    {
        time_t tm;
        time(&tm);
        char tmp[64];
        strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", localtime(&tm));
        LOG_INFO << '<' << tmp << "> " << describe << (subReport ? " sub report " : " ") << "statistics";
    }

    // log metrics information
    // metricName: the name of the metric
    // metricData: the data of the metric
    void logMetricInfo(const std::string &metricName, const timespec &metricData)
    {
        LOG_INFO << metricName << ":" << metricData.tv_sec << "s" << metricData.tv_nsec / timeScale << timeMessage;
    }

    // log online information
    void logOnlineInfo(void)
    {
        logDescribeInfo(true);
        logMetricInfo("Max", maxDeltaTime);
        logMetricInfo("Min", minDeltaTime);
        logMetricInfo("Mean", sum / subReportTimes);
    }

    // log all information
    void logInfo(void)
    {
        std::vector<timespec> allOfTimeList = getTimeList();
        // caculate mean
        timespec timeSum = {0, 0};
        // 去除最大值与最小值
        for (auto iter = allOfTimeList.begin() + 1; iter != allOfTimeList.end() - 1; ++iter)
        {
            timeSum = timeSum + *iter;
        }

        logDescribeInfo(false);
        logMetricInfo("Max", allOfTimeList[allOfTimeList.size() - 1]);
        logMetricInfo("Min", allOfTimeList[0]);
        logMetricInfo("Mean", timeSum / allOfTimeList.size());
        logMetricInfo("95%", allOfTimeList[long(allOfTimeList.size() * 0.95)]);
        logMetricInfo("75%", allOfTimeList[long(allOfTimeList.size() * 0.75)]);
        logMetricInfo("50%", allOfTimeList[long(allOfTimeList.size() * 0.50)]);
        logMetricInfo("25%", allOfTimeList[long(allOfTimeList.size() * 0.25)]);
    }

    void logAnalysisInfo(void)
    {
        std::ofstream file(describe + ".json", std::ios::app);  
        cereal::JSONOutputArchive archive(file);

        std::vector<timespec> allOfTimeList = getTimeList();

        archive(cereal::make_nvp("Max", allOfTimeList[allOfTimeList.size() - 1].tv_nsec),
                cereal::make_nvp("Min", allOfTimeList[0].tv_nsec),
                cereal::make_nvp("95%", allOfTimeList[long(allOfTimeList.size() * 0.95)].tv_nsec),
                cereal::make_nvp("75%", allOfTimeList[long(allOfTimeList.size() * 0.75)].tv_nsec),
                cereal::make_nvp("50%", allOfTimeList[long(allOfTimeList.size() * 0.50)].tv_nsec),
                cereal::make_nvp("25%", allOfTimeList[long(allOfTimeList.size() * 0.25)].tv_nsec));
    }
    
    // init all online Metrics
    void initOnlineMetrics(void)
    {
        sum = {0, 0}, maxDeltaTime = {0, 0}, minDeltaTime = {LONG_MAX, 0};
    }

    // update the online metrics and add the counter
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

    // update all metrics
    void updateMetrics(void)
    {
        rollingTimeList.push_back(partOfTimeList);
        if (rollingTimeList.size() > windowSize)
        {
            rollingTimeList.pop_front();
        }
    }

    // be used in begin() and end() to get the time by function clock_gettime()
    // selfTime: which paramater to store the time
    void getTimeByFunction(timespec &selfTime)
    {
        clock_gettime(CLOCK_REALTIME, &selfTime);
    }

    // be used in begin() and end() to get the time by function rdtsc()
    // selfTime: which paramater to store the time
    void getTimeByRDTSC(timespec &selfTime)
    {
        __u64 timeFromRDTSC = rdtsc();
        selfTime = {long(timeFromRDTSC / RDTSC_PARAMETER), long(timeFromRDTSC % RDTSC_PARAMETER)};
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
          describe(describe),
          timeMessage(TIME_MESSAGE_LIST[unit]),
          timeScale(pow(1000, unit))
    {
        nanolog::initialize(nanolog::GuaranteedLogger(), std::string(get_current_dir_name()) + '/', describe, 1);
        windowSize = rolling ? ((rollingWindowSize - 1) / reportTimes + 1) : 1;
        partOfTimeList.resize(reportTimes);
        initOnlineMetrics();
    };

    // unit slave perf tool and append to master
    PerfTool(const char *describe,
             PerfTool *master)
        : reportTimes(master->reportTimes),
          subReportTimes(master->subReportTimes),
          bUseCPUClock(master->bUseCPUClock),
          windowSize(master->windowSize),
          describe(describe),
          timeScale(master->timeScale),
          timeMessage(master->timeMessage)
    {
        partOfTimeList.assign((master->partOfTimeList).begin(), (master->partOfTimeList).end());
        nanolog::initialize(nanolog::GuaranteedLogger(), std::string(get_current_dir_name()) + '/', describe, 1);
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
        if (time != 0)
        {
            beginTime = {long(time), 0};
        }
        else if (bUseCPUClock == false)
        {
            getTimeByFunction(beginTime);
        }
        else
        {
            getTimeByRDTSC(beginTime);
        }
    }

    void end(uint64_t time = 0)
    {
        if (time != 0)
        {
            endTime = {long(time), 0};
        }
        else if (bUseCPUClock == false)
        {
            getTimeByFunction(endTime);
        }
        else
        {
            getTimeByRDTSC(endTime);
        }
    };

    // report was actuallly perform when the call times reaches (subReportTimes or reportTimes)
    // bForce: calculate and report immediately
    void report(bool bForce = false)
    {
        updateOnlineMetrics();
        if (bForce == true || reportTimesCounter == 0)
        {
            updateMetrics();
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
            updateMetrics();
            logAnalysisInfo();
        }
    };
};